#pragma once

#include <memory>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

namespace moe {
    class Logger {
    public:
        void initialize();
        void shutdown();
        void flush();

        static void setThreadName(std::string_view name);

        template<typename... Args>
        static void info(const char* fmt, const Args&... args) {
            get()->log(spdlog::level::info, fmt, args...);
        }
        template<typename... Args>
        static void warn(const char* fmt, const Args&... args) {
            get()->log(spdlog::level::warn, fmt, args...);
        }
        template<typename... Args>
        static void error(const char* fmt, const Args&... args) {
            get()->log(spdlog::level::err, fmt, args...);
        }
        template<typename... Args>
        static void critical(const char* fmt, const Args&... args) {
            get()->log(spdlog::level::critical, fmt, args...);
        }
        template<typename... Args>
        static void debug(const char* fmt, const Args&... args) {
            get()->log(spdlog::level::debug, fmt, args...);
        }

        static std::shared_ptr<Logger> get();

    private:
        std::string_view getThreadName() const {
            auto it = m_threadNames.find(std::this_thread::get_id());
            if (it != m_threadNames.end()) return it->second;
            return "Unknown";
        }

        template<typename... Args>
        void log(spdlog::level::level_enum lvl, const char* fmt, Args&&... args) {
            auto threadName = getThreadName();
            auto output = fmt::format(fmt, std::forward<Args>(args)...);
            if (m_logger) m_logger->log(lvl, "[{}] {}", threadName, output);
        }

        std::unordered_map<std::thread::id, std::string> m_threadNames;

        std::shared_ptr<spdlog::logger> m_logger;
        static std::shared_ptr<Logger> m_instance;
    };
}// namespace moe