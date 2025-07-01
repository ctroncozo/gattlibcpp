/**
 * @file gattlib_client_impl.hpp
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-03
 *
 * @brief Implementation details for GattlibClient using PIMPL pattern.
 *
 */

#pragma once

// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
// NOLINTEND(*)

#include "gattlib_client.hpp"
#include "gattlib_scanner.hpp"
#include "gmain_loop_manager.hpp"

#include <condition_variable>
#include <mutex>
#include <string>

namespace blecpp {

/**
 * @enum ConnectionState
 * @brief Enumerates the possible states of a BLE connection.
 *
 * DISCONNECTED: The device is not connected.
 * CONNECTING: A connection attempt is in progress.
 * CONNECTED: The device is currently connected.
 * FAILED: The connection attempt failed or the connection is unusable.
 * ABORTED: The connection attempt was aborted.
 * SHUTTING_DOWN: A disconnect operation is in progress.
 */
enum class ConnectionState {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  FAILED,
  ABORTED,
  SHUTTING_DOWN
};

/**
 * @struct ConnectionContext
 * @brief Holds all state and synchronization for a single BLE connection.
 *
 * This struct is used to manage the lifecycle, state, and synchronization of
 * a BLE device connection. It is owned and managed by GattlibClient, and
 * accessed by both the connection thread and C API callbacks.
 *
 * Thread safety: Uses atomics and mutexes to protect state and shared
 * resources.
 */
struct ConnectionContext {
  /// BLE device address.
  std::string address;
  /// Pointer to the BLE adapter used for this connection. Managed with C API.
  gattlib_adapter_t *adapterPtr {nullptr};
  /// Number of connection attempts remaining.
  std::atomic<size_t> attempts {1};
  /// Current state of the connection (see ConnectionState).
  std::atomic<ConnectionState> state {ConnectionState::DISCONNECTED};
  /// Pointer to the underlying gattlib connection. Managed with C API.
  gattlib_connection_t *connection {nullptr};
  /// Protects state and connection pointer.
  std::mutex mutex;
  /// Used to signal state changes (e.g., disconnect) to waiting threads.
  std::condition_variable cv;
  /// Pointer to the thread handling this connection.
  std::thread thread;
  /// GattlibFunctions for gattlib operations
  const GattlibFunctions *functions {nullptr};
};


class GattlibClient::Impl {
private:

public:
  /**
   * @brief Construct a new GattlibClient object
   *
   * @details Initiates the gmain loop manager, starts the event loop and opens
   * the adapter.
   *
   * @param functions Optional GattlibFunctions object to use for C API calls.
   */
  explicit Impl(const GattlibFunctions &functions = GattlibFunctions());

  /// Deep copy constructor deleted
  Impl(const Impl &) = delete;
  /// Copy operator deleted
  Impl &operator=(const Impl &) = delete;
  /// Move constructor deleted
  Impl(Impl &&) = delete;
  /// Move operator deleted
  Impl &operator=(Impl &&) = delete;

  /**
   * @brief Destroy the GattlibClient object
   *
   * Stops the event loop and cleans up all resources. Idempotent and
   * thread-safe.
   */
  ~Impl();

  /**
   * @see GattlibClient::connect
   */
  int connect(int32_t timeoutSec, const std::string &address);

  /**
   * @see GattlibClient::disconnect
   */
  int disconnect();

  /**
   * @see GattlibClient::isConnected
   */
  [[nodiscard]] bool isConnected() const;

  /**
   * @brief Shuts down the client and releases all resources.
   *
   * @details This function stops the event loop, disconnects from any active
   * connections, and releases all resources. It is idempotent and thread-safe.
   */
  void shutdown();

private:
  /**
   * @brief Callback to be invoked by gattlib during a connection attempt.
   *
   * @details This is an asynchronous callback that is invoked by gattlib when
   * a connection attempt is completed. It is used to update the connection
   * state and notify any waiting threads.
   *
   * @param adapter The adapter used for the connection attempt
   * @param dst The destination address of the connection attempt
   * @param connection The connection object
   * @param error The error code
   * @param userData User data passed to the callback
   */
  static void onDeviceConnect(
    gattlib_adapter_t *adapter, const char *dst,
    gattlib_connection_t *connection, int error, void *userData
  );

  /**
   * @brief Callback invoked when a BLE device is disconnected.
   *
   * @details This is an asynchronous callback that is invoked by gattlib when
   * a connection is lost or closed. It is used to update the connection state
   * and notify any waiting threads.
   *
   * @param connection BLE connection pointer
   * @param userData Pointer to ConnectionContext for the BLE device
   * @param connection BLE connection pointer
   * @param userData Pointer to ConnectionContext for the BLE device
   */
  static void
    onDeviceDisconnect(gattlib_connection_t *connection, void *userData);

  /**
   * @brief Dumps the connection data for the connected BLE device.
   *
   * @param connectionData Pointer to ConnectionContext for the BLE device
   */
  static void dumpConnectionData(ConnectionContext *connectionData);

  /**
   * @brief Callback invoked when a BLE device is discovered.
   *
   * @details This callback is passed to the scanner and it will be called when
   * the device with the given address is discovered.
   * The function create a thread to connect to the device.
   *
   * @param adapter The adapter used for scanning
   * @param addr The address of the discovered device
   * @param name The name of the discovered device
   * @param userData User data passed to the callback
   */
  static void onDeviceDiscoverConnect(
    gattlib_adapter_t *adapter, const char *addr, const char *name,
    void *userData
  );

  /**
   * @brief Asynchronously waits for the connection to reach the disconnected state
   * 
   * @param timeoutMs Timeout in milliseconds to wait for disconnect
   * @return true if disconnect completed successfully, false if timed out
   */
  [[nodiscard]] bool waitForDisconnect(int timeoutMs);

  /**
   * @brief Converts a ConnectionState to a string.
   *
   * @param state The ConnectionState to convert
   * @return std::string The string representation of the ConnectionState
   */
  static std::string toString(ConnectionState state) {
    switch (state) {
      case ConnectionState::DISCONNECTED:
        return "DISCONNECTED";
      case ConnectionState::CONNECTING:
        return "CONNECTING";
      case ConnectionState::CONNECTED:
        return "CONNECTED";
      case ConnectionState::FAILED:
        return "FAILED";
      case ConnectionState::ABORTED:
        return "ABORTED";
      case ConnectionState::SHUTTING_DOWN:
        return "SHUTTING_DOWN";
      default:
        return "UNKNOWN";
    }
  }

  /**
   * @brief Cleanup resources used by the connection.
   *
   * @param connectionData Pointer to ConnectionContext for the BLE device
   */
  static void cleanup(ConnectionContext *connectionData);

  /// GMainLoopManager for BLE operations
  std::shared_ptr<GMainLoopManager> m_gmainLoopManager {nullptr};
  /// Pointer to the BLE adapter
  gattlib_adapter_t *m_adapterPtr {nullptr};
  /// GattlibFunctions for gattlib operations
  const GattlibFunctions &m_gattlibFunctions;
  /// Pointer to a scanner
  std::shared_ptr<GattlibScanner> m_scanner {nullptr};
  /// Current connection context (raw pointer for C API callbacks)
  ConnectionContext* m_connectionContext {nullptr};
};
} // namespace blecpp
