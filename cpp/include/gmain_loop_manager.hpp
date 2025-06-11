/**
 * @file gmain_loop_manager.hpp
 * @brief Defines the GMainLoopManager class for managing a GLib GMainLoop.
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-04
 *
 * @details This file contains the declaration of the GMainLoopManager class,
 * which provides an abstraction for running a GLib main event loop in a
 * separate thread. It handles the lifecycle of the GMainLoop, including
 * creation, starting, stopping, and resource cleanup.
 */

#pragma once

// NOLINTBEGIN(*) Do not check the library
#include <glib.h>
// NOLINTEND(*)

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace blecpp {

/**
 * @class GMainLoopManager
 * @brief Manages a GLib GMainLoop required by gattlib for BLE operations.
 *
 * @details The GMainLoop is required by gattlib's underlying C API to process
 * Bluetooth events. This class manages the lifecycle of a GMainLoop that runs
 * in a dedicated thread. Key aspects:
 *
 * 1. Required by gattlib:
 *    - Both Scanner and Connection classes need a running GMainLoop
 *    - The loop must be started before any BLE operations
 *    - Multiple BLE operations can share the same loop
 *
 * 2. Event Processing:
 *    - Processes BLE events (device discovery, connections)
 *    - Executes gattlib callbacks in the loop's thread
 *    - Handles asynchronous BLE operations
 *
 * 3. Thread Safety:
 *    - Runs the loop in a dedicated thread
 *    - Thread-safe start/stop operations
 *    - Safe for multiple BLE operations
 *
 * @note The class is a wrapper around C API functions from GLib. Hence there
 * are no smart pointers used to avoid potential use-after-free issues.
 * Avoid using shared_from_this() or passing this to lambda callbacks.

 * Usage with gattlib:
 * @code
 * // 1. Create and start the loop manager first
 * auto loop_manager = std::make_shared<GMainLoopManager>();
 * if (!loop_manager->start()) {
 *     std::cerr << "Failed to start GMainLoop" << std::endl;
 *     return 1;
 * }
 *
 * // 2. Create Scanner/Connection with the running loop
 * auto scanner = std::make_shared<GattlibScanner>(loop_manager, adapter);
 * auto connection = std::make_shared<GattlibConnection>(loop_manager, adapter);
 *
 * // 3. Use Scanner/Connection - they rely on the running loop
 * scanner->scan();        // This works because loop is running
 * connection->connect();  // This also needs the running loop
 *
 * // 4. When done with ALL BLE operations, stop the loop
 * loop_manager->stop();
 * @endcode
 *
 * Important Notes:
 * - Start the loop BEFORE creating Scanner/Connection
 * - Keep the loop running while BLE operations are active
 * - Only stop the loop when ALL BLE operations are done
 */
class GMainLoopManager {
public:
  /**
   * Maximum time to wait for GMainLoop thread to start.
   * This value is based on typical GLib/DBus initialization times:
   * - GLib main loop init: ~500ms
   * - DBus system init: ~1-2s
   * - Bluetooth subsystem: ~1-2s
   * Total expected time: 2.5-5s
   */
  static constexpr auto THREAD_START_TIMEOUT = std::chrono::seconds(5);

  /**
   * Maximum time to wait for GMainLoop thread to stop in destructor.
   * Based on:
   * - Time for BLE operations to complete: ~1-2s
   * - Time for DBus to disconnect: ~1s
   * - Time for GLib cleanup: ~500ms
   * Plus safety margin for loaded systems.
   */
  static constexpr auto THREAD_STOP_TIMEOUT = std::chrono::seconds(5);

  /**
   * Maximum time to wait for thread join operations.
   * This is used when explicitly stopping the GMainLoop.
   * Longer than THREAD_STOP_TIMEOUT to allow for graceful shutdown.
   */
  static constexpr auto THREAD_JOIN_TIMEOUT = std::chrono::seconds(10);
  /**
   * @brief Constructs a GMainLoopManager instance.
   * @details Initializes internal state but does not start the GMainLoop.
   *          The loop must be started explicitly by calling start().
   */
  GMainLoopManager();

  /**
   * @brief Destroys the GMainLoopManager instance.
   * @details Ensures the GMainLoop is stopped gracefully and all associated
   * resources (GLib main loop, thread) are released. Calls stop() internally.
   */
  ~GMainLoopManager();

  // Delete copy constructor and copy assignment operator
  GMainLoopManager(const GMainLoopManager &) = delete;
  GMainLoopManager &operator=(const GMainLoopManager &) = delete;

  // Delete move constructor and move assignment operator
  GMainLoopManager(GMainLoopManager &&) = delete;
  GMainLoopManager &operator=(GMainLoopManager &&) = delete;

  /**
   * @brief Starts the GMainLoop in a new dedicated thread.
   *
   * @details If the loop is already running, this method logs a warning and
   * returns true. Otherwise, it creates a new GMainLoop, spawns a thread to run
   * it, and updates the running state. Thread safety: This method is
   * thread-safe due to internal locking.
   * @return True if the loop was started successfully or was already running,
   *         false if GMainLoop creation or thread spawning fails.
   */
  bool start();

  /**
   * @brief Stops the GMainLoop and joins its thread.
   *
   * @details If the loop is not running, this method returns immediately.
   * Otherwise, it requests the GMainLoop to quit, waits for its thread to
   * complete execution (join), and then releases GLib resources. Thread safety:
   * This method is thread-safe due to internal locking.
   * @note This function is blocking and will wait for the GMainLoop thread to
   * join.
   */
  void stop();

  /**
   * @brief Checks if the GMainLoop is currently active and running.
   *
   * @details Thread safety: This method is thread-safe as it reads an atomic
   * flag.
   * @return True if the loop is considered active, false otherwise.
   */
  [[nodiscard]] bool isRunning() const;

private:
  /// Pointer to the GMainLoop instance.
  GMainLoop *m_mainLoop;
  /// std::thread object managing the GMainLoop's execution thread.
  std::thread m_glibThread;
  /// Mutex to protect the start and stop logic, ensuring atomicity of these
  /// operations.
  std::mutex m_startStopMutex;
  /// Atomic flag indicating whether the GMainLoop is currently running.
  std::atomic<bool> m_isRunning;
};

} // namespace blecpp
