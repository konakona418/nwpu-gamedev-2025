#pragma once

#include "Core/Common.hpp"
#include "Core/Task/Future.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

MOE_BEGIN_NAMESPACE

struct ThreadPoolScheduler {
public:
    static ThreadPoolScheduler& getInstance();

    static void init(size_t threadCount = std::thread::hardware_concurrency());

    static void shutdown();

    void schedule(Function<void()> task);

    size_t workerCount() const { return m_workers.size(); }

    template<typename F>
    void schedule(F&& task) {
        schedule(Function<void()>(std::forward<F>(task)));
    }

private:
    Vector<std::thread> m_workers;
    Queue<Function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic_bool m_running{false};
    std::once_flag m_initFlag;
    std::once_flag m_shutdownFlag;

    ThreadPoolScheduler() = default;

    ~ThreadPoolScheduler() {
        stop();
    }

    ThreadPoolScheduler(const ThreadPoolScheduler&) = delete;
    ThreadPoolScheduler& operator=(const ThreadPoolScheduler&) = delete;

    void start(size_t threadCount);

    void stop();

    void workerMain();
};

struct MainScheduler {
public:
    static MainScheduler& getInstance();

    void init() {
        m_mainThreadId = std::this_thread::get_id();
        m_initialized = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lk(m_queueMutex);
        while (!m_taskQueue.empty()) {
            m_taskQueue.front()();
            m_taskQueue.pop();
        }
        m_initialized = false;
    }

    void processTasks();

    void schedule(Function<void()> task);

    bool isMainThread() const {
        MOE_ASSERT(m_initialized, "MainScheduler not initialized");
        return std::this_thread::get_id() == m_mainThreadId;
    }

    template<typename F>
    void schedule(F&& task) {
        schedule(Function<void()>(std::forward<F>(task)));
    }

private:
    bool m_initialized{false};
    std::mutex m_queueMutex;
    Queue<Function<void()>> m_taskQueue;

    std::thread::id m_mainThreadId;
};

MOE_END_NAMESPACE