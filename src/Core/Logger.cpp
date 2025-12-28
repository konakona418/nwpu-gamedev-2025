#include "Core/Logger.hpp"

#include <mutex>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace moe {
    std::shared_ptr<Logger> Logger::m_instance{nullptr};

    void Logger::initialize() {
        constexpr std::size_t queue_size = 8192;
        spdlog::init_thread_pool(queue_size, 1);

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%T] [%^%l%$] %v");

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("engine.log", true);
        file_sink->set_pattern("[%Y-%m-%d %T] [%l] %v");

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

        m_logger = std::make_shared<spdlog::async_logger>(
                "moe",
                sinks.begin(), sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
        m_logger->set_level(spdlog::level::debug);
        m_logger->flush_on(spdlog::level::info);

        spdlog::register_logger(m_logger);
    }

    void Logger::shutdown() {
        spdlog::drop_all();
        m_logger.reset();
        spdlog::shutdown();
    }

    void Logger::flush() {
        if (m_logger) m_logger->flush();
    }

    void Logger::setThreadName(std::string_view name) {
        static std::mutex mutex;
        // protect m_threadNames map
        {
            std::lock_guard<std::mutex> lk(mutex);
            auto logger = get();
            logger->m_threadNames[std::this_thread::get_id()] = name;
        }
    }

    std::shared_ptr<Logger> Logger::get() {
        static std::once_flag flag;
        std::call_once(flag, []() {
            m_instance = std::make_shared<Logger>();
            m_instance->initialize();
        });
        return m_instance;
    }
}// namespace moe