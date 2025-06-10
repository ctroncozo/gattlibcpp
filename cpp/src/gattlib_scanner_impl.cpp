/**
 * @file gattlib_scanner_impl.cpp
 * @brief Implementation of GattlibScanner and its PIMPL class.
 */

#include "gattlib_scanner_impl.hpp"

#include "gattlib_scanner.hpp"
#include "gattlib_utils.hpp"
#include "log_macros.hpp"

#include <gattlib.h>

namespace blecpp {

// Public interface implementation
GattlibScanner::GattlibScanner(std::shared_ptr<const GattlibFunctions> functions
) {
  // Initialize gmain_loop_manager_ to nullptr
  if (!functions) {
    throw std::invalid_argument("GattlibFunctions cannot be null");
  }
  pimpl_ = std::make_unique<Impl>(functions);
}

// Public interface implementation
GattlibScanner::GattlibScanner(
  gattlib_adapter_t *adapter_ptr,
  std::shared_ptr<const GattlibFunctions> functions
) {
  if (!adapter_ptr || !functions) {
    throw std::invalid_argument("Constructor parameters cannot be null");
  }
  pimpl_ = std::make_unique<Impl>(adapter_ptr, functions);
}

// Public interface implementation
GattlibScanner::~GattlibScanner() = default;
GattlibScanner::GattlibScanner(GattlibScanner &&) noexcept = default;
GattlibScanner &GattlibScanner::operator=(GattlibScanner &&) noexcept = default;

// Public interface implementation
int GattlibScanner::scan(
  uint32_t timeout_sec, std::optional<std::string> device_address
) {
  return pimpl_->scan(timeout_sec, device_address);
}

bool GattlibScanner::is_scanning() const { return pimpl_->is_scanning(); }

void GattlibScanner::abort() { pimpl_->abort(); }

// PIMPL implementation
GattlibScanner::Impl::Impl(std::shared_ptr<const GattlibFunctions> functions)
  : gmain_loop_manager_(std::make_shared<GMainLoopManager>()),
    adapter_ptr_(nullptr), gattlib_functions_(functions), scanning_(false),
    abort_(std::make_shared<std::atomic<bool>>(false)) {

  if (!functions || !functions->is_complete()) {
    throw std::runtime_error("GattlibFunctions is not fully initialized");
  }

  if (!gmain_loop_manager_->start()) {
    throw std::runtime_error("Failed to start GLib main loop");
  }

  // Create adapter after GLib loop is running
  int ret = gattlib_functions_->adapter_open(nullptr, &adapter_ptr_);
  if (ret != GATTLIB_SUCCESS || !adapter_ptr_) {
    gmain_loop_manager_->stop();
    throw std::runtime_error("Failed to open BLE adapter");
  } else {
    adapter_owned_ = true;
  }
}

GattlibScanner::Impl::Impl(
  gattlib_adapter_t *adapter_ptr,
  std::shared_ptr<const GattlibFunctions> functions
)
  : gmain_loop_manager_(nullptr), adapter_ptr_(adapter_ptr),
    gattlib_functions_(functions), scanning_(false),
    abort_(std::make_shared<std::atomic<bool>>(false)) {

  if (!adapter_ptr_ || !functions) {
    throw std::invalid_argument("Constructor parameters cannot be null");
  }
}

GattlibScanner::Impl::~Impl() { abort(); }

void GattlibScanner::Impl::abort() {
  // Set abort flag to prevent new callbacks
  try {
    if (abort_) {
      // The scan context data has a pointer pointing to this abort flag. Hence
      // changinging this flag will be visible to the ScanContext.
      // This will cause that during on discovery the callback set the state to
      // ABORTED and notify the condition variable. Then, the shutdown process
      // will be triggered.
      abort_->store(true);
      // Wait for scanning to finish
      while (scanning_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        BLECPP_LOG_WARN("Waiting for scan to be destroyed");
      }
    }
  } catch (const std::exception &e) {
    BLECPP_LOG_ERROR("Failed to abort scan: {}", e.what());
  }
}

bool GattlibScanner::Impl::is_scanning() const {
  // Check if adapter is running use gattlib_adapter_get_name(adapter_ptr_)
  return scanning_.load();
}

int GattlibScanner::Impl::scan(
  uint32_t timeout_sec, std::optional<std::string> device_address
) {
  BLECPP_LOG_INFO("Scanning for {} seconds", timeout_sec);
  if (scanning_.load()) {
    BLECPP_LOG_ERROR("Scan is already in progress");
    return GATTLIB_BUSY;
  }
  if (timeout_sec == 0) {
    BLECPP_LOG_ERROR("Timeout must be greater than 0");
    return GATTLIB_INVALID_PARAMETER;
  }

  int scan_return_code = GATTLIB_SUCCESS;

  ScanContext *scan_data = nullptr;
  try {
    scan_data = new ScanContext();
    scan_data->timeout = timeout_sec;
    scan_data->device_filter = device_address;
    scan_data->adapter_ptr = adapter_ptr_;
    scan_data->state = ScanState::SCANNING;
    scan_data->abort = abort_;
    BLECPP_LOG_INFO(
      "Scan filter: {}", device_address.value_or("scanning all devices")
    );
    // Set scanning flag to true
    scanning_.store(true);
    scan_return_code = gattlib_functions_->adapter_scan_enable(
      adapter_ptr_, on_scan_discovery, scan_data->timeout, scan_data
    );
    if (scan_return_code != GATTLIB_SUCCESS) {
      BLECPP_LOG_ERROR(
        "Failed to enable scan: {}", gattLibErrorToString(scan_return_code)
      );
      scan_data->state.store(ScanState::FAILED);
      shutdown(scan_data);
      return scan_return_code;
    }
    // Wait for condition variable to be notified
    auto timeout_condition = std::chrono::seconds(timeout_sec + 5);
    BLECPP_LOG_INFO(
      "Waiting for scan to complete timeout: {}", timeout_condition.count()
    );

    bool timed_out = false;
    std::atomic<ScanState> final_state;
    {
      std::unique_lock<std::mutex> lock(scan_data->mtx);
      timed_out = !scan_data->cv.wait_for(
        lock, timeout_condition,
        [scan_data, &final_state]() {
          final_state.store(scan_data->state.load());
          return final_state == ScanState::SUCCEEDED ||
                 final_state == ScanState::FAILED ||
                 final_state == ScanState::ABORTED;
        }
      );
      BLECPP_LOG_INFO(
        "Finished waiting - Scan state: {}, timed out: {}",
        to_string(final_state), timed_out
      );
    }
    shutdown(scan_data);

    if (timed_out) {
      BLECPP_LOG_INFO("Scan timed out");
      scan_return_code = GATTLIB_TIMEOUT;
    } else if (final_state == ScanState::SUCCEEDED) {
      BLECPP_LOG_INFO("Scan succeeded");
      scan_return_code = GATTLIB_SUCCESS;
    } else {
      BLECPP_LOG_INFO("Scan failed");
      scan_return_code = GATTLIB_DEVICE_ERROR;
    }
  } catch (const std::exception &e) {
    BLECPP_LOG_ERROR("Scan failed: {}", e.what());
    shutdown(scan_data);
    throw;
  }
  return scan_return_code;
}

void GattlibScanner::Impl::on_scan_discovery(
  gattlib_adapter_t *adapter, const char *addr, const char *name,
  void *user_data
) {
  if (!user_data) {
    BLECPP_LOG_ERROR("User data is null");
    throw std::runtime_error("Fatal error: User data cannot be null - crash");
  }

  ScanContext *scan_data = static_cast<ScanContext *>(user_data);
  if (!adapter) {
    BLECPP_LOG_ERROR("Adapter is null");
    {
      std::lock_guard<std::mutex> lock(scan_data->mtx);
      scan_data->state.store(ScanState::FAILED);
      scan_data->cv.notify_all();
    }
    return;
  }

  // Always print out the device info.
  BLECPP_LOG_INFO("Discovered device: {} ({})", addr, name ? name : "unnamed");

  // Check if scan was aborted
  if (scan_data->abort->load()) {
    BLECPP_LOG_INFO("Scan aborted");
    {
      // Minimal lock scope for state update and notification
      std::lock_guard<std::mutex> lock(scan_data->mtx);
      scan_data->state.store(ScanState::ABORTED);
      scan_data->cv.notify_all();
    }
    return;
  }

  // Check if device matches our filter (if any)
  if (scan_data->device_filter.has_value() &&
      scan_data->device_filter.value() == addr) {
    BLECPP_LOG_INFO("Device match filter: {}", addr);
    {
      // Minimal lock scope for state update and notification
      std::lock_guard<std::mutex> lock(scan_data->mtx);
      scan_data->state.store(ScanState::SUCCEEDED);
      scan_data->cv.notify_all();
    }
    return;
  }
}

void GattlibScanner::Impl::cleanup(ScanContext *scan_ctx) {
  BLECPP_LOG_INFO("Cleaning up scan");
  if (!scan_ctx) {
    BLECPP_LOG_ERROR("Scan context is null");
    return;
  }

  // Safety check: never cleanup while scanning
  if (scan_ctx->state != ScanState::SHUTTING_DOWN) {
    BLECPP_LOG_ERROR("Scan is cleaning without being shut down");
    throw std::runtime_error("Scan is cleaning without being shut down - crash"
    );
  }

  // Free allocated memory
  delete scan_ctx;
}

void GattlibScanner::Impl::shutdown(ScanContext *scan_ctx) {
  BLECPP_LOG_INFO("Shutting down scan");
  if (!scan_ctx) {
    BLECPP_LOG_ERROR("Scan context is null");
    throw std::runtime_error("Fatal error: Scan context is null During shutdown"
    );
  }

  // Lock the for the duration of the shutdown process
  std::lock_guard<std::mutex> lock(scan_ctx->mtx);
  // 1. Atomically set state to shutting down if not already
  if (scan_ctx->state == ScanState::SHUTTING_DOWN) {
    BLECPP_LOG_INFO("Scan is already shutting down or in different state");
    return;
  }
  scan_ctx->state = ScanState::SHUTTING_DOWN;

  if (!scanning_.load()) {
    BLECPP_LOG_INFO("Scan is not running");
    throw std::runtime_error("Fatal error: Scan is not running beforeshutdown");
  }
  // 2. Disable the adapter
  BLECPP_LOG_INFO("Disabling scan");
  auto result = gattlib_functions_->adapter_scan_disable(adapter_ptr_);
  if (result != GATTLIB_SUCCESS) {
    BLECPP_LOG_ERROR("Failed to disable scan: {}", result);
  }

  // 3. Wait for the adapter to disable scanning
  BLECPP_LOG_INFO("Waiting for scan to stop");
  gattlib_functions_->adapter_wait_scan_stopped(adapter_ptr_);

  // 4. It can only close the adapter if it was opened by the scaner
  BLECPP_LOG_INFO("Closing adapter");
  if (adapter_owned_ && adapter_ptr_) {
    int ret = gattlib_functions_->adapter_close(adapter_ptr_);
    if (ret != GATTLIB_SUCCESS) {
      BLECPP_LOG_ERROR("Failed to close adapter: {}", ret);
    }
    adapter_ptr_ = nullptr;
  }

  // 5. Stop the gmain loop if we own it
  BLECPP_LOG_INFO("Stopping gmain loop");
  if (gmain_loop_manager_) {
    gmain_loop_manager_->stop();
  }

  // 6. Call cleanup to release resources
  cleanup(scan_ctx);

  scanning_.store(false);
}

} // namespace blecpp