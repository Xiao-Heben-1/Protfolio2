#ifndef RENDER_THREAD_POOL_HPP
#define RENDER_THREAD_POOL_HPP

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace render {
class thread_pool {
public:
    explicit thread_pool(int nThreads = static_cast<int>(std::thread::hardware_concurrency())) {
        // hardware_concurrency() is permitted to return zero.
        const size_t count = static_cast<size_t>(std::max(1, nThreads));
        workers.reserve(count);
        try {
            for (size_t i = 0; i < count; ++i) {
                workers.emplace_back([this] {
                    current_pool = this;
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(queueMutex);
                            condition.wait(lock, [this] { return stop || !tasks.empty(); });
                            if (stop && tasks.empty()) return;
                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                        task(); // packaged_task transfers exceptions to its future
                    }
                });
            }
        } catch (...) {
            // A partially constructed vector of joinable threads would terminate.
            shutdown();
            throw;
        }
    }

    // Drain accepted work before joining. Destroy from the owning thread only,
    // after callers have stopped submitting work (never from this pool's worker).
    ~thread_pool() { shutdown(); }
    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;

    size_t size() const noexcept { return workers.size(); }
    bool is_worker_thread() const noexcept { return current_pool == this; }

    // Arguments are owned by the task, like std::thread; use std::ref for references.
    // std::bind cannot forward stored move-only arguments to a by-value parameter.
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
        using result_type = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
        auto taskPtr = std::make_shared<std::packaged_task<result_type()>>(
            [function = std::forward<F>(f), arguments = std::make_tuple(std::forward<Args>(args)...)]() mutable
                -> result_type {
                return std::apply(std::move(function), std::move(arguments));
            });
        auto future = taskPtr->get_future();
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (stop) throw std::runtime_error("Enqueue on stopped thread_pool");
            tasks.emplace([taskPtr] { (*taskPtr)(); });
        }
        condition.notify_one();
        return future;
    }

private:
    void shutdown() noexcept {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& worker : workers) if (worker.joinable()) worker.join();
    }

    inline static thread_local const thread_pool* current_pool = nullptr;
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;
};

// Header-only definition: one shared pool across translation units.
inline thread_pool gThreadPool;

// chunkSize controls task granularity. At most pool.size() balanced ranges are
// submitted, rather than allocating a future for every single item.
// The callback can run concurrently; shared writes must be synchronized by it.
template<typename Func>
void parallelfor(thread_pool& pool, Func func, size_t nTasks, size_t chunkSize = 1) {
    if (chunkSize == 0) throw std::invalid_argument("parallelfor: chunkSize must be positive");
    if (nTasks == 0) return;
    const size_t chunks = nTasks / chunkSize + (nTasks % chunkSize != 0);
    const size_t task_count = std::min(pool.size(), chunks);
    // Nested blocking parallel loops must not starve the same pool of workers.
    if (task_count == 1 || pool.is_worker_thread()) {
        for (size_t i = 0; i < nTasks; ++i) func(i);
        return;
    }

    std::vector<std::future<void>> futures;
    futures.reserve(task_count);
    std::exception_ptr failure;
    try {
        size_t begin = 0;
        for (size_t task = 0; task < task_count; ++task) {
            const size_t end = begin + nTasks / task_count + (task < nTasks % task_count);
            futures.push_back(pool.enqueue([&func, begin, end] {
                for (size_t i = begin; i < end; ++i) func(i);
            }));
            begin = end;
        }
    } catch (...) {
        failure = std::current_exception();
    }
    // Wait for every accepted task, including when submission or a callback fails,
    // before func and the caller's referenced data can leave scope.
    for (auto& future : futures) {
        try { future.get(); }
        catch (...) { if (!failure) failure = std::current_exception(); }
    }
    if (failure) std::rethrow_exception(failure);
}

template<typename Func>
void parallelfor(Func func, size_t nTasks, size_t chunkSize = 1) {
    parallelfor(gThreadPool, std::move(func), nTasks, chunkSize);
}
} // namespace render
#endif
