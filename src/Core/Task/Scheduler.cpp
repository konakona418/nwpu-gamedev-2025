#include "Core/Task/Scheduler.hpp"

MOE_BEGIN_NAMESPACE

ThreadPoolScheduler& ThreadPoolScheduler::getInstance() {
    static ThreadPoolScheduler instance;
    return instance;
}

void ThreadPoolScheduler::init(size_t threadCount) {
    auto& inst = getInstance();
    threadCount = threadCount == 0 ? 1 : threadCount;
    std::call_once(inst.m_initFlag, [threadCount, &inst]() {
        Logger::info("Initializing scheduler with {} threads", threadCount);
        inst.start(threadCount);
    });
}

void ThreadPoolScheduler::shutdown() {
    Logger::info("Shutting down scheduler");

    auto& inst = getInstance();
    std::call_once(inst.m_shutdownFlag, [&inst]() {
        inst.stop();
    });
}

void ThreadPoolScheduler::schedule(Function<void()> task) {
    MOE_ASSERT(m_running, "Scheduler not running");
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_running) return;
        m_tasks.emplace(std::move(task));
    }
    m_cv.notify_one();
}

void ThreadPoolScheduler::start(size_t threadCount) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_running) return;
        m_running = true;
    }

    m_workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back([this, i]() {
            Logger::setThreadName(fmt::format("Worker#{}", i));
            Logger::info("Scheduler thread Worker#{} started", i);
            workerMain();
        });
    }
}

void ThreadPoolScheduler::stop() {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_running) return;
        m_running = false;
    }
    m_cv.notify_all();
    for (auto& t: m_workers) {
        if (t.joinable()) t.join();
    }
    m_workers.clear();

    Queue<Function<void()>> empty;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::swap(m_tasks, empty);
    }
}

void ThreadPoolScheduler::workerMain() {
    while (true) {
        Function<void()> task;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this]() { return !m_running || !m_tasks.empty(); });
            if (!m_running && m_tasks.empty()) return;
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        task();
    }
}

MainScheduler& MainScheduler::getInstance() {
    static MainScheduler instance;
    return instance;
}

void MainScheduler::schedule(Function<void()> task) {
    MOE_ASSERT(m_initialized, "MainThreadDispatcher not initialized");

    if (isMainThread()) {
        task();
        return;
    }

    {
        std::lock_guard<std::mutex> lk(m_queueMutex);
        m_taskQueue.emplace(std::move(task));
    }
}

void MainScheduler::processTasks() {
    MOE_ASSERT(
            isMainThread(),
            "MainThreadDispatcher::processTasks() called from non-main thread");

    Queue<Function<void()>> tasksToProcess;
    {
        std::lock_guard<std::mutex> lk(m_queueMutex);
        std::swap(tasksToProcess, m_taskQueue);
    }

    while (!tasksToProcess.empty()) {
        Function<void()> task = std::move(tasksToProcess.front());
        tasksToProcess.pop();
        task();
    }
}

MOE_END_NAMESPACE