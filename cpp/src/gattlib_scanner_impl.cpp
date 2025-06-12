/**
 * @file gattlib_scanner_impl.cpp
 * @brief Implementation of GattlibScanner and its PIMPL class.
 */

#include "gattlib_scanner_impl.hpp"

#include "gattlib_scanner.hpp"
#include "gattlib_utils.hpp"
#include "log_macros.hpp"

#include <future>
// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
// NOLINTEND(*)

namespace blecpp {

// Public interface implementation
GattlibScanner::GattlibScanner(const GattlibFunctions &functions) {
  m_pimpl = std::make_unique<Impl>(functions);
}

// Public interface implementation
GattlibScanner::GattlibScanner(
  gattlib_adapter_t *adapterPtr, const GattlibFunctions &functions
) {
  m_pimpl = std::make_unique<Impl>(adapterPtr, functions);
}

// Public interface implementation
GattlibScanner::~GattlibScanner() = default;

// Public interface implementation
int GattlibScanner::scan(
  uint32_t timeoutSec, const std::optional<std::string> &deviceAddress
) {
  return m_pimpl->scan(timeoutSec, deviceAddress);
}

bool GattlibScanner::isScanning() const { return m_pimpl->isScanning(); }

void GattlibScanner::abort() { m_pimpl->abort(); }

// PIMPL implementation
GattlibScanner::Impl::Impl(const GattlibFunctions &functions)
  : m_gmainLoopManager(std::make_shared<GMainLoopManager>()),
    m_gattlibFunctions(functions), m_scanning(false),
    m_abort(std::make_shared<std::atomic<bool>>(false)) {

  if (!functions.isComplete()) {
    throw std::runtime_error("GattlibFunctions is not fully initialized");
  }

  if (!m_gmainLoopManager->start()) {
    throw std::runtime_error("Failed to start GLib main loop");
  }

  // Create adapter after GLib loop is running
  int ret = m_gattlibFunctions.adapterOpen(nullptr, &m_adapterPtr);
  if (ret != GATTLIB_SUCCESS || m_adapterPtr == nullptr) {
    m_gmainLoopManager->stop();
    throw std::runtime_error("Failed to open BLE adapter");
  }
}

GattlibScanner::Impl::Impl(
  gattlib_adapter_t *adapterPtr, const GattlibFunctions &functions
)
  : m_gmainLoopManager(nullptr), m_adapterPtr(adapterPtr),
    m_gattlibFunctions(functions), m_scanning(false),
    m_abort(std::make_shared<std::atomic<bool>>(false)) {

  if (m_adapterPtr == nullptr || !functions.isComplete()) {
    throw std::invalid_argument("Constructor parameters cannot be null");
  }
}

GattlibScanner::Impl::~Impl() { abort(); }

void GattlibScanner::Impl::abort() {
  // Set abort flag to prevent new callbacks
  try {
    if (m_abort) {
      // The scan context data has a pointer pointing to this abort flag. Hence
      // changinging this flag will be visible to the ScanContext.
      // This will cause that during on discovery the callback set the state to
      // ABORTED and notify the condition variable. Then, the shutdown process
      // will be triggered.
      m_abort->store(true);
      // Wait for scanning to finish
      while (m_scanning.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        BLECPP_LOG_WARN("Waiting for scan to be destroyed");
      }
    }
  } catch (const std::exception &e) {
    BLECPP_LOG_ERROR("Failed to abort scan: {}", e.what());
  }
}

bool GattlibScanner::Impl::isScanning() const {
  // Check if adapter is running use gattlib_adapter_get_name(adapter_ptr_)
  return m_scanning.load();
}

int GattlibScanner::Impl::scan(
  uint32_t timeoutSec, const std::optional<std::string> &deviceAddress
) {
  BLECPP_LOG_INFO("Scanning for {} seconds", timeoutSec);
  if (m_scanning.load()) {
    BLECPP_LOG_ERROR("Scan is already in progress");
    return GATTLIB_BUSY;
  }
  if (timeoutSec == 0) {
    BLECPP_LOG_ERROR("Timeout must be greater than 0");
    return GATTLIB_INVALID_PARAMETER;
  }

  int scanReturnCode = GATTLIB_SUCCESS;

  // ScanContext is owned by this function and must be deleted before returning
  ScanContext *scanData = nullptr;
  try {
    scanData = new ScanContext(); // NOLINT (cppcoreguidelines-owning-memory)
    scanData->timeout = timeoutSec;
    scanData->deviceFilter = deviceAddress;
    scanData->adapterPtr = m_adapterPtr;
    scanData->state = ScanState::SCANNING;
    scanData->abort = m_abort;
    BLECPP_LOG_INFO(
      "Scan filter: {}", deviceAddress.value_or("scanning all devices")
    );

    // Create a future to monitor the scan state
    auto monitorFuture = std::async(std::launch::async, [this, scanData]() {
      // Only abort if it's scanning otherwise it means it's shutting down, or
      // failed, succeeded or aborted
      while (m_scanning.load()) {
        if (m_abort->load()) {
          std::lock_guard<std::mutex> lock(scanData->mtx);
          scanData->state.store(ScanState::ABORTED);
          scanData->cv.notify_all();
          break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });

    m_scanning.store(true);
    scanReturnCode = m_gattlibFunctions.adapterScanEnable(
      m_adapterPtr, onScanDiscovery, scanData->timeout, scanData
    );
    if (scanReturnCode != GATTLIB_SUCCESS) {
      BLECPP_LOG_ERROR(
        "Failed to enable scan: {}", gattLibErrorToString(scanReturnCode)
      );
      scanData->state.store(ScanState::FAILED);
      shutdown(scanData);
      return scanReturnCode;
    }

    static constexpr int kScanTimeoutBufferSec = 5;
    auto timeoutCondition =
      std::chrono::seconds(timeoutSec + kScanTimeoutBufferSec);
    BLECPP_LOG_INFO(
      "Waiting for scan to complete timeout: {}", timeoutCondition.count()
    );

    bool timedOut = false;
    std::atomic<ScanState> finalState;
    {
      std::unique_lock<std::mutex> lock(scanData->mtx);
      timedOut = !scanData->cv.wait_for(
        lock, timeoutCondition,
        [scanData, &finalState]() {
          finalState.store(scanData->state.load());
          return finalState == ScanState::SUCCEEDED ||
                 finalState == ScanState::FAILED ||
                 finalState == ScanState::ABORTED;
        }
      );
      BLECPP_LOG_INFO(
        "Finished waiting - Scan state: {}, timed out: {}",
        toString(finalState), timedOut
      );
    }
    shutdown(scanData);

    if (timedOut) {
      BLECPP_LOG_INFO("Scan timed out");
      scanReturnCode = GATTLIB_TIMEOUT;
    } else if (finalState == ScanState::SUCCEEDED) {
      BLECPP_LOG_INFO("Scan succeeded");
      scanReturnCode = GATTLIB_SUCCESS;
    } else if (finalState == ScanState::ABORTED) {
      BLECPP_LOG_INFO("Scan aborted");
      scanReturnCode = GATTLIB_UNEXPECTED;
    } else {
      BLECPP_LOG_INFO("Scan failed");
      scanReturnCode = GATTLIB_DEVICE_ERROR;
    }
  } catch (const std::exception &e) {
    BLECPP_LOG_ERROR("Scan failed: {}", e.what());
    shutdown(scanData);
    throw;
  }
  return scanReturnCode;
}

void GattlibScanner::Impl::onScanDiscovery(
  [[maybe_unused]] gattlib_adapter_t *adapter, const char *addr,
  [[maybe_unused]] const char *name, void *userData
) {
  if (userData == nullptr) {
    BLECPP_LOG_ERROR("User data is null");
    throw std::runtime_error("Fatal error: User data cannot be null - crash");
  }

  auto *scanData = static_cast<ScanContext *>(userData);
  if (adapter == nullptr) {
    BLECPP_LOG_ERROR("Adapter is null");
    {
      std::lock_guard<std::mutex> lock(scanData->mtx);
      scanData->state.store(ScanState::FAILED);
      scanData->cv.notify_all();
    }
    return;
  }

  // Always print out the device info.
  BLECPP_LOG_INFO("Discovered device: {} ({})", addr, name ? name : "unnamed");

  if (scanData->deviceFilter.has_value() &&
      scanData->deviceFilter.value() == addr) {
    BLECPP_LOG_INFO("Device match filter: {}", addr);
    {
      std::lock_guard<std::mutex> lock(scanData->mtx);
      scanData->state.store(ScanState::SUCCEEDED);
      scanData->cv.notify_all();
    }
    return;
  }
}

void GattlibScanner::Impl::cleanup(ScanContext *scanCtx) {
  BLECPP_LOG_INFO("Cleaning up scan");
  if (scanCtx == nullptr) {
    BLECPP_LOG_ERROR("Scan context is null");
    return;
  }

  if (scanCtx->state != ScanState::SHUTTING_DOWN) {
    BLECPP_LOG_ERROR("Scan is cleaning without being shut down");
    throw std::runtime_error("Scan is cleaning without being shut down - crash"
    );
  }

  delete scanCtx; // NOLINT (cppcoreguidelines-owning-memory)
}

void GattlibScanner::Impl::shutdown(ScanContext *scanCtx) {
  BLECPP_LOG_INFO("Shutting down scan");
  if (scanCtx == nullptr) {
    BLECPP_LOG_ERROR("Scan context is null");
    throw std::runtime_error("Fatal error: Scan context is null During shutdown"
    );
  }

  {
    std::lock_guard<std::mutex> lock(scanCtx->mtx);
    if (scanCtx->state == ScanState::SHUTTING_DOWN) {
      BLECPP_LOG_INFO("Scan is already shutting down or in different state");
      return;
    }
  }

  {
    std::lock_guard<std::mutex> shutdownLock(m_shutdownMutex);
    scanCtx->state = ScanState::SHUTTING_DOWN;

    if (!m_scanning.load()) {
      BLECPP_LOG_INFO("Scan is not running");
      throw std::runtime_error("Fatal error: Scan is not running beforeshutdown");
    }

    auto result = m_gattlibFunctions.adapterScanDisable(m_adapterPtr);
    if (result != GATTLIB_SUCCESS) {
      BLECPP_LOG_ERROR("Failed to disable scan: {}", result);
    }

    m_gattlibFunctions.adapterWaitScanStopped(m_adapterPtr);

    if (m_adapterOwned && m_adapterPtr != nullptr) {
      int ret = m_gattlibFunctions.adapterClose(m_adapterPtr);
      if (ret != GATTLIB_SUCCESS) {
        BLECPP_LOG_ERROR("Failed to close adapter: {}", ret);
      }
      m_adapterPtr = nullptr;
    }

    if (m_gmainLoopManager) {
      m_gmainLoopManager->stop();
    }

    cleanup(scanCtx);
    m_scanning.store(false);
  }
}

} // namespace blecpp