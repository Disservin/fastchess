#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool
{
  public:
    ThreadPool(size_t threads) : stop(false)
    {
        for (size_t i = 0; i < threads; ++i)
            // Each worker thread runs an infinite loop that waits for tasks to be added to the queue
            workers.emplace_back([this] {
                while (!this->stop)
                {
                    std::function<void()> task;

                    // Acquire a lock on the queue mutex and wait for a task to be added to the queue
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            });
    }

    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args) -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        // Create a packaged task that wraps the specified function and arguments
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        // Get a future that will eventually hold the result of the task
        std::future<return_type> res = task->get_future();
        {
            // Acquire a lock on the queue mutex and add the task to the queue
            std::unique_lock<std::mutex> lock(queue_mutex);

            // don't allow enqueueing after stopping the pool
            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");

            // Move the task into the queue as a function object

            tasks.emplace([task]() { (*task)(); });
        }
        // Notify one worker thread that a new task is available
        condition.notify_one();
        return res;
    }

    void resize(size_t num_threads)
    {
        if (num_threads == 0)
        {
            throw std::invalid_argument("ThreadPool::resize() - num_threads cannot be 0");
        }

        size_t curr_threads = workers.size();
        if (num_threads > curr_threads)
        {
            // Add new threads
            for (size_t i = 0; i < num_threads - curr_threads; ++i)
            {
                workers.emplace_back([this] {
                    while (true)
                    {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            condition.wait(lock, [this] { return stop || !tasks.empty(); });
                            if (stop && tasks.empty())
                                return;
                            task = std::move(tasks.front());
                            tasks.pop();
                        }

                        task();
                    }
                });
            }
        }
        else if (num_threads < curr_threads)
        {
            // Signal workers to exit and join them
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }
            condition.notify_all();
            for (auto &worker : workers)
                worker.join();
            workers.resize(num_threads);
            stop = false;
            for (size_t i = curr_threads; i < num_threads; ++i)
            {
                workers.emplace_back([this] {
                    while (true)
                    {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        }
                    }
                });
            }
        }
    }

    void kill()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            std::queue<std::function<void()>> empty;
            std::swap(tasks, empty);
            stop = true;
        }
        condition.notify_all();
        for (auto &worker : workers)
        {
            if (worker.joinable())
            {
                /*is using detach or join here better?
                this gets called when ctrl c happens
                so in order to quickly cleanup i guess detach?
                */
                worker.detach();
            }
        }
        workers.clear();
    }

    ~ThreadPool()
    {
        kill();
    }

    std::atomic_bool stop = false;

  private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
};
