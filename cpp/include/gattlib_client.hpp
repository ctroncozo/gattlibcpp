/**
 * @file gattlib_client.hpp
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-03
 *
 * @brief BLE connection/session manager with robust thread-safe lifecycle
 * management.
 *
 * This file defines the GattlibClient class and supporting types for managing
 * concurrent BLE device connections. It provides:
 *   - Thread-safe connection scheduling, state management, and resource cleanup
 *   - Compatibility with C APIs (gattlib, GLib, BlueZ) via raw pointers and
 * C-style callbacks
 *   - Diagnostics and introspection of device GATT profiles
 *   - Automatic cleanup and graceful shutdown for long-running applications
 */

#pragma once

#include "gattlib_functions.hpp"

// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
#include <glib.h>
// NOLINTEND(*)

#include <cstdint>
#include <memory>
#include <string>

namespace blecpp {

/**
 * @class GattlibClient
 * @brief BLE connection/session manager with robust lifecycle and resource
 * management.
 *
 * @details
 * - Handles connection attempts, retries, and asynchronous connection logic
 * via background threads.
 * - Ensures robust cleanup and resource management, even in the presence of
 * errors or signals.
 * - Uses C-style memory management and raw pointers for compatibility with
 * gattlib, GLib, and BlueZ APIs.
 * - Thread-safe: uses mutexes and atomics to protect all shared state and
 * connection resources.
 * - Designed for use in long-running applications that require reliable BLE
 * connectivity and graceful shutdown.
 *
 * The API provides synchronous feedback on connection scheduling, while the
 * actual BLE connection process is asynchronous. All resources are cleaned up
 * automatically on shutdown or destruction.
 */
class GattlibClient {
public:
  /**
   * @brief Construct a new GattlibClient object
   *
   * @details Initiates the gmain loop manager, starts the event loop and
   * opens the adapter.
   *
   * @param functions Optional GattlibFunctions object to use for C API calls.
   */
  GattlibClient(const GattlibFunctions &functions = GattlibFunctions());

  /**
   * @brief Destroy the GattlibClient object
   *
   * Stops the event loop and cleans up all resources. Idempotent and
   * thread-safe.
   */
  ~GattlibClient();

  /// Delete copy constructor
  GattlibClient(const GattlibClient &) = delete;
  /// Delete copy operator
  GattlibClient &operator=(const GattlibClient &) = delete;
  /// Delete moving constructor
  GattlibClient(GattlibClient &&) = delete;
  /// Delete moving operator
  GattlibClient &operator=(GattlibClient &&) = delete;

  /**
   * @brief Initiates a BLE connection attempt to the specified device
   * address.
   *
   * @details
   * - The function returns synchronously, indicating whether the connection
   * request was successfully etablished.
   * - A return value of GATTLIB_SUCCESS means that the connection was
   * successfully etablished.
   * - A return value of GATTLIB_TIMEOUT means that the connection attempt
   * timed out.
   * - A return value of GATTLIB_DEVICE_ERROR means that the connection attempt
   * failed.
   * Raw pointers are used for ConnectionContext due to the need for
   * compatibility with C libraries and callback-heavy APIs (such as BlueZ,
   * GLib, and DBus), which require C-style memory management and function
   * signatures. All resources are properly managed and cleaned up internally.
   *
   * @param timeoutSec The timeout for the connection attempt in seconds. If
   * the connection can't be started after this timeout, the function returns
   * an error.
   * @param address The BLE device address to connect to.
   * @return GATTLIB_SUCCESS if the connection request was scheduled
   * successfully (thread created and context inserted), GATTLIB_DEVICE_ERROR
   * otherwise.
   */
  int connect(int32_t timeoutSec, const std::string &address);

  /**
   * @brief Checks if the client is currently connected to a BLE device.
   *
   * @return true if the client is connected to a BLE device, false otherwise.
   */
  [[nodiscard]] bool isConnected() const;

  /**
   * @brief Disconnects from a BLE device.
   *
   * @details This is a blocking function that will wait for the disconnection
   * to complete. It will return GATTLIB_SUCCESS if the disconnection was
   * successful, GATTLIB_DEVICE_ERROR otherwise.
   * @details It call the gattlib_disconnect function to disconnect from the
   * device. and update the internal state to DISCONNECTED.
   *
   * @return GATTLIB_SUCCESS if the disconnection request was scheduled
   * successfully, GATTLIB_DEVICE_ERROR otherwise.
   */
  int disconnect();

  /**
   * @brief Shutodown the client.
   * @details It shutdown the gmain loop and clean up all resources, specially
   * the scanner, the adapter and the deleted connection context.
   */
  void shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> m_pimpl;
};
} // namespace blecpp