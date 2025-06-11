/**
 * @file gattlib_scanner.hpp
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-10
 *
 * @brief GattlibScanner: Scans for BLE devices using gattlib and GLib main
 * loop.
 *
 */

#pragma once

#include "gattlib_functions.hpp"

// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
// NOLINTEND(*)

#include <memory>
#include <optional>
#include <string>

namespace blecpp {

/**
 * @brief A synchronous BLE scanner using gattlib and GLib main loop.
 *
 * GattlibScanner provides a high-level interface for scanning BLE devices.
 * It uses the gattlib C API through GattlibFunctions for BLE operations
 * and GMainLoopManager for the GLib main loop.
 *
 * Key features:
 * - RAII-based resource management
 * - Support for targeted device scanning via address filtering
 * - Proper cleanup on destruction
 * - Exception-safe operation
 *
 * Usage example:
 * @code
 * auto scanner = GattlibScanner(functions);
 * scanner.scan(10); // Scan for 10 seconds
 * @endcode
 */
class GattlibScanner {
public:
  /**
   * @brief Constructs a new GattlibScanner.
   *
   * @details This constructor creates a new BLE adapter and GLib main loop
   * manger internally. The GMainLoopManager is started and the BLE adapter is
   * opened. The scanner will own and manage these resources throughout its
   * lifetime.
   *
   * @param functions Interface for gattlib operations (must not be null)
   * @throws std::invalid_argument if functions is null
   * @throws std::runtime_error if adapter creation fails
   */
  explicit GattlibScanner(const GattlibFunctions& functions);

  /**
   * @brief Constructs a new GattlibScanner with a pre-initialized adapter.
   *
   * @details The lifetime of the adapter is managed by the caller. The scanner
   * will use the adapter provided by the caller and will not take ownership of
   * it. The caller must ensure the adapter remains valid for the scanner's
   * lifetime.
   * The GLib main loop is managed by the caller, hence the scanner will not
   * interact with it in any way.
   *
   * @param adapter_ptr Pre-initialized BLE adapter (must not be null)
   * @param functions Interface for gattlib operations (must not be null)
   * @throws std::invalid_argument if any parameter is null
   */
  GattlibScanner(
    gattlib_adapter_t *adapterPtr,
    const GattlibFunctions& functions
  );

  /**
   * @brief Destructor.
   */
  ~GattlibScanner();

  /// Delete copy constructor
  GattlibScanner(const GattlibScanner &) = delete;
  /// Delete copy operator
  GattlibScanner &operator=(const GattlibScanner &) = delete;
  /// Delete moving constructor
  GattlibScanner(GattlibScanner &&) = delete;
  /// Delete moving operator
  GattlibScanner &operator=(GattlibScanner &&) = delete;

  /**
   * @brief Initiates a BLE scan.
   *
   * This method starts a Bluetooth Low Energy scan operation. The scan will run
   * for the specified timeout period, looking for all nearby BLE devices or a
   * specific device if an address is provided.
   *
   * @param timeout_sec Duration of the scan in seconds. Must be greater than 0.
   * @param device_address Optional MAC address of a specific BLE device to scan
   * for. If not provided (empty string), scans for all devices.
   *
   * @return GATTLIB_SUCCESS if scan started successfully
   *         GATTLIB_ERROR if scan fails to start or is already in progress
   *         GATTLIB_INVALID_PARAMETER if timeout_sec is 0
   *
   * @note This method works the same way regardless of how the scanner was
   * constructed (whether it owns the adapter/loop or uses externally provided
   * ones).
   * @return GATTLIB_DEVICE_ERROR if adapter is not available.
   * @return GATTLIB_BUSY_ERROR if scan is already in progress.
   * @return GATTLIB_INVALID_PARAMETER if timeout is 0.
   * @throw std::runtime_error if scan fails to start.
   */
  int scan(
    uint32_t timeoutSec, const std::optional<std::string>& deviceAddress = std::nullopt
  );

  /**
   * @brief Check if scanner is currently scanning
   * @return true if scanning, false otherwise
   */
  [[nodiscard]] bool isScanning() const;

  /**
   * @brief Abort an ongoing scan operation
   * @details This method safely interrupts an ongoing scan operation and
   * ensures all resources are properly cleaned up. It is useful when needing to
   * stop a scan in response to external events like signal interrupts.
   */
  void abort();

private:
  class Impl;
  std::unique_ptr<Impl> m_pimpl;
};

} // namespace blecpp