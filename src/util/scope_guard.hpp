#pragma once

template <typename T>
class ScopeGuard {
   public:
    explicit ScopeGuard(T &entry) : entry_(entry) {}

    ~ScopeGuard() { entry_.release(); }

    ScopeGuard(const ScopeGuard &)            = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    [[nodiscard]] auto &get() noexcept { return entry_; }
    [[nodiscard]] auto &get() const noexcept { return entry_; }

   private:
    T &entry_;
};
