/**
 * @file log_macros.hpp
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-04
 *
 * This file provides a type-safe logging interface using C++20 concepts.
 * It enforces compile-time validation of logger requirements and provides
 * a clean, macro-based interface for logging at different severity levels.
 *
 * Example usage:
 * @code
 * BLECPP_LOG_INFO("Processing device {}", device_id);
 * BLECPP_LOG_ERROR("Failed to connect: {}", error.what());
 * @endcode
 */

#pragma once

#include <concepts>

// NOLINTBEGIN(*) Do not check the library
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
// NOLINTEND(*)

namespace blecpp {

/**
 * @brief Concept defining the requirements for a valid logger type.
 *
 * This concept ensures that any logger type provides the necessary logging
 * methods with the correct signatures. A type T satisfies this concept if it
 * provides:
 * - Logging methods (trace, debug, info, warn, error, critical)
 * - Configuration methods (set_level, set_pattern)
 *
 * All methods must return void and accept appropriate format strings and
 * arguments.
 *
 * @tparam T The logger type to validate
 */
template <typename T>
concept Logger = requires(T &logger) {
  { logger.trace("") } -> std::same_as<void>;
  { logger.debug("") } -> std::same_as<void>;
  { logger.info("") } -> std::same_as<void>;
  { logger.warn("") } -> std::same_as<void>;
  { logger.error("") } -> std::same_as<void>;
  { logger.critical("") } -> std::same_as<void>;
  { logger.set_level(spdlog::level::info) } -> std::same_as<void>;
  { logger.set_pattern("") } -> std::same_as<void>;
};

/**
 * @brief Thread-safe log manager that enforces logger requirements at compile
 * time.
 *
 * This class provides a static interface to a logger instance that satisfies
 * the Logger concept. It ensures thread-safe initialization and access to the
 * logger, and provides type-safe logging methods for different severity levels.
 *
 * @tparam T The logger type, must satisfy the Logger concept
 */
template <Logger T> class LogManager {
public:
  /**
   * @brief Get the singleton logger instance.
   *
   * This method ensures thread-safe lazy initialization of the logger.
   * The logger is configured with:
   * - Debug level logging enabled
   * - Timestamp, level, and message pattern
   * - Colored console output
   *
   * @return T& Reference to the logger instance
   */
  static T &get() {
    static auto instance = []() {
      std::shared_ptr<spdlog::logger> consoleLogger = spdlog::stdout_color_mt("logger");
      spdlog::set_default_logger(consoleLogger);
      spdlog::set_level(spdlog::level::debug);
      // %s:%# will be populated by source_loc when using the source-aware logging methods
      spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [%s:%#] [T %t] %v");

      return consoleLogger;
    }();
    return *instance;
  }

  /**
   * @brief Log a trace message.
   * @tparam Args Variadic template for format arguments
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void trace(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().trace(fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log a debug message.
   * @tparam Args Variadic template for format arguments
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void debug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().debug(fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log an info message.
   * @tparam Args Variadic template for format arguments
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void info(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().info(fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log a warning message.
   * @tparam Args Variadic template for format arguments
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void warn(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().warn(fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log an error message.
   * @tparam Args Variadic template for format arguments
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void error(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().error(fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log a critical message.
   * @tparam Args Variadic template for format arguments
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void critical(spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().critical(fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log a trace message with source location information.
   * @tparam Args Variadic template for format arguments
   * @param loc Source location information
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void traceWithSource(const spdlog::source_loc& loc, spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().log(loc, spdlog::level::trace, fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log a debug message with source location information.
   * @tparam Args Variadic template for format arguments
   * @param loc Source location information
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void debugWithSource(const spdlog::source_loc& loc, spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().log(loc, spdlog::level::debug, fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log an info message with source location information.
   * @tparam Args Variadic template for format arguments
   * @param loc Source location information
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void infoWithSource(const spdlog::source_loc& loc, spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().log(loc, spdlog::level::info, fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log a warning message with source location information.
   * @tparam Args Variadic template for format arguments
   * @param loc Source location information
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void warnWithSource(const spdlog::source_loc& loc, spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().log(loc, spdlog::level::warn, fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log an error message with source location information.
   * @tparam Args Variadic template for format arguments
   * @param loc Source location information
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void errorWithSource(const spdlog::source_loc& loc, spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().log(loc, spdlog::level::err, fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log a critical message with source location information.
   * @tparam Args Variadic template for format arguments
   * @param loc Source location information
   * @param fmt Format string
   * @param args Format arguments
   */
  template <typename... Args>
  static void criticalWithSource(const spdlog::source_loc& loc, spdlog::format_string_t<Args...> fmt, Args &&...args) {
    get().log(loc, spdlog::level::critical, fmt, std::forward<Args>(args)...);
  }

  LogManager() = delete; // Prevent instantiation

}; // class LogManager

/// Type alias for the concrete logger type we're using
using Logger_t = spdlog::logger;

/// Convenience type alias for our log manager
using Log = LogManager<Logger_t>;

} // namespace blecpp

/**
 * @brief User-facing logging macros.
 * These macros provide a simple interface to the logging system.
 * They automatically forward all arguments to the appropriate logging method.
 * They also capture source file and line information for better debugging.
 */
// Use spdlog's source location aware logging methods
#define BLECPP_LOG_TRACE(...) ::blecpp::Log::traceWithSource({__FILE__, __LINE__, __FUNCTION__}, __VA_ARGS__)       // NOLINT
#define BLECPP_LOG_DEBUG(...) ::blecpp::Log::debugWithSource({__FILE__, __LINE__, __FUNCTION__}, __VA_ARGS__)       // NOLINT
#define BLECPP_LOG_INFO(...) ::blecpp::Log::infoWithSource({__FILE__, __LINE__, __FUNCTION__}, __VA_ARGS__)         // NOLINT
#define BLECPP_LOG_WARN(...) ::blecpp::Log::warnWithSource({__FILE__, __LINE__, __FUNCTION__}, __VA_ARGS__)         // NOLINT
#define BLECPP_LOG_ERROR(...) ::blecpp::Log::errorWithSource({__FILE__, __LINE__, __FUNCTION__}, __VA_ARGS__)       // NOLINT
#define BLECPP_LOG_CRITICAL(...) ::blecpp::Log::criticalWithSource({__FILE__, __LINE__, __FUNCTION__}, __VA_ARGS__) // NOLINT