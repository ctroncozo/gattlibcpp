/**
 * @file mock_gattlib.cpp
 * @brief Mock implementation of GattlibFunctions
 *
 * This file provides a mock implementation of the GattlibFunctions class,
 * which can be used for unit testing and dependency injection in production
 * code.
 */

#include "mock_gattlib.hpp"

// C-style functions that delegate to the mock instance
extern "C" {

int gattlibAdapterOpen(
  const char *adapterName, gattlib_adapter_t **adapter
) {
  return MockGattlib::mockAdapterOpen(adapterName, adapter);
}

int gattlibAdapterClose(gattlib_adapter_t *adapter) {
  return MockGattlib::mockAdapterClose(adapter);
}

int gattlibAdapterScanEnable(
  gattlib_adapter_t *adapter, gattlib_discovered_device_t callback,
  size_t timeout, void *userData
) {
  return MockGattlib::mockAdapterScanEnable(
    adapter, callback, timeout, userData
  );
}

int gattlibAdapterScanDisable(gattlib_adapter_t *adapter) {
  return MockGattlib::mockAdapterScanDisable(adapter);
}

int gattlibConnect(
  gattlib_adapter_t *adapter, const char *dst, unsigned long options,
  gatt_connect_cb_t connectCb, void *userData
) {
  return MockGattlib::mockConnect(adapter, dst, options, connectCb, userData);
  }

void gattlibAdapterWaitScanStopped(gattlib_adapter_t *adapter) {
  MockGattlib::mockAdapterWaitScanStopped(adapter);
}
}
