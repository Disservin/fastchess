//! Thread pool for concurrent task execution.
//!
//! Ports `core/threading/threadpool.hpp` from C++.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::thread;

type Task = Box<dyn FnOnce() + Send + 'static>;

/// A cloneable handle for enqueuing tasks onto a `ThreadPool`.
///
/// This allows closures running on worker threads to enqueue follow-up
/// tasks without needing a reference to the `ThreadPool` itself.
#[derive(Clone)]
pub struct TaskSender {
    shared: Arc<Shared>,
}

impl TaskSender {
    /// Enqueue a task for execution by a worker thread.
    ///
    /// If the pool has been stopped, the task is silently dropped.
    pub fn enqueue<F>(&self, task: F)
    where
        F: FnOnce() + Send + 'static,
    {
        if self.shared.stop.load(Ordering::Relaxed) {
            return;
        }

        {
            let mut queue = self.shared.queue.lock().unwrap();
            queue.push(Box::new(task));
        }
        self.shared.condvar.notify_one();
    }
}

/// A thread pool that executes tasks on a fixed set of worker threads.
///
/// Tasks are queued and executed in FIFO order. The pool can be resized
/// or killed. Dropping the pool joins all worker threads.
pub struct ThreadPool {
    shared: Arc<Shared>,
    workers: Vec<thread::JoinHandle<()>>,
}

struct Shared {
    queue: Mutex<Vec<Task>>,
    condvar: Condvar,
    stop: AtomicBool,
}

impl ThreadPool {
    /// Create a new thread pool with `num_threads` worker threads.
    pub fn new(num_threads: usize) -> Self {
        assert!(num_threads > 0, "ThreadPool requires at least 1 thread");

        let shared = Arc::new(Shared {
            queue: Mutex::new(Vec::new()),
            condvar: Condvar::new(),
            stop: AtomicBool::new(false),
        });

        let mut workers = Vec::with_capacity(num_threads);
        for _ in 0..num_threads {
            let shared = Arc::clone(&shared);
            workers.push(thread::spawn(move || work_loop(&shared)));
        }

        Self { shared, workers }
    }

    /// Enqueue a task for execution by a worker thread.
    ///
    /// Panics if the pool has been killed.
    pub fn enqueue<F>(&self, task: F)
    where
        F: FnOnce() + Send + 'static,
    {
        if self.shared.stop.load(Ordering::Relaxed) {
            panic!("Cannot enqueue on a stopped ThreadPool");
        }

        {
            let mut queue = self.shared.queue.lock().unwrap();
            queue.push(Box::new(task));
        }
        self.shared.condvar.notify_one();
    }

    /// Resize the pool to `num_threads` workers.
    ///
    /// This kills all existing workers and creates new ones.
    pub fn resize(&mut self, num_threads: usize) {
        assert!(num_threads > 0, "ThreadPool requires at least 1 thread");

        if num_threads == self.workers.len() {
            return;
        }

        self.kill();

        self.shared.stop.store(false, Ordering::SeqCst);

        let mut workers = Vec::with_capacity(num_threads);
        for _ in 0..num_threads {
            let shared = Arc::clone(&self.shared);
            workers.push(thread::spawn(move || work_loop(&shared)));
        }
        self.workers = workers;
    }

    /// Stop all workers and clear the task queue.
    pub fn kill(&mut self) {
        if self.shared.stop.load(Ordering::Relaxed) {
            return;
        }

        {
            let mut queue = self.shared.queue.lock().unwrap();
            queue.clear();
            self.shared.stop.store(true, Ordering::SeqCst);
            self.shared.condvar.notify_all();
        }

        for worker in self.workers.drain(..) {
            let _ = worker.join();
        }
    }

    /// Number of queued tasks.
    pub fn queue_size(&self) -> usize {
        self.shared.queue.lock().unwrap().len()
    }

    /// Whether the pool has been stopped.
    pub fn is_stopped(&self) -> bool {
        self.shared.stop.load(Ordering::Relaxed)
    }

    /// Number of worker threads.
    pub fn num_threads(&self) -> usize {
        self.workers.len()
    }

    /// Get a cloneable sender handle for enqueuing tasks.
    ///
    /// The returned `TaskSender` can be moved into closures and cloned
    /// freely, allowing worker tasks to enqueue follow-up work.
    pub fn sender(&self) -> TaskSender {
        TaskSender {
            shared: Arc::clone(&self.shared),
        }
    }
}

impl Drop for ThreadPool {
    fn drop(&mut self) {
        self.kill();
    }
}

fn work_loop(shared: &Shared) {
    loop {
        let task: Task;

        {
            let mut queue = shared.queue.lock().unwrap();
            while queue.is_empty() && !shared.stop.load(Ordering::Relaxed) {
                queue = shared.condvar.wait(queue).unwrap();
            }
            if shared.stop.load(Ordering::Relaxed) && queue.is_empty() {
                return;
            }
            task = queue.remove(0);
        }

        task();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::AtomicUsize;
    use std::sync::Arc;

    #[test]
    fn test_basic_execution() {
        let pool = ThreadPool::new(2);
        let counter = Arc::new(AtomicUsize::new(0));

        for _ in 0..10 {
            let counter = Arc::clone(&counter);
            pool.enqueue(move || {
                counter.fetch_add(1, Ordering::SeqCst);
            });
        }

        // Give tasks time to complete
        thread::sleep(std::time::Duration::from_millis(100));
        assert_eq!(counter.load(Ordering::SeqCst), 10);
    }

    #[test]
    fn test_resize() {
        let mut pool = ThreadPool::new(1);
        assert_eq!(pool.num_threads(), 1);

        pool.resize(4);
        assert_eq!(pool.num_threads(), 4);

        let counter = Arc::new(AtomicUsize::new(0));
        for _ in 0..20 {
            let counter = Arc::clone(&counter);
            pool.enqueue(move || {
                counter.fetch_add(1, Ordering::SeqCst);
            });
        }

        thread::sleep(std::time::Duration::from_millis(100));
        assert_eq!(counter.load(Ordering::SeqCst), 20);
    }

    #[test]
    fn test_kill() {
        let mut pool = ThreadPool::new(2);
        pool.kill();
        assert!(pool.is_stopped());
        assert_eq!(pool.num_threads(), 0);
    }

    #[test]
    fn test_task_sender_reenqueue() {
        // Verify that a task can enqueue follow-up tasks via TaskSender,
        // simulating the tournament runner's re-enqueue pattern.
        let pool = ThreadPool::new(2);
        let sender = pool.sender();
        let counter = Arc::new(AtomicUsize::new(0));

        // Enqueue 5 tasks that each chain the next one via TaskSender
        let remaining = Arc::new(AtomicUsize::new(5));
        {
            let counter = Arc::clone(&counter);
            let remaining = Arc::clone(&remaining);
            let sender = sender.clone();
            pool.enqueue(move || {
                fn chain(
                    counter: Arc<AtomicUsize>,
                    remaining: Arc<AtomicUsize>,
                    sender: TaskSender,
                ) {
                    counter.fetch_add(1, Ordering::SeqCst);
                    let left = remaining.fetch_sub(1, Ordering::SeqCst);
                    if left > 1 {
                        let counter = counter.clone();
                        let remaining = remaining.clone();
                        let sender2 = sender.clone();
                        sender.enqueue(move || chain(counter, remaining, sender2));
                    }
                }
                chain(counter, remaining, sender);
            });
        }

        thread::sleep(std::time::Duration::from_millis(200));
        assert_eq!(counter.load(Ordering::SeqCst), 5);
    }

    #[test]
    fn test_task_sender_after_stop() {
        // TaskSender should silently drop tasks after pool is stopped.
        let mut pool = ThreadPool::new(1);
        let sender = pool.sender();
        pool.kill();

        // This should not panic
        sender.enqueue(|| {
            panic!("This should never run");
        });
    }
}
