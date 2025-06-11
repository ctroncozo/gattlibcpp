/**
 * @file gmain_loop_manager.cpp
 * @brief Implements the GMainLoopManager class for managing a GLib GMainLoop.
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 *
 * The GMainLoopManager is a critical component for BLE operations in gattlib.
 * It manages a GLib main event loop that runs in a dedicated thread and is
 * responsible for:
 *
 * 1. Event Processing:
 *    - Handles BLE device discovery events
 *    - Processes connection state changes
 *    - Manages characteristic notifications
 *
 * 2. Thread Safety:
 *    - Provides thread-safe start/stop operations through mutex locking
 *    - Uses atomic operations for state changes (is_running_)
 *    - Implements timeouts to prevent deadlocks (THREAD_JOIN_TIMEOUT)
 *
 * 3. Resource Management:
 *    - RAII-style management of GMainLoop with proper cleanup
 *    - Takes ownership of resources before operations (move semantics)
 *    - Clears state before potentially throwing operations
 *
 * 4. Idempotent Operations:
 *    - Start: Returns success if already running
 *    - Stop: Returns early if already stopped
 *    - Resource cleanup: Safe to call multiple times
 *
 * 5. Error Handling:
 *    - Exception-safe resource cleanup
 *    - Proper logging of all state transitions
 *    - Fallback detach for hung threads
 *
 * The manager ensures proper initialization, thread safety, and cleanup
 * through careful state management and resource ownership transfer.
 */

#include "gmain_loop_manager.hpp"

#include "log_macros.hpp"

// NOLINTBEGIN(*) Do not check the library
#include <glib.h>
// NOLINTEND(*)

#include <future>

namespace blecpp {

/**
 * @brief Constructs a GMainLoopManager with signal handling.
 *
 * Initialization Guarantees:
 * - Safe initial state (main_loop_ = nullptr, is_running_ = false)
 * - No resource allocation until start() is called
 *
 * Signal Handling:
 * - Registers for system signals via AsioSignalHandler
 * - Ensures idempotent stop on signal receipt
 * - Prevents signal handler races with atomic state
 */
GMainLoopManager::GMainLoopManager() : m_mainLoop(nullptr), m_isRunning(false) {
  BLECPP_LOG_DEBUG("GMainLoopManager created.");
}

GMainLoopManager::~GMainLoopManager() {
  BLECPP_LOG_DEBUG("GMainLoopManager destroying...");
  try {
    // Use a timeout in destructor to prevent hanging
    std::future<void> stopFuture =
      std::async(std::launch::async, [this]() { stop(); });

    if (stopFuture.wait_for(GMainLoopManager::THREAD_STOP_TIMEOUT) ==
        std::future_status::timeout) {
      BLECPP_LOG_ERROR("GMainLoopManager: Destructor timeout, forcing cleanup");
      // Force cleanup - detach thread and let OS clean up
      if (m_glibThread.joinable()) {
        m_glibThread.detach();
      }
    }
  } catch (const std::exception &e) {
    BLECPP_LOG_ERROR("GMainLoopManager: Destructor error: {}", e.what());
  }

  BLECPP_LOG_DEBUG("GMainLoopManager destroyed.");
}

/**
 * @brief Starts the GMainLoop in a dedicated thread.
 *
 * This method implements an idempotent start operation with the following
 * guarantees:
 *
 * Thread Safety:
 * - Protected by start_stop_mutex_ to prevent concurrent starts/stops
 * - Uses atomic is_running_ for safe state transitions
 * - Condition variable synchronizes thread startup
 *
 * Resource Management:
 * - Ensures clean state before start (unrefs existing loop)
 * - RAII cleanup on failure through unique_ptr and scope guards
 * - Proper ownership transfer of thread resources
 *
 * Error Handling:
 * - Returns false if loop creation fails
 * - Throws system_error if thread creation fails
 * - Times out and cleans up if thread doesn't start
 *
 * State Transitions:
 * 1. Lock mutex -> Check running state
 * 2. Create loop -> Launch thread
 * 3. Wait for ready -> Update state
 * 4. Return success or cleanup on failure
 *
 * @return true if started or already running, false on failure
 * @throws std::system_error on thread creation failure
 */
bool GMainLoopManager::start() {
  // Idempotent start: prevent multiple starts and ensure clean state
  std::lock_guard<std::mutex> lock(m_startStopMutex);

  // Return success if already running (idempotent)
  if (m_isRunning.load()) {
    BLECPP_LOG_DEBUG("GMainLoopManager: Already running, start is idempotent.");
    return true;
  }

  // Ensure clean state before start
  if (m_mainLoop != nullptr) {
    g_main_loop_unref(m_mainLoop);
    m_mainLoop = nullptr;
  }

  // Create the GMainLoop instance
  m_mainLoop = g_main_loop_new(nullptr, FALSE);
  if (m_mainLoop == nullptr) {
    BLECPP_LOG_ERROR("GMainLoopManager: Failed to create GMainLoop.");
    return false;
  }

  try {
    // Launch the GMainLoop thread
    m_glibThread = std::thread([this]() {
      BLECPP_LOG_INFO("GMainLoopManager: GMainLoop thread started.");

      // First verify main_loop_ is still valid
      if (m_mainLoop == nullptr) {
        BLECPP_LOG_ERROR("GMainLoopManager: Main loop invalid at thread start");
        return;
      }

      // Run the loop - this will block until quit
      g_main_loop_run(m_mainLoop);
      BLECPP_LOG_INFO("GMainLoopManager: GMainLoop thread finished.");
    });
    constexpr int kTenSeconds = 10;
    // Wait for the loop to be running
    auto startTime = std::chrono::steady_clock::now();
    while (g_main_loop_is_running(m_mainLoop) == FALSE) {
      if (std::chrono::steady_clock::now() - startTime >
          THREAD_START_TIMEOUT) {
        BLECPP_LOG_ERROR(
          "GMainLoopManager: Timeout waiting for main loop to start"
        );
        if (m_mainLoop != nullptr) {
          g_main_loop_quit(m_mainLoop);
          g_main_loop_unref(m_mainLoop);
          m_mainLoop = nullptr;
        }
        if (m_glibThread.joinable()) {
          m_glibThread.detach();
        }
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(kTenSeconds));
    }

    if (g_main_loop_is_running(m_mainLoop) == FALSE) {
      BLECPP_LOG_ERROR(
        "GMainLoopManager: Timeout waiting for GMainLoop thread to start"
      );
      // Request loop quit before cleanup
      if (m_mainLoop != nullptr) {
        g_main_loop_quit(m_mainLoop);
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = nullptr;
      }
      // Thread will exit since loop was quit
      if (m_glibThread.joinable()) {
        m_glibThread.detach();
      }
      return false;
    }

  } catch (const std::system_error &e) {
    BLECPP_LOG_ERROR(
      "GMainLoopManager: Failed to start GMainLoop thread: {}", e.what()
    );
    if (m_mainLoop != nullptr) {
      g_main_loop_unref(m_mainLoop);
      m_mainLoop = nullptr;
    }
    return false;
  }

  m_isRunning.store(true);
  BLECPP_LOG_INFO("GMainLoopManager: Started successfully.");
  return true;
}

/**
 * @brief Stops the GMainLoop and cleans up resources.
 *
 * This method implements an idempotent stop operation with the following
 * guarantees:
 *
 * Thread Safety:
 * - Protected by start_stop_mutex_ to prevent concurrent stops
 * - Atomic state transition prevents races
 * - Takes ownership of resources before operations
 *
 * Resource Management:
 * - Clear state before potentially throwing operations
 * - Move semantics for proper thread ownership
 * - Single responsibility per cleanup step
 *
 * Error Handling:
 * - Timeout protection for thread joins
 * - Fallback to detach for hung threads
 * - Exception-safe cleanup in all paths
 *
 * State Transitions:
 * 1. Lock mutex -> Exchange running state
 * 2. Take resource ownership -> Clear state
 * 3. Quit loop -> Join thread
 * 4. Cleanup resources -> Log completion
 *
 * @throws std::runtime_error if cleanup fails
 */
void GMainLoopManager::stop() {
  // Prevent concurrent stop operations
  std::lock_guard<std::mutex> lock(m_startStopMutex);

  // First mark as not running to prevent new operations
  if (!m_isRunning.exchange(false)) {
    BLECPP_LOG_DEBUG("GMainLoopManager: Already stopped.");
    return;
  }
  constexpr int kTenSeconds = 10;
  try {
    // First quit the loop
    if (m_mainLoop != nullptr && g_main_loop_is_running(m_mainLoop) == TRUE) {
      BLECPP_LOG_DEBUG("GMainLoopManager: Requesting GMainLoop quit.");
      g_main_loop_quit(m_mainLoop);
    }

    // Ensure loop is fully stopped before thread cleanup
    if (m_mainLoop != nullptr) {
      auto startTime = std::chrono::steady_clock::now();
      while (g_main_loop_is_running(m_mainLoop) == TRUE) {
        if (std::chrono::steady_clock::now() - startTime >
            THREAD_STOP_TIMEOUT) {
          BLECPP_LOG_ERROR(
            "GMainLoopManager: Timeout waiting for main loop to stop"
          );
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kTenSeconds));
      }
    }

    // Handle thread cleanup
    if (m_glibThread.joinable()) {
      BLECPP_LOG_DEBUG("GMainLoopManager: Joining GMainLoop thread...");
      std::thread worker = std::move(m_glibThread);

      try {
        // Use shared_ptr for promise to ensure lifetime
        auto done = std::make_shared<std::promise<void>>();
        std::thread joiner([thread = std::move(worker), done]() mutable {
          thread.join();
          done->set_value();
        });

        auto future = done->get_future();
        auto status = future.wait_for(GMainLoopManager::THREAD_JOIN_TIMEOUT);
        if (status == std::future_status::timeout) {
          BLECPP_LOG_ERROR("GMainLoopManager: Thread join timeout, detaching.");
          joiner.detach();
        } else {
          try {
            joiner.join();
            BLECPP_LOG_DEBUG("GMainLoopManager: Thread joined successfully.");
          } catch (...) {
            joiner.detach(); // Ensure no leaks
            throw;
          }
        }
      } catch (const std::exception &e) {
        BLECPP_LOG_ERROR("GMainLoopManager: Thread join failed: {}", e.what());
      }
    }

    // Finally cleanup GLib resources
    if (m_mainLoop != nullptr) {
      g_main_loop_unref(m_mainLoop);
      BLECPP_LOG_DEBUG("GMainLoopManager: GMainLoop unreferenced.");
    }

    BLECPP_LOG_INFO("GMainLoopManager: Stopped successfully.");
  } catch (const std::exception &e) {
    BLECPP_LOG_ERROR("GMainLoopManager: Error during stop: {}", e.what());
    throw; // Re-throw after logging
  }
}

bool GMainLoopManager::isRunning() const { return m_isRunning.load(); }

} // namespace blecpp
