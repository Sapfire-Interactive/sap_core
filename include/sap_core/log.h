#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

namespace sap::log {

    // ============================================================================
    // Simple Logging
    // ============================================================================
    // A minimal logging system. Logs to stderr with timestamps.
    //
    // Usage:
    //   sap::log::info("Server started on port {}", 8080);
    //   sap::log::error("Failed to open database: {}", error_msg);
    //
    // Log levels can be filtered at runtime via set_level().
    // ============================================================================

    enum class ELevel {
        Debug = 0,
        Info = 1,
        Warn = 2,
        Error = 3,
        None = 4 // Disables all logging
    };

    namespace detail {

        inline ELevel& current_level() {
            static ELevel level = ELevel::Info;
            return level;
        }

        inline std::string timestamp() {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::ostringstream oss;
            oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
            return oss.str();
        }

        inline const char* level_string(ELevel level) {
            switch (level) {
            case ELevel::Debug:
                return "DEBUG";
            case ELevel::Info:
                return "INFO ";
            case ELevel::Warn:
                return "WARN ";
            case ELevel::Error:
                return "ERROR";
            default:
                return "?????";
            }
        }

        inline const char* level_color(ELevel level) {
            switch (level) {
            case ELevel::Debug:
                return "\033[36m"; // Cyan
            case ELevel::Info:
                return "\033[32m"; // Green
            case ELevel::Warn:
                return "\033[33m"; // Yellow
            case ELevel::Error:
                return "\033[31m"; // Red
            default:
                return "\033[0m";
            }
        }

        // Simple format implementation (replaces {} with arguments)
        inline void format_impl(std::ostream& os, std::string_view fmt) { os << fmt; }

        template <typename T, typename... Args>
        void format_impl(std::ostream& os, std::string_view fmt, T&& arg, Args&&... args) {
            auto pos = fmt.find("{}");
            if (pos == std::string_view::npos) {
                os << fmt;
                return;
            }
            os << fmt.substr(0, pos) << std::forward<T>(arg);
            format_impl(os, fmt.substr(pos + 2), std::forward<Args>(args)...);
        }

        template <typename... Args>
        void log_impl(ELevel level, std::string_view fmt, Args&&... args) {
            if (level < current_level())
                return;
            std::ostringstream oss;
            format_impl(oss, fmt, std::forward<Args>(args)...);
            std::cerr << level_color(level) << "[" << timestamp() << "] " << level_string(level) << " "
                      << "\033[0m" << oss.str() << "\n";
        }

    } // namespace detail

    inline void set_level(ELevel level) { detail::current_level() = level; }

    inline ELevel get_level() { return detail::current_level(); }

    template <typename... Args>
    void debug(std::string_view fmt, Args&&... args) {
        detail::log_impl(ELevel::Debug, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(std::string_view fmt, Args&&... args) {
        detail::log_impl(ELevel::Info, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(std::string_view fmt, Args&&... args) {
        detail::log_impl(ELevel::Warn, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(std::string_view fmt, Args&&... args) {
        detail::log_impl(ELevel::Error, fmt, std::forward<Args>(args)...);
    }

} // namespace sap::log