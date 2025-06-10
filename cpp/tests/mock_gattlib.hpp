/**
 * @file mock_gattlib.hpp
 * @brief Mock class for GattlibFunctions
 *
 * This file provides a mock class for GattlibFunctions, which can be used for
 * unit testing and dependency injection in production code.
 *
 * Example usage in a test:
 * @code
 * TEST(GattlibScannerTest, ScanForDevices) {
 *   // Get the mock instance
 *   auto& mock = MockGattlib::getInstance();
 *
 *   // Set up expectations
 *   EXPECT_CALL(mock, adapter_open(testing::_, testing::_))
 *     .WillOnce(testing::Return(GATTLIB_SUCCESS));
 *   EXPECT_CALL(mock, adapter_scan_enable(testing::_, testing::_, testing::_,
 * testing::_)) .WillOnce(testing::Return(GATTLIB_SUCCESS));
 *
 *   // Create scanner with mock functions
 *   auto functions = mock.getMockFunctions();
 *   GattlibScanner scanner(functions);
 *
 *   // Test the scanner
 *   EXPECT_TRUE(scanner.startScan());
 * }
 * @endcode
 */

#pragma once

#include "gattlib_functions.hpp"

#include <cstddef>

#include <gmock/gmock.h>

/**
 * @class MockGattlib
 * @brief Mock class for GattlibFunctions
 *
 * This class provides a mock implementation of the GattlibFunctions class,
 * which can be used for unit testing and dependency injection in production
 * code.
 */
class MockGattlib {
public:
  /**
   * @brief Get the singleton instance of the mock class
   *
   * This method returns a reference to the singleton instance of the mock
   * class. It uses the singleton pattern to ensure that only one instance of
   * the mock class is created and returned.
   *
   * @return Reference to the singleton instance of the mock class
   */
  static MockGattlib &getInstance() {
    static MockGattlib instance;
    return instance;
  }

  /**
   * @brief Google Mock methods matching open adapter function signature
   *
   * This method provides a mock implementation of the open adapter function,
   * which can be used for unit testing and dependency injection in production
   * code.
   *
   * @param adapter_name Name of the adapter to open (e.g., "hci0")
   * @param adapter Pointer to store the opened adapter handle
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  MOCK_METHOD(
    int, adapter_open, (const char *adapter_name, gattlib_adapter_t **adapter),
    ()
  );

  /**
   * @brief Google Mock methods matching close adapter function signature
   *
   * This method provides a mock implementation of the close adapter function,
   * which can be used for unit testing and dependency injection in production
   * code.
   *
   * @param adapter Handle to the adapter to close
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  MOCK_METHOD(int, adapter_close, (gattlib_adapter_t * adapter), ());

  /**
   * @brief Google Mock methods matching scan enable function signature
   *
   * This method provides a mock implementation of the scan enable function,
   * which can be used for unit testing and dependency injection in production
   * code.
   *
   * @param adapter Handle to the adapter to enable scanning on
   * @param callback Callback function to be called when a device is discovered
   * @param timeout Timeout in seconds for the scan operation
   * @param user_data User data to be passed to the callback function
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  MOCK_METHOD(
    int, adapter_scan_enable,
    (gattlib_adapter_t * adapter, gattlib_discovered_device_t callback,
     size_t timeout, void *user_data),
    ()
  );

  /**
   * @brief Google Mock methods matching scan disable function signature
   *
   * This method provides a mock implementation of the scan disable function,
   * which can be used for unit testing and dependency injection in production
   * code.
   *
   * @param adapter Handle to the adapter to disable scanning on
   * @return GATTLIB_SUCCESS on success, or a GATTLIB_* error code
   */
  MOCK_METHOD(int, adapter_scan_disable, (gattlib_adapter_t * adapter), ());

  /**
   * @brief Google Mock methods matching wait scan stopped function signature
   *
   * This method provides a mock implementation of the wait scan stopped
   * function, which can be used for unit testing and dependency injection in
   * production code.
   *
   * @param adapter Handle to the adapter to wait for scan to stop on
   */
  MOCK_METHOD(
    void, adapter_wait_scan_stopped, (gattlib_adapter_t * adapter), ()
  );

  /**
   * @brief Static mock functions for use as C function pointers
   *
   * These static functions provide a way to call the mock methods as C
   * function pointers, which can be used in production code to inject the
   * mock functions into the gattlib API.
   */
  static int
    mock_adapter_open(const char *adapter_name, gattlib_adapter_t **adapter) {
    return getInstance().adapter_open(adapter_name, adapter);
  }

  /**
   * @brief Static mock functions for use as C function pointers
   *
   * These static functions provide a way to call the mock methods as C
   * function pointers, which can be used in production code to inject the
   * mock functions into the gattlib API.
   */
  static int mock_adapter_close(gattlib_adapter_t *adapter) {
    return getInstance().adapter_close(adapter);
  }

  /**
   * @brief Static mock functions for use as C function pointers
   *
   * These static functions provide a way to call the mock methods as C
   * function pointers, which can be used in production code to inject the
   * mock functions into the gattlib API.
   */
  static int mock_adapter_scan_enable(
    gattlib_adapter_t *adapter, gattlib_discovered_device_t callback,
    size_t timeout, void *user_data
  ) {
    return getInstance().adapter_scan_enable(
      adapter, callback, timeout, user_data
    );
  }

  /**
   * @brief Static mock functions for use as C function pointers
   *
   * These static functions provide a way to call the mock methods as C
   * function pointers, which can be used in production code to inject the
   * mock functions into the gattlib API.
   */
  static int mock_adapter_scan_disable(gattlib_adapter_t *adapter) {
    return getInstance().adapter_scan_disable(adapter);
  }

  /**
   * @brief Static mock functions for use as C function pointers
   *
   * These static functions provide a way to call the mock methods as C
   * function pointers, which can be used in production code to inject the
   * mock functions into the gattlib API.
   */
  static void mock_adapter_wait_scan_stopped(gattlib_adapter_t *adapter) {
    getInstance().adapter_wait_scan_stopped(adapter);
  }

  /**
   * @brief Helper to get a GattlibFunctions struct with all mock functions
   *
   * This method returns a GattlibFunctions struct containing all the mock
   * functions, which can be used in production code to inject the mock
   * functions into the gattlib API.
   *
   * @return GattlibFunctions struct containing all mock functions
   */
  blecpp::GattlibFunctions getMockFunctions() {
    blecpp::GattlibFunctions fns;
    fns.adapter_open = &mock_adapter_open;
    fns.adapter_close = &mock_adapter_close;
    fns.adapter_scan_enable = &mock_adapter_scan_enable;
    fns.adapter_scan_disable = &mock_adapter_scan_disable;
    fns.adapter_wait_scan_stopped = &mock_adapter_wait_scan_stopped;
    return fns;
  }

private:
  MockGattlib() = default;
};
