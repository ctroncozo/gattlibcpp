/**
 * @file gattlib_tests.cpp
 * @brief Unit tests for the GattlibScanner class.
 *
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-09
 *
 * This file provides a set of unit tests for the GattlibScanner class,
 * which is a wrapper around the gattlib library for Bluetooth Low Energy
 * device scanning.
 */

#include "gattlib_scanner.hpp"
#include "gmain_loop_manager.hpp"
#include "log_macros.hpp"
#include "mock_gattlib.hpp"

#include <gattlib.h>

#include <csignal>
#include <future>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;

/**
 * @brief Test fixture for GattlibScanner tests.
 *
 * This fixture sets up the necessary dependencies for testing the BLE scanner:
 * - A mock gattlib implementation
 * - A fake BLE adapter
 * - GattlibFunctions wrapper around the mock
 * - GMainLoopManager for handling the GLib event loop
 */
class GattlibScannerWithGMainLoop : public ::testing::Test {
protected:
  /**
   * @brief Set up the test environment before each test.
   *
   * Creates and initializes:
   * - Mock gattlib implementation
   * - Fake adapter pointer (just a non-null value)
   * - GattlibFunctions wrapper with the mock
   * - GMainLoopManager and starts it
   */
  void SetUp() override {
    mock_ = &MockGattlib::getInstance();
    fake_adapter_ = reinterpret_cast<gattlib_adapter_t *>(0x1234);
    functions_ =
      std::make_shared<const blecpp::GattlibFunctions>(mock_->getMockFunctions()
      );
    loop_manager_ = std::make_shared<blecpp::GMainLoopManager>();
    if (!loop_manager_->start()) {
      throw std::runtime_error("Failed to start GMainLoop");
    }
  }

  /**
   * @brief Clean up after each test.
   *
   * - Stops the GMainLoopManager
   * - Verifies and clears mock expectations
   */
  void TearDown() override {
    // Stop the GMainLoop
    if (loop_manager_) {
      loop_manager_->stop();
      loop_manager_.reset();
    }
    testing::Mock::VerifyAndClearExpectations(mock_);
  }

  /// Mock gattlib implementation
  MockGattlib *mock_;
  /// Fake BLE adapter pointer
  gattlib_adapter_t *fake_adapter_;
  /// Gattlib function wrapper
  std::shared_ptr<const blecpp::GattlibFunctions> functions_;
  /// GLib event loop manager
  std::shared_ptr<blecpp::GMainLoopManager> loop_manager_;

  /// Callback storage for tests
  void (*saved_callback_)(gattlib_adapter_t *, const char *, const char *, void *);
  void *saved_user_data_;
};

/**
 * @brief Test basic BLE scanning failure
 * @details Creates a Scanner with a fake adapter and without an
 * on_scan_discovery. It calls the scan method with a time out.
 * Check the scanner is not scanning at the end of the timeout.
 */
TEST_F(GattlibScannerWithGMainLoop, ShutdownTest) {
  // Create scanner with injected dependencies for better testability
  blecpp::GattlibScanner scanner(fake_adapter_, functions_);

  // Set up expectations for the complete scan lifecycle
  // Use InSequence to verify shutdown sequence happens in order
  testing::InSequence seq;

  // 1. Initial scan enable
  EXPECT_CALL(*mock_, adapter_scan_enable(fake_adapter_, _, 2, _))
    .WillOnce(Return(GATTLIB_SUCCESS));

  // 2. Scan disable during shutdown
  EXPECT_CALL(*mock_, adapter_scan_disable(fake_adapter_))
    .WillOnce(Return(GATTLIB_SUCCESS));

  // 3. Wait for scan to stop
  EXPECT_CALL(*mock_, adapter_wait_scan_stopped(fake_adapter_));

  // 4. Expect adapter_close is never called since we don't own it
  EXPECT_CALL(*mock_, adapter_close(fake_adapter_)).Times(Exactly(0));

  // Verify scanner handles scan correctly
  EXPECT_EQ(scanner.scan(2), GATTLIB_TIMEOUT);
  // Verify scanner is not scanning
  EXPECT_FALSE(scanner.is_scanning());
}

/**
 * @brief Tests the behavior of GattlibScanner when dealing with a null
 * adapter scenario. Let me break it down:
 * @details Starts a scan in the background using std::async since it's a
 * blocking operation Waits for scanning to begin Simulates three device
 * discoveries using the saved callback:
 * First device: "Metawear-1" (normal case)
 * Second device: "Metawear-2" (normal case)
 * Third device: "Metawear-3" (error case - uses nullptr as adapter)
 * Test Assertion: Expects the scan
 * operation to fail (EXPECT_NE(scan_future.get(), GATTLIB_SUCCESS)) due to the
 * null adapter in the third discovery
 */
TEST_F(GattlibScannerWithGMainLoop, AdapterNullTest) {
  blecpp::GattlibScanner scanner(fake_adapter_, functions_);

  // Expect the scan to be called and start successfully
  EXPECT_CALL(*mock_, adapter_scan_enable(fake_adapter_, _, 10, _))
    .WillOnce(DoAll(
      SaveArg<1>(&saved_callback_), SaveArg<3>(&saved_user_data_),
      Return(GATTLIB_SUCCESS)
    ));

  // Expect cleanup calls during shutdown
  EXPECT_CALL(*mock_, adapter_scan_disable(fake_adapter_))
    .WillOnce(Return(GATTLIB_SUCCESS));
  EXPECT_CALL(*mock_, adapter_wait_scan_stopped(fake_adapter_));
  EXPECT_CALL(*mock_, adapter_close(fake_adapter_)).Times(Exactly(0));

  // Start scan in background since it's blocking
  auto scan_future =
    std::async(std::launch::async, [&scanner]() { return scanner.scan(10); });

  // Wait for scan to start
  while (!scanner.is_scanning()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Simulate multiple device discoveries
  if (saved_callback_) {
    // First discovery - normal
    saved_callback_(
      fake_adapter_, "D4:28:C8:F3:7F:A1", "Metawear-1", saved_user_data_
    );
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Second discovery - normal
    saved_callback_(
      fake_adapter_, "D4:28:C8:F3:7F:A2", "Metawear-2", saved_user_data_
    );
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Third discovery - with null adapter, this should cause failure
    saved_callback_(
      nullptr, "D4:28:C8:F3:7F:A3", "Metawear-3", saved_user_data_
    );
  }

  // Verify scanner handles failure correctly
  EXPECT_NE(scan_future.get(), GATTLIB_SUCCESS);

  // Test but this time nullify the adapter from ouside the callback.
}

/**
 * @brief Tests the behavior of GattlibScanner when the adapter becomes null
 * during scanning
 * @details Similar to AdapterNullTest but simulates the adapter becoming null
 * during normal operation, rather than being passed as null to the callback.
 */
TEST_F(GattlibScannerWithGMainLoop, AdapterNullifiedTest) {
  blecpp::GattlibScanner scanner(fake_adapter_, functions_);

  // Expect the scan to be called and start successfully
  EXPECT_CALL(*mock_, adapter_scan_enable(fake_adapter_, _, 10, _))
    .WillOnce(DoAll(
      SaveArg<1>(&saved_callback_), SaveArg<3>(&saved_user_data_),
      Return(GATTLIB_SUCCESS)
    ));

  // Expect cleanup calls during shutdown
  EXPECT_CALL(*mock_, adapter_scan_disable(fake_adapter_))
    .WillOnce(Return(GATTLIB_SUCCESS));
  EXPECT_CALL(*mock_, adapter_wait_scan_stopped(fake_adapter_));
  EXPECT_CALL(*mock_, adapter_close(fake_adapter_)).Times(Exactly(0));

  // Start scan in background since it's blocking
  auto scan_future =
    std::async(std::launch::async, [&scanner]() { return scanner.scan(10); });

  // Wait for scan to start
  while (!scanner.is_scanning()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Simulate normal device discovery
  if (saved_callback_) {
    saved_callback_(
      fake_adapter_, "D4:28:C8:F3:7F:A1", "Metawear-1", saved_user_data_
    );
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Nullify the adapter directly
    fake_adapter_ = nullptr;

    // Try another discovery with the now-null adapter
    saved_callback_(
      fake_adapter_, "D4:28:C8:F3:7F:A2", "Metawear-2", saved_user_data_
    );
  }

  // Verify scanner handles failure correctly
  EXPECT_NE(scan_future.get(), GATTLIB_SUCCESS);
}

/**
 * @brief Tests that concurrent scan calls are rejected
 * @details Verifies that:
 * 1. Cannot start a new scan while one is in progress
 * 2. Multiple attempts to start concurrent scans all fail with GATTLIB_BUSY
 */
TEST_F(GattlibScannerWithGMainLoop, ConcurrentScansTest) {
  blecpp::GattlibScanner scanner(fake_adapter_, functions_);
  std::string mac_address = "D4:28:C8:F3:7F:A1";

  // Set up expectations for the main scan
  EXPECT_CALL(*mock_, adapter_scan_enable(fake_adapter_, _, 10, _))
    .WillOnce(DoAll(
      SaveArg<1>(&saved_callback_), SaveArg<3>(&saved_user_data_),
      Return(GATTLIB_SUCCESS)
    ));
  EXPECT_CALL(*mock_, adapter_scan_disable(fake_adapter_))
    .WillOnce(Return(GATTLIB_SUCCESS));
  EXPECT_CALL(*mock_, adapter_wait_scan_stopped(fake_adapter_));
  EXPECT_CALL(*mock_, adapter_close(fake_adapter_)).Times(Exactly(0));

  // Start the main scan in background
  auto main_scan = std::async(std::launch::async, [&scanner, mac_address]() {
    return scanner.scan(10, mac_address);
  });

  // Wait for scan to start
  while (!scanner.is_scanning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Try multiple concurrent scans while the main scan is running
  auto concurrent_scan1 =
    std::async(std::launch::async, [&scanner]() { return scanner.scan(10); });
  EXPECT_EQ(concurrent_scan1.get(), GATTLIB_BUSY);

  auto concurrent_scan2 =
    std::async(std::launch::async, [&scanner]() { return scanner.scan(10); });
  EXPECT_EQ(concurrent_scan2.get(), GATTLIB_BUSY);

  auto concurrent_scan3 =
    std::async(std::launch::async, [&scanner]() { return scanner.scan(10); });
  EXPECT_EQ(concurrent_scan3.get(), GATTLIB_BUSY);

  // Simulate device discovery and let main scan complete
  if (saved_callback_) {
    // First discovery
    saved_callback_(
      fake_adapter_, mac_address.c_str(), "Device-1", saved_user_data_
    );
  }

  EXPECT_EQ(main_scan.get(), GATTLIB_SUCCESS);
}

/**
 * @brief Tests that scanner aborts gracefully when receiving a signal
 * @details Verifies that:
 * 1. Scanner can be aborted via signal
 * 2. Resources are properly cleaned up in correct order
 * 3. Scan operation returns GATTLIB_SUCCESS after abort
 */
TEST_F(GattlibScannerWithGMainLoop, SignalAbortTest) {
  blecpp::GattlibScanner scanner(fake_adapter_, functions_);
  std::string mac_address = "D4:28:C8:F3:7F:A1";

  // Set up signal handler
  struct sigaction sa;
  sa.sa_handler = [](int) {
    // This is just a placeholder, actual abort happens in the test
  };
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  ASSERT_NE(sigaction(SIGUSR1, &sa, nullptr), -1)
    << "Failed to set up signal handler";

  // Set up expectations for the scan and shutdown sequence
  {
    // Use strict ordering to verify shutdown sequence
    testing::InSequence seq;

    // 1. Initial scan enable
    EXPECT_CALL(*mock_, adapter_scan_enable(fake_adapter_, _, 30, _))
      .WillOnce(DoAll(
        SaveArg<1>(&saved_callback_), SaveArg<3>(&saved_user_data_),
        Return(GATTLIB_SUCCESS)
      ));

    // 2. Scan disable during shutdown
    EXPECT_CALL(*mock_, adapter_scan_disable(fake_adapter_))
      .WillOnce(Return(GATTLIB_SUCCESS));

    // 3. Wait for scan to stop
    EXPECT_CALL(*mock_, adapter_wait_scan_stopped(fake_adapter_));

    // 4. Close adapter during shutdown
    EXPECT_CALL(*mock_, adapter_close(fake_adapter_)).Times(Exactly(0));
  }

  // Start scan in background
  auto scan_future = std::async(std::launch::async, [&]() {
    try {
      return scanner.scan(30, mac_address);
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Scan threw unexpected exception: " << e.what();
      return GATTLIB_DEVICE_ERROR;
    }
  });

  // Wait for scan to start
  while (!scanner.is_scanning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Simulate some device discoveries
  if (saved_callback_) {
    // First discovery - should continue scanning
    saved_callback_(
      fake_adapter_, "D4:28:C8:F3:7F:B1", "Other-Device", saved_user_data_
    );
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Launch abort in background
  auto abort_future = std::async(std::launch::async, [&]() {
    // Small delay to ensure scan is running
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // Send signal to self
    EXPECT_EQ(kill(getpid(), SIGUSR1), 0) << "Failed to send signal";
    // Call abort
    scanner.abort();
  });

  // Simulate discovery after abort signal - should trigger shutdown
  if (saved_callback_) {
    saved_callback_(
      fake_adapter_, mac_address.c_str(), "Target-Device", saved_user_data_
    );
  }

  // Wait for abort to complete
  abort_future.get();

  // Verify scan completes with success after abort
  EXPECT_EQ(scan_future.get(), GATTLIB_SUCCESS)
    << "Scan did not complete successfully after abort";

  // Verify scanner is no longer scanning
  EXPECT_FALSE(scanner.is_scanning())
    << "Scanner still reports as scanning after abort";
}

/**
 * @brief Tests scanning for specific MAC address
 * @details Verifies that:
 * 1. First 10 seconds discover wrong MAC
 * 2. Next 10 seconds discover correct MAC
 * 3. With 5 second timeout, second scan attempt should succeed
 */
TEST_F(GattlibScannerWithGMainLoop, ScanForMacTest) {
  blecpp::GattlibScanner scanner(fake_adapter_, functions_);
  std::string target_mac = "D4:28:C8:F3:7F:A1";
  std::string other_mac = "00:11:22:33:44:55";
  bool discovery_running = true;
  bool success = false;

  // Expect multiple scan attempts
  EXPECT_CALL(*mock_, adapter_scan_enable(fake_adapter_, _, 5, _))
    .Times(AtLeast(2))
    .WillRepeatedly(DoAll(
      SaveArg<1>(&saved_callback_), SaveArg<3>(&saved_user_data_),
      Return(GATTLIB_SUCCESS)
    ));
  EXPECT_CALL(*mock_, adapter_scan_disable(fake_adapter_))
    .Times(AtLeast(2))
    .WillRepeatedly(Return(GATTLIB_SUCCESS));
  EXPECT_CALL(*mock_, adapter_wait_scan_stopped(fake_adapter_))
    .Times(AtLeast(2));
  EXPECT_CALL(*mock_, adapter_close(fake_adapter_)).Times(Exactly(0));

  // Start discovery simulation thread
  auto discovery_thread = std::thread([&]() {
    auto start_time = std::chrono::steady_clock::now();

    while (discovery_running) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
          .count();

      // Stop after 20 seconds
      if (elapsed >= 20) {
        discovery_running = false;
        break;
      }

      if (saved_callback_) {
        if (elapsed < 10) {
          // First 10 seconds: discover wrong MAC
          saved_callback_(
            fake_adapter_, other_mac.c_str(), "Other-Device", saved_user_data_
          );
        } else {
          // 10-20 seconds: discover correct MAC
          saved_callback_(
            fake_adapter_, target_mac.c_str(), "Target-Device", saved_user_data_
          );
        }
      }

      // Sleep for 1 second between discoveries
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });

  // Keep scanning until success
  while (!success) {
    auto scan_future =
      std::async(std::launch::async, [&scanner, &target_mac]() {
        return scanner.scan(5, target_mac);
      });

    int result = scan_future.get();
    if (result == GATTLIB_SUCCESS) {
      success = true;
    } else {
      EXPECT_EQ(result, GATTLIB_TIMEOUT);
    }
  }

  // Wait for discovery thread
  if (discovery_thread.joinable()) {
    discovery_thread.join();
  }
}
