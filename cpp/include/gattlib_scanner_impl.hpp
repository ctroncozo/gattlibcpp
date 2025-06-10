/**
 * @file gattlib_scanner_impl.hpp
 * @brief Implementation details for GattlibScanner using PIMPL pattern.
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-10
 *
 * This file contains the private implementation of GattlibScanner, including:
 * - State management for BLE scanning
 * - C callback implementation for device discovery
 * - Synchronization primitives for thread safety
 *
 * The implementation is separated from the public interface to maintain ABI
 * stability and hide implementation details from clients.
 */

#pragma once

#include "gattlib_functions.hpp"
#include "gattlib_scanner.hpp"
#include "gmain_loop_manager.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace blecpp {

class GattlibScanner::Impl {
private:
  /**
   * @brief Enumerates the possible states of a BLE scan.
   */
  enum class ScanState {
    /// The scan is currently running
    SCANNING,
    /// The scan failed to start or encountered an error
    FAILED,
    /// Initial state before scanning and final state after cleanup
    IDLE,
    /// The scan has successfully completed
    SUCCEEDED,
    /// The scan was aborted
    ABORTED,
    /// The scan is being shut down
    SHUTTING_DOWN,
  };

  /**
   * @brief Holds state and synchronization primitives for BLE scanning.
   */
  struct ScanContext {
    /// Timeout for the scan in seconds
    uint32_t timeout;
    /// Optional MAC address filter
    std::optional<std::string> device_filter;
    /// Number of scan attempts remaining
    std::atomic<size_t> attempts {1};
    /// Pointer to the BLE adapter
    gattlib_adapter_t *adapter_ptr {nullptr};
    /// Mutex for thread-safe state access
    std::mutex mtx;
    std::condition_variable cv;
    /// Current scan state
    std::atomic<ScanState> state {ScanState::IDLE};
    /// Flag for scan abortion. This pointer allow C API callbacks to check if
    /// the scan was aborted, and vice versa, allows the scanner to know if the
    /// scan was aborted by any of the static methods or a callback.
    std::shared_ptr<std::atomic<bool>> abort {nullptr};
  };

public:
  /**
   * @brief Constructs a new GattlibScanner.
   */
  explicit Impl(std::shared_ptr<const GattlibFunctions> functions);

  /**
   * @brief Constructs a new GattlibScanner with a pre-initialized adapter.
   */
  Impl(
    gattlib_adapter_t *adapter_ptr,
    std::shared_ptr<const GattlibFunctions> functions
  );

  /**
   * @brief Destroys the GattlibScanner.
   */
  ~Impl();

  /**
   * @brief The scan method implementation. This method is called by the public
   *        scan method.
   * @return int Return a GATTLIB_* error code defined in gattlib.h
   */
  int scan(uint32_t timeout_sec, std::optional<std::string> device_address);

  /**
   * @brief Checks if a scan is currently in progress.
   * @return true if scanning, false otherwise
   */
  bool is_scanning() const;

  /**
   * @brief The abort method implementation.
   * @details This methos interrupts the scan operation and releases resources.
   * It is usefull when users want to interrupt the scan operation due to signal
   * received or any other reason.
   */
  void abort();

private:
  /**
   * @brief Callback function invoked when a BLE device is discovered.
   *
   * IMPORTANT: This is a synchronous callback from the gattlib C API.
   * The BLE stack waits for this callback to complete before processing
   * more events. This means:
   * 1. The callback must finish its work before returning
   * 2. State changes and logging happen immediately
   * 3. No new devices are discovered until this callback returns
   *
   * The callback handles these cases synchronously:
   * 1. Scan aborted: Updates state and notifies waiting threads
   * 2. Device found matching filter: Updates state and notifies waiting threads
   * 3. Device doesn't match filter: Logs and continues scanning
   *
   * Thread Safety:
   * - Uses minimal locking scope for state updates
   * - Only locks when updating shared state and notifying
   * - Atomic operations for abort flag
   *
   * @param adapter The BLE adapter that discovered the device
   * @param addr MAC address of the discovered device
   * @param name Name of the discovered device (may be null)
   * @param user_data Pointer to ScanContext for this scan operation
   */
  static void on_scan_discovery(
    gattlib_adapter_t *adapter, const char *addr, const char *name,
    void *user_data
  );

  /**
   * @brief Clean up resources associated with a scan operation.
   *
   * This function is responsible for safely cleaning up after a scan:
   * - Verifies scan context is valid
   * - Ensures scan is not still running
   * - Frees allocated memory and resets state
   *
   * @param scan_ctx The scan context to clean up
   * @throws std::runtime_error if cleanup is attempted while scan is still
   * running
   */
  void cleanup(ScanContext *scan_data);

  /**
   * @brief Perform a complete shutdown of a scan operation.
   *
   * This function handles the complete shutdown sequence for a scan:
   * 1. Update state to SHUTTING_DOWN (thread-safe)
   * 2. Disable BLE adapter scanning
   * 3. Wait for scanning to fully stop
   * 4. Close adapter if owned
   * 5. Stop GLib main loop if owned
   * 6. Clean up resources
   *
   * Thread Safety:
   * - Uses atomic operations for state changes
   * - Minimal lock scope for state updates
   * - Safe cleanup sequence to prevent resource leaks
   *
   * @param scan_ctx The scan context to shut down
   */
  void shutdown(ScanContext *scan_data);

  /**
   * @brief Convert a ScanState to a string.
   * @param state The ScanState to convert
   * @return The string representation of the state
   */
  static std::string to_string(ScanState state) {
    switch (state) {
    case ScanState::SCANNING:
      return "SCANNING";
    case ScanState::FAILED:
      return "FAILED";
    case ScanState::IDLE:
      return "IDLE";
    case ScanState::SUCCEEDED:
      return "SUCCEEDED";
    case ScanState::ABORTED:
      return "ABORTED";
    case ScanState::SHUTTING_DOWN:
      return "SHUTTING_DOWN";
    default:
      return "UNKNOWN";
    }
  }

private:
  /// GMainLoopManager for BLE operations
  std::shared_ptr<GMainLoopManager> gmain_loop_manager_ {nullptr};
  /// Pointer to the BLE adapter
  gattlib_adapter_t *adapter_ptr_ {nullptr};
  /// GattlibFunctions for gattlib operations
  std::shared_ptr<const GattlibFunctions> gattlib_functions_ {nullptr};
  /// Flag indicating if a scan is currently in progress
  std::atomic<bool> scanning_ {false};
  /// Shared flag to safely stop callbacks after Scanner destruction.
  /// Must be a shared_ptr to outlive the Scanner instance since C callbacks
  /// may still be active. Must be atomic for thread-safe access from callbacks.
  std::shared_ptr<std::atomic<bool>> abort_ {nullptr};
  /// Flag indicating if the adapter was opened by the scanner
  bool adapter_owned_ {false};
};

} // namespace blecpp
