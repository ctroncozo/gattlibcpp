/**
 * @file gattlib_functions.hpp
 * @brief Defines function types and container for gattlib API
 *
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-10
 */

#pragma once

// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
// NOLINTEND(*)


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
 * functions.adapterOpen = &gattlib_adapter_open;
 * functions.adapterClose = &gattlib_adapter_close;
 * functions.adapterScanEnable = &gattlib_adapter_scan_enable;
 * functions.adapterScanDisable = &gattlib_adapter_scan_disable;
 * functions.adapterWaitScanStopped = &gattlib_adapter_wait_scan_stopped;
 *
 * GattlibScanner scanner("hci0", functions);
 * scanner.scan(5, 3, "00:11:22:33:44:55");
 * @endcode
 *
 * Example usage with mock functions:
 * @code
 * // Using lambda functions for simple mocks
 * GattlibFunctions mock_functions;
 * mock_functions.adapterOpen = [](const char* adapterName, gattlib_adapter_t**
adapter) {
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
   * @param adapterName Name of the adapter to open (e.g., "hci0")
   * @param adapter Pointer to store the opened adapter handle
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using AdapterOpenFn =
    int (*)(const char *adapterName, gattlib_adapter_t **adapter);

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
   * @param userData User data to pass to the callback
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using AdapterScanEnableFn = int (*)(
    gattlib_adapter_t *adapter, gattlib_discovered_device_t callback,
    size_t timeout, void *userData
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

  /**
   * @brief Function pointer type for connecting to a BLE device
   *
   * Corresponds to gattlib_connect() in the gattlib API.
   *
   * @param adapter The adapter to use
   * @param dst The address of the device to connect to
   * @param timeout Connection timeout in milliseconds
   * @param connectCb Callback to invoke on connect
   * @param userData User data passed to the callback
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using ConnectFn = int (*)(gattlib_adapter_t *adapter,
                            const char *dst,
                            unsigned long options,
                            gatt_connect_cb_t connectCb,
                            void *userData);

  /**
   * @brief Function pointer type for disconnecting from a BLE device
   *
   * Corresponds to gattlib_disconnect() in the gattlib API.
   *
   * @param connection The BLE connection to disconnect
   * @param wait Whether to wait for the disconnect to complete
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using DisconnectFn = int (*)(gattlib_connection_t *connection, bool wait);

  /**
   * @brief Function pointer type for registering a disconnect callback
   *
   * Corresponds to gattlib_register_on_disconnect() in the gattlib API.
   *
   * @param connection The BLE connection
   * @param disconnectionHandler Callback to invoke on disconnect
   * @param userData User data passed to the callback
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  using RegisterOnDisconnectFn = int (*)(gattlib_connection_t *connection,
                                         gattlib_disconnection_handler_t disconnectionHandler,
                                         void *userData);

  /// Function to open a Bluetooth adapter
  /// NOLINTNEXTLINE
  AdapterOpenFn adapterOpen {gattlib_adapter_open};
  /// Function to close a Bluetooth adapter
  /// NOLINTNEXTLINE
  AdapterCloseFn adapterClose {gattlib_adapter_close};
  /// Function to enable Bluetooth scanning
  /// NOLINTNEXTLINE
  AdapterScanEnableFn adapterScanEnable {gattlib_adapter_scan_enable};
  /// Function to disable Bluetooth scanning
  /// NOLINTNEXTLINE
  AdapterScanDisableFn adapterScanDisable {gattlib_adapter_scan_disable};
  /// Function to wait for scan to stop
  /// NOLINTNEXTLINE
  AdapterWaitScanStoppedFn adapterWaitScanStopped {
    gattlib_adapter_wait_scan_stopped
  };
  /// Function to connect to a BLE device
  /// NOLINTNEXTLINE
  ConnectFn connect {gattlib_connect};
  /// Function to disconnect from a BLE device
  /// NOLINTNEXTLINE
  DisconnectFn disconnect {gattlib_disconnect};
  /// Function to register a disconnect callback
  /// NOLINTNEXTLINE
  RegisterOnDisconnectFn registerOnDisconnect {gattlib_register_on_disconnect};

  /**
   * @brief Default constructor
   *
   * Initializes all function pointers to nullptr.
   * You must set all function pointers before using this object.
   */
  GattlibFunctions() = default;

  /**
   * @brief Checks if all function pointers are set
   *
   * @return true if all function pointers are non-null, false otherwise
   */
  [[nodiscard]] bool isComplete() const {
    return adapterOpen != nullptr && adapterClose != nullptr &&
           adapterScanEnable != nullptr && adapterScanDisable != nullptr &&
           adapterWaitScanStopped != nullptr && connect != nullptr &&
           disconnect != nullptr && registerOnDisconnect != nullptr;
  }
};

} // namespace blecpp
