/**
 * @file gattlib_functions.hpp
 * @brief Defines function types and container for gattlib API
 *
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-10
 */

#pragma once

#include "gattlib.h"

#include <cstddef>

// NOTE: This header relies on types defined in gattlib.h (such as
// gattlib_adapter_t and gattlib_discovered_device_t). Do not redefine these
// types here to avoid conflicts and ensure consistency with the C API.

namespace blecpp {

/**
 * @brief Container for function pointers that match the gattlib C API
 *
 * This class provides a way to inject different implementations of the
 * gattlib functions, allowing for dependency injection in production code
 * and tests. In production, it can be initialized with pointers to the real
 * gattlib functions. In tests, it can be initialized with pointers to mock
 * implementations.
 *
 * Benefits:
 * - Enables unit testing without real Bluetooth hardware
 * - Allows simulation of error conditions and edge cases
 * - Provides a clean separation between business logic and external
 * dependencies
 * - Makes code more maintainable by centralizing external API dependencies
 *
 * Example usage with real functions:
 * @code
 * GattlibFunctions functions;
 * functions.adapter_open = &gattlib_adapter_open;
 * functions.adapter_close = &gattlib_adapter_close;
 * functions.adapter_scan_enable = &gattlib_adapter_scan_enable;
 * functions.adapter_scan_disable = &gattlib_adapter_scan_disable;
 * functions.adapter_wait_scan_stopped = &gattlib_adapter_wait_scan_stopped;
 *
 * GattlibScanner scanner("hci0", functions);
 * scanner.scan(5, 3, "00:11:22:33:44:55");
 * @endcode
 *
 * Example usage with mock functions:
 * @code
 * // Using lambda functions for simple mocks
 * GattlibFunctions mock_functions;
 * mock_functions.adapter_open = [](const char*, gattlib_adapter_t** adapter) {
 *   *adapter = reinterpret_cast<gattlib_adapter_t*>(0x1234);  // Fake pointer
 *   return GATTLIB_SUCCESS;
 * };
 *
 * // Or with Google Mock:
 * GattlibMock& mock = GattlibMock::getInstance();
 * EXPECT_CALL(mock, adapter_open(_, _))
 *     .WillOnce(Return(GATTLIB_SUCCESS));
 * GattlibFunctions functions = mock.getFunctions();
 *
 * GattlibScanner scanner("hci0", functions);
 * @endcode
 */
struct GattlibFunctions {
  /**
   * @brief Function pointer type for opening a Bluetooth adapter
   *
   * Corresponds to gattlib_adapter_open() in the gattlib API.
   *
   * @param adapter_name Name of the adapter to open (e.g., "hci0")
   * @param adapter Pointer to store the opened adapter handle
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using AdapterOpenFn =
    int (*)(const char *adapter_name, gattlib_adapter_t **adapter);

  /**
   * @brief Function pointer type for closing a Bluetooth adapter
   *
   * Corresponds to gattlib_adapter_close() in the gattlib API.
   *
   * @param adapter Handle to the adapter to close
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using AdapterCloseFn = int (*)(gattlib_adapter_t *adapter);

  /**
   * @brief Function pointer type for enabling Bluetooth scanning
   *
   * Corresponds to gattlib_adapter_scan_enable() in the gattlib API.
   *
   * @param adapter Handle to the adapter to use for scanning
   * @param callback Function to call when a device is discovered
   * @param timeout Timeout for scanning in seconds (0 for indefinite)
   * @param user_data User data to pass to the callback
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using AdapterScanEnableFn = int (*)(
    gattlib_adapter_t *adapter, gattlib_discovered_device_t callback,
    size_t timeout, void *user_data
  );

  /**
   * @brief Function pointer type for disabling Bluetooth scanning
   *
   * Corresponds to gattlib_adapter_scan_disable() in the gattlib API.
   *
   * @param adapter Handle to the adapter to stop scanning
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using AdapterScanDisableFn = int (*)(gattlib_adapter_t *adapter);

  /**
   * @brief Function pointer type for waiting for the scan to stop
   *
   * Corresponds to gattlib_adapter_wait_scan_stopped() in the gattlib API.
   *
   * @param adapter Handle to the adapter to wait for
   */
  using AdapterWaitScanStoppedFn = void (*)(gattlib_adapter_t *adapter);

  /// Function to open a Bluetooth adapter
  AdapterOpenFn adapter_open;
  /// Function to close a Bluetooth adapter
  AdapterCloseFn adapter_close;
  /// Function to enable Bluetooth scanning
  AdapterScanEnableFn adapter_scan_enable;
  /// Function to disable Bluetooth scanning
  AdapterScanDisableFn adapter_scan_disable;
  /// Function to wait for scan to stop
  void (*adapter_wait_scan_stopped)(gattlib_adapter_t *adapter);

  /**
   * @brief Default constructor
   *
   * Initializes all function pointers to nullptr.
   * You must set all function pointers before using this object.
   */
  GattlibFunctions()
    : adapter_open(nullptr), adapter_close(nullptr),
      adapter_scan_enable(nullptr), adapter_scan_disable(nullptr),
      adapter_wait_scan_stopped(nullptr) {}

  /**
   * @brief Checks if all function pointers are set
   *
   * @return true if all function pointers are non-null, false otherwise
   */
  bool is_complete() const {
    return adapter_open && adapter_close && adapter_scan_enable &&
           adapter_scan_disable && adapter_wait_scan_stopped;
  }
};

} // namespace blecpp
