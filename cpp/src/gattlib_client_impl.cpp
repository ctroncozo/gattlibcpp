/**
 * @file gattlib_client.cpp
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-03
 *
 * @brief Implementation of the GattlibClient class.
 *
 * This file contains the implementation of the GattlibClient class and its
 * supporting types. It provides thread-safe connection management for BLE
 * devices using the gattlib library.
 */

#include "gattlib_client_impl.hpp"

// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
#include <cstdint>
// NOLINTEND(*)
#include "gattlib_utils.hpp"
#include "log_macros.hpp"

namespace blecpp {

GattlibClient::GattlibClient(const GattlibFunctions &functions)
  : m_pimpl(std::make_unique<Impl>(functions)) {}

// Connect
int GattlibClient::connect(int32_t timeout, const std::string &address) {
  return m_pimpl->connect(timeout, address);
}

// PIMPL implementation requires destructor definition in cpp file
GattlibClient::~GattlibClient() = default;

// Disconnect
int GattlibClient::disconnect() { return m_pimpl->disconnect(); }

// Shutdown
void GattlibClient::shutdown() { m_pimpl->shutdown(); }

// Check is connected
[[nodiscard]] bool GattlibClient::isConnected() const {
  return m_pimpl->isConnected();
}

// PIMPL implementation
GattlibClient::Impl::Impl(const GattlibFunctions &functions)
  : m_gmainLoopManager(std::make_shared<GMainLoopManager>()),
    m_gattlibFunctions(functions){
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
  // After the adapter is open, create the scanner
  m_scanner = std::make_shared<GattlibScanner>(m_adapterPtr, m_gattlibFunctions);
}

GattlibClient::Impl::~Impl() { shutdown(); }

int GattlibClient::Impl::connect(int32_t timeout, const std::string &address) {

  BLECPP_LOG_INFO("Connecting to {}", address);

  if (timeout == 0) {
    BLECPP_LOG_ERROR("Timeout must be greater than 0");
    return GATTLIB_INVALID_PARAMETER;
  }
  if (m_connectionContext != nullptr) {
    BLECPP_LOG_ERROR("Device is already connected");
    return GATTLIB_BUSY;
  }

  try {
    m_connectionContext = new ConnectionContext(); // NOLINT (owning-memory)
    m_connectionContext->address = address;
    m_connectionContext->adapterPtr = m_adapterPtr;
    m_connectionContext->attempts = timeout;
    m_connectionContext->state = ConnectionState::CONNECTING;
    m_connectionContext->connection = nullptr;
    m_connectionContext->functions = &m_gattlibFunctions;
    
  } catch (const std::exception &e) {
    BLECPP_LOG_ERROR("Failed to connect: {}", e.what());
    return GATTLIB_DEVICE_ERROR;
  }
  BLECPP_LOG_INFO("Connection context created");

  int scanReturnCode =
    m_scanner->scan(timeout, address, onDeviceDiscoverConnect);
  if (scanReturnCode != GATTLIB_SUCCESS) {
    BLECPP_LOG_ERROR(
      "Failed to scan: {}", gattLibErrorToString(scanReturnCode)
    );
    return scanReturnCode;
  }

  // Wait for the device to be discovered and connected, scanning timeout plus
  // a buffer of 5 seconds to account for the connection process.
  static constexpr int kScanTimeoutBufferSec = 5;
  auto timeoutCondition = std::chrono::seconds(timeout + kScanTimeoutBufferSec);
  BLECPP_LOG_INFO(
    "Waiting for device to be discovered and connected timeout: {}",
    timeoutCondition.count()
  );

  bool timedOut = false;
  std::atomic<ConnectionState> finalState;
  {
    std::unique_lock<std::mutex> lock(m_connectionContext->mutex);
    timedOut = !m_connectionContext->cv.wait_for(
      lock, timeoutCondition,
      [&finalState, this]() {
        finalState.store(m_connectionContext->state.load());
        return finalState == ConnectionState::CONNECTED ||
               finalState == ConnectionState::FAILED ||
               finalState == ConnectionState::ABORTED;
      }
    );
    BLECPP_LOG_INFO(
      "Finished waiting - Connection state: {}, timed out: {}",
      toString(finalState), timedOut
    );
  }

  int returnCode = GATTLIB_SUCCESS;
  if (timedOut) {
    BLECPP_LOG_ERROR("Connection timed out");
    returnCode = GATTLIB_TIMEOUT;
  } else if (finalState == ConnectionState::CONNECTED) {
    BLECPP_LOG_INFO("Device connected");
    returnCode = GATTLIB_SUCCESS;
  } else if (finalState == ConnectionState::FAILED) {
    BLECPP_LOG_ERROR("Device connection failed");
    delete m_connectionContext;
    m_connectionContext = nullptr;
    returnCode = GATTLIB_DEVICE_NOT_CONNECTED;
  } else if (finalState == ConnectionState::ABORTED) {
    BLECPP_LOG_ERROR("Device connection aborted");
    delete m_connectionContext;
    m_connectionContext = nullptr;
    returnCode = GATTLIB_DEVICE_ERROR;
  }

  return returnCode;
}

int GattlibClient::Impl::disconnect() {
  // Early return if no connection context exists
  if (m_connectionContext == nullptr) {
    BLECPP_LOG_DEBUG("No connection context exists, nothing to disconnect");
    return GATTLIB_DEVICE_NOT_CONNECTED;
  }
  
  {
    std::lock_guard<std::mutex> lock(m_connectionContext->mutex);
    // Check if we're already disconnected based on our internal state
    if (m_connectionContext->state.load() != ConnectionState::CONNECTED) {
      BLECPP_LOG_DEBUG("Device is not connected according to internal state, nothing to disconnect");
      return GATTLIB_DEVICE_NOT_CONNECTED;
    }

    BLECPP_LOG_INFO("Starting to disconnect from device");
    m_connectionContext->state.store(ConnectionState::SHUTTING_DOWN);

    // Verify that we have a valid connection pointer
    if (m_connectionContext->connection == nullptr) {
      BLECPP_LOG_WARN("Inconsistent state: connected flag is true but connection pointer is null");
      // We'll still continue with cleanup to ensure all resources are released
      // This helps prevent resource leaks in error cases
    } else {
      BLECPP_LOG_DEBUG("Disconnecting gattlib");
      // Use the function pointer from GattlibFunctions to disconnect
      // Pass true to wait for the disconnect to complete
      int ret = m_connectionContext->functions->disconnect(m_connectionContext->connection, true);
      if (ret != GATTLIB_SUCCESS) {
        BLECPP_LOG_ERROR("Failed to disconnect: {}", gattLibErrorToString(ret));
      } else {
        BLECPP_LOG_INFO("Successfully disconnected from BLE device");
        m_connectionContext->state.store(ConnectionState::DISCONNECTED);
      }
    }
  } // Release mutex before waiting
  
  // Wait for the disconnect callback to be processed
  // This ensures we don't delete the connection context while the callback is still using it
  static constexpr int kDisconnectTimeoutMs = 2000; // 2 seconds timeout
  bool disconnectCompleted = waitForDisconnect(kDisconnectTimeoutMs);
  
  if (!disconnectCompleted) {
    BLECPP_LOG_WARN("Disconnect operation did not complete within timeout, forcing cleanup");
  }
  
  // Clean up the connection context
  delete m_connectionContext;
  m_connectionContext = nullptr;
  
  BLECPP_LOG_INFO("Disconnected successfully");
  return GATTLIB_SUCCESS;
}

void GattlibClient::Impl::shutdown() {
  int ret = disconnect();
  if (ret != GATTLIB_SUCCESS) {
    BLECPP_LOG_ERROR("Failed to disconnect: {}", gattLibErrorToString(ret));
  }

  // If the scanner is still running, abort it
  m_scanner->abort();

  // Close adapter
  if(m_adapterPtr != nullptr){
    ret = m_gattlibFunctions.adapterClose(m_adapterPtr);
    if (ret != GATTLIB_SUCCESS) {
      BLECPP_LOG_ERROR("Failed to close adapter: {}", gattLibErrorToString(ret));
    }
    m_adapterPtr = nullptr;
  }

  if(m_gmainLoopManager){
    m_gmainLoopManager->stop();
  }
}


bool GattlibClient::Impl::waitForDisconnect(int timeoutMs) {
  if (m_connectionContext == nullptr) {
    BLECPP_LOG_DEBUG("No active connection to wait for");
    return true; // Already disconnected
  }

  BLECPP_LOG_INFO("Waiting for disconnect to complete, timeout: {} ms", timeoutMs);
  
  std::unique_lock<std::mutex> lock(m_connectionContext->mutex);
  bool disconnected = m_connectionContext->cv.wait_for(
    lock, 
    std::chrono::milliseconds(timeoutMs),
    [this]() {
      auto state = m_connectionContext->state.load();
      return state == ConnectionState::DISCONNECTED;
    }
  );

  if (disconnected) {
    BLECPP_LOG_INFO("Disconnect completed successfully");
  } else {
    BLECPP_LOG_WARN("Timed out waiting for disconnect, current state: {}", 
                    toString(m_connectionContext->state.load()));
  }

  return disconnected;
}

[[nodiscard]] bool GattlibClient::Impl::isConnected() const {
  if(m_connectionContext == nullptr){
    return false;
  }
  // Check both our internal state flag and the connection pointer
  bool isConnected = m_connectionContext->state.load() == ConnectionState::CONNECTED;
  BLECPP_LOG_DEBUG("Connection status: {}", isConnected ? "connected" : "disconnected");
  return isConnected;
}



void GattlibClient::Impl::onDeviceDiscoverConnect(
  gattlib_adapter_t *adapter, const char *addr, const char *name, void *userData
) {
  if (userData == nullptr) {
    BLECPP_LOG_ERROR("User data is null");
    throw std::runtime_error("Fatal error: User data cannot be null - crash");
  }

  auto *connectionData = static_cast<ConnectionContext *>(userData);
  if (adapter == nullptr) {
    BLECPP_LOG_ERROR("Adapter is null");
    {
      std::lock_guard<std::mutex> lock(connectionData->mutex);
      connectionData->state.store(ConnectionState::FAILED);
      connectionData->cv.notify_all();
    }
    return;
  }

  // Always print out the device info
  BLECPP_LOG_INFO(
    "GattlibClient: Discovered device: {} ({})", addr, name ? name : "unnamed"
  );


  auto connect = [](ConnectionContext *connectioCtx) {
    int ret{GATTLIB_SUCCESS};

    if (connectioCtx == nullptr) {
      BLECPP_LOG_ERROR("Connection context is null");
      throw std::runtime_error("Fatal error: Connection context cannot be null - crash");
    }

    BLECPP_LOG_DEBUG("GattlibClient: Attempting to connect to device {}", connectioCtx->address);
    {
      std::lock_guard<std::mutex> lock(connectioCtx->mutex);
      ret = connectioCtx->functions->connect(
        connectioCtx->adapterPtr, connectioCtx->address.c_str(),
        GATTLIB_CONNECTION_OPTIONS_NONE, &GattlibClient::Impl::onDeviceConnect,
        connectioCtx
      );
    }
    if (ret != GATTLIB_SUCCESS) {
      BLECPP_LOG_ERROR(
        "Failed to connect to the bluetooth device '{}'", connectioCtx->address
      );
      {
        std::lock_guard<std::mutex> lock(connectioCtx->mutex);
        connectioCtx->state.store(ConnectionState::FAILED);
        connectioCtx->cv.notify_all();
      }
    } else {
      BLECPP_LOG_DEBUG("GattlibClient: Connected to device {}", connectioCtx->address);
      {
        std::lock_guard<std::mutex> lock(connectioCtx->mutex);
        connectioCtx->state.store(ConnectionState::CONNECTED);
        connectioCtx->cv.notify_all();
      }
    }
    return;
  };
  connect(connectionData);
}

// TODO: review clang-tidy warnings NOLINTNEXTLINE
void GattlibClient::Impl::dumpConnectionData(ConnectionContext *connectionData
) {
  std::lock_guard<std::mutex> lock(connectionData->mutex);
  gattlib_connection_t *connection = connectionData->connection;
  int servicesCount = 0;

  char uuidStr[MAX_LEN_UUID_STR + 1]; // NOLINT

  // Raw pointer for gattlib C API
  gattlib_primary_service_t *services = nullptr;

  // NOLINTNEXTLINE
  int primaryServiceRet =
    gattlib_discover_primary(connection, &services, &servicesCount);

  if (primaryServiceRet == GATTLIB_SUCCESS && services != nullptr) {
    BLECPP_LOG_INFO("Discover primary services");
    int getServiceRet = GATTLIB_DEVICE_ERROR;
    for (int i = 0; i < servicesCount; i++) {
      getServiceRet = gattlib_uuid_to_string(
        &services[i].uuid /*NOLINT*/, uuidStr /*NOLINT*/, sizeof(uuidStr)
      );

      if (getServiceRet == GATTLIB_SUCCESS) {
        BLECPP_LOG_INFO(
          "service[{}], start_handle:{}, end_handle:{}, uuid:{}", i,
          services[i].attr_handle_start /*NOLINT*/, services[i].attr_handle_end,
          uuidStr
        );
      } else {
        BLECPP_LOG_ERROR(
          "Fail to get service uuid with error {}",
          gattLibErrorToString(getServiceRet)
        );
      }
    }
    if (services != nullptr) {
      free(services); // NOLINT
    }
  } else {
    if (services != nullptr) {
      free(services); // NOLINT
    }
    BLECPP_LOG_ERROR(
      "Fail to discover primary services with error {}",
      gattLibErrorToString(primaryServiceRet)
    );
    connectionData->state.store(ConnectionState::FAILED);
    return;
  }

  int characteristicsCount = 0;
  gattlib_characteristic_t *characteristics = nullptr;
  int characteristicsRet =
    gattlib_discover_char(connection, &characteristics, &characteristicsCount);

  if (characteristicsRet == GATTLIB_SUCCESS && characteristics != nullptr) {
    BLECPP_LOG_INFO("Discover characteristics");

    int getCharacteristicRet = GATTLIB_DEVICE_ERROR;
    for (int i = 0; i < characteristicsCount; i++) {
      getCharacteristicRet = gattlib_uuid_to_string(
        &characteristics[i].uuid /*NOLINT*/, uuidStr /*NOLINT*/,
        sizeof(uuidStr) /*NOLINT*/
      );
      if (getCharacteristicRet == GATTLIB_SUCCESS) {
        BLECPP_LOG_INFO(
          "characteristic[{}], properties:{}, value_handle:{}, uuid:{}", i,
          characteristics[i].properties /*NOLINT*/,
          characteristics[i].value_handle /*NOLINT*/, uuidStr /*NOLINT*/
        );
      } else {
        BLECPP_LOG_ERROR(
          "Fail to get characteristic uuid with error {}",
          gattLibErrorToString(getCharacteristicRet)
        );
      }
    }
    if (characteristics != nullptr) {
      free(characteristics); // NOLINT
    }
  } else {
    if (characteristics != nullptr) {
      free(characteristics); // NOLINT
    }
    BLECPP_LOG_ERROR(
      "Fail to discover characteristics {}",
      gattLibErrorToString(characteristicsRet)
    );
    connectionData->state.store(ConnectionState::FAILED);
    return;
  }
  BLECPP_LOG_INFO("Discover characteristics success");
}

void GattlibClient::Impl::onDeviceDisconnect(
  [[maybe_unused]] gattlib_connection_t *connection,
  [[maybe_unused]] void *userData
) {
  if(userData == nullptr){
    BLECPP_LOG_ERROR("User data is null during disconnect callback");
    return;
  }
  auto *connectionCtx = static_cast<ConnectionContext *>(userData);
  {
    std::lock_guard<std::mutex> lock(connectionCtx->mutex);
    connectionCtx->state.store(ConnectionState::DISCONNECTED);
    connectionCtx->cv.notify_all();
  }
  BLECPP_LOG_WARN("Device disconnected callback invoked.");
}

void GattlibClient::Impl::onDeviceConnect(
  gattlib_adapter_t *adapter, const char *dst, gattlib_connection_t *connection,
  int error, void *userData
) {
  if(userData == nullptr) {
    BLECPP_LOG_ERROR("User data is null");
    throw std::runtime_error("Fatal error: User data cannot be null - crash");
  }
  BLECPP_LOG_INFO(
    "Connection callback invoked with error {}", gattLibErrorToString(error)
  );

  bool connectionFailed = false;

  if (adapter == nullptr) {
    BLECPP_LOG_ERROR("Connection callback failed - gattlib_adapter_t is null");
    connectionFailed = true;
  }

  if (connection == nullptr) {
    BLECPP_LOG_ERROR("Connection callback failed - gattlib_connection_t is null"
    );
    connectionFailed = true;
  }

  if (error != GATTLIB_SUCCESS) {
    BLECPP_LOG_ERROR(
      "Connection callback failed with error {}", gattLibErrorToString(error)
    );
    connectionFailed = true;
  }

  auto *connectionCtx = static_cast<ConnectionContext *>(userData);
  if (connectionCtx->address != dst) {
    BLECPP_LOG_ERROR("Connection callback failed - address does not match");
    connectionFailed = true;
  }

  BLECPP_LOG_INFO("Connection callback - connection failed: {}", connectionFailed);
  {
    // Lock the connection mutex
    std::lock_guard<std::mutex> lock(connectionCtx->mutex);
    connectionCtx->connection = connection;
    // If no failures:
    // 1. set the state to connected
    // 2. register the disconnect callback
    if (!connectionFailed) {
      BLECPP_LOG_INFO("Connection callback success");
      connectionCtx->state.store(ConnectionState::CONNECTED);
      gattlib_register_on_disconnect(
        connectionCtx->connection,
        GattlibClient::Impl::onDeviceDisconnect,
        connectionCtx
      );
    } else {
      connectionCtx->state.store(ConnectionState::FAILED);
      BLECPP_LOG_ERROR("Connection callback failed");
    }
    BLECPP_LOG_INFO("Connection callback - notify all");
    connectionCtx->cv.notify_all();
  }


}

} // namespace blecpp