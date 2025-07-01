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
   * @enum ScanState
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
    std::optional<std::string> deviceFilter;
    /// Number of scan attempts remaining
    std::atomic<size_t> attempts {1};
    /// Pointer to the BLE adapter
    gattlib_adapter_t *adapterPtr {nullptr};
    /// Mutex for thread-safe state access
    std::mutex mtx;
    /// Condition variable for thread-safe state access
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
  explicit Impl(const GattlibFunctions &functions = GattlibFunctions());

  /**
   * @brief Constructs a new GattlibScanner with a pre-initialized adapter.
   */
  Impl(
    gattlib_adapter_t *adapterPtr,
    const GattlibFunctions &functions = GattlibFunctions()
  );

  /// Deep copy constructor deleted
  Impl(const Impl &) = delete;
  /// Copy operator deleted
  Impl &operator=(const Impl &) = delete;
  /// Move constructor deleted
  Impl(Impl &&) = delete;
  /// Move operator deleted
  Impl &operator=(Impl &&) = delete;

  /**
   * @brief Destroys the GattlibScanner.
   */
  ~Impl();

  /**
   * @brief The scan method implementation. This method is called by the public
   *        scan method.
   * @param timeSec Stop the scan after some time passed.
   * @param deviceAddress [optional] An mac address to scan for. If an address
   * is passed and the scan don't find it after timeout, it return error.
   * @param discoveredDeviceCb [optional] A callback function to be called when
   * a device is discovered. If not provided, the default callback will be used.
   * @return int Return a GATTLIB_* error code defined in gattlib.h
   */
  int scan(
    uint32_t timeoutSec, const std::optional<std::string> &deviceAddress,
    std::optional<gattlib_discovered_device_t> onDiscoveredDeviceCb
  );

  /**
   * @brief Checks if a scan is currently in progress.
   * @return true if scanning, false otherwise
   */
  [[nodiscard]] bool isScanning() const;

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
   * @param userData Pointer to ScanContext for this scan operation
   */
  static void onScanDiscovery(
    gattlib_adapter_t *adapter, const char *addr, const char *name,
    void *userData
  );

  /**
   * @brief Clean up resources associated with a scan operation.
   *
   * This function is responsible for safely cleaning up after a scan:
   * - Verifies scan context is valid
   * - Ensures scan is not still running
   * - Frees allocated memory and resets state
   *
   * @param scanCtx The scan context to clean up
   * @throws std::runtime_error if cleanup is attempted while scan is still
   * running
   */
  static void cleanup(ScanContext *scanCtx);

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
   * @param scanCtx The scan context to shut down
   */
  void shutdown(ScanContext *scanCtx);

  /**
   * @brief Convert a ScanState to a string.
   * @param state The ScanState to convert
   * @return The string representation of the state
   */
  static std::string toString(ScanState state) {
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

  /// GMainLoopManager for BLE operations
  std::shared_ptr<GMainLoopManager> m_gmainLoopManager {nullptr};
  /// Pointer to the BLE adapter
  gattlib_adapter_t *m_adapterPtr {nullptr};
  /// GattlibFunctions for gattlib operations
  const GattlibFunctions &m_gattlibFunctions;
  /// Flag indicating if a scan is currently in progress
  std::atomic<bool> m_scanning {false};
  /// Shared flag to safely stop callbacks after Scanner destruction.
  /// Must be a shared_ptr to outlive the Scanner instance since C callbacks
  /// may still be active. Must be atomic for thread-safe access from callbacks.
  std::shared_ptr<std::atomic<bool>> m_abort {nullptr};
  /// Flag indicating if the adapter was opened by the scanner
  bool m_adapterOwned {false};
  /// A shutdown mutex to prevent interruptions during shutdown
  std::mutex m_shutdownMutex;
};

} // namespace blecpp
