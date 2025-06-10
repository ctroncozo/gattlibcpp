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

int gattlib_adapter_open(
  const char *adapter_name, gattlib_adapter_t **adapter
) {
  return MockGattlib::mock_adapter_open(adapter_name, adapter);
}

int gattlib_adapter_close(gattlib_adapter_t *adapter) {
  return MockGattlib::mock_adapter_close(adapter);
}

int gattlib_adapter_scan_enable(
  gattlib_adapter_t *adapter, gattlib_discovered_device_t callback,
  size_t timeout, void *user_data
) {
  return MockGattlib::mock_adapter_scan_enable(
    adapter, callback, timeout, user_data
  );
}

int gattlib_adapter_scan_disable(gattlib_adapter_t *adapter) {
  return MockGattlib::mock_adapter_scan_disable(adapter);
}

void gattlib_adapter_wait_scan_stopped(gattlib_adapter_t *adapter) {
  MockGattlib::mock_adapter_wait_scan_stopped(adapter);
}
}
