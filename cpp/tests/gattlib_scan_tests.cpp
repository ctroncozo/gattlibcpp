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
#include "mock_gattlib.hpp"

// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
// NOLINTEND(*)

#include <chrono>
#include <csignal>
#include <future>
#include <thread>

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
    m_mock = &MockGattlib::getInstance();
    // Create a fake adapter pointer using a properly sized allocation. Will be
    // cleaned up in TearDown
    m_fakeAdapter =
      static_cast<gattlib_adapter_t *>(::operator new(sizeof(void *)));

    // Set up mock to return success and set the adapter pointer
    EXPECT_CALL(*m_mock, adapter_open(testing::_, testing::_))
      .WillOnce(DoAll(SetArgPointee<1>(m_fakeAdapter), Return(GATTLIB_SUCCESS)));

    // Create adapter through the mock interface
    int result = MockGattlib::mockAdapterOpen("hci0", &m_fakeAdapter);

    EXPECT_EQ(result, GATTLIB_SUCCESS);
    EXPECT_NE(m_fakeAdapter, nullptr);

    m_functions =
      std::make_shared<blecpp::GattlibFunctions>(MockGattlib::getMockFunctions()
      );

    m_loopManager = std::make_shared<blecpp::GMainLoopManager>();
    if (!m_loopManager->start()) {
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
    if (m_loopManager) {
      m_loopManager->stop();
      m_loopManager.reset();
    }
    testing::Mock::VerifyAndClearExpectations(m_mock);
  }

  /// Mock gattlib implementation
  MockGattlib *m_mock; // NOLINT
  /// Fake BLE adapter pointer
  gattlib_adapter_t *m_fakeAdapter; // NOLINT
  /// Gattlib function wrapper
  std::shared_ptr<blecpp::GattlibFunctions> m_functions; // NOLINT
  /// GLib event loop manager
  std::shared_ptr<blecpp::GMainLoopManager> m_loopManager; // NOLINT

  /// Callback storage for tests
  gattlib_discovered_device_t m_savedCallback; // NOLINT
  void *m_savedUserData; // NOLINT

  /// Mutex to protect callback access
  std::mutex m_callbackMutex; // NOLINT
};

/**
 * @brief Test basic BLE scanning failure
 * @details Creates a Scanner with a fake adapter and without an
 * on_scan_discovery. It calls the scan method with a time out.
 * Check the scanner is not scanning at the end of the timeout.
 */
TEST_F(GattlibScannerWithGMainLoop, ShutdownTest) {
  // Create scanner with injected dependencies for better testability
  blecpp::GattlibScanner scanner(m_fakeAdapter, *m_functions);

  // Set up expectations for the complete scan lifecycle
  // Use InSequence to verify shutdown sequence happens in order
  testing::InSequence seq;

  // 1. Initial scan enable
  EXPECT_CALL(*m_mock, adapter_scan_enable(m_fakeAdapter, _, 2, _))
    .WillOnce(Return(GATTLIB_SUCCESS));

  // 2. Scan disable during shutdown
  EXPECT_CALL(*m_mock, adapter_scan_disable(m_fakeAdapter))
    .WillOnce(Return(GATTLIB_SUCCESS));

  // 3. Wait for scan to stop
  EXPECT_CALL(*m_mock, adapter_wait_scan_stopped(m_fakeAdapter));

  // 4. Expect adapter_close is never called since we don't own it
  EXPECT_CALL(*m_mock, adapter_close(m_fakeAdapter)).Times(Exactly(0));

  // Verify scanner handles scan correctly
  EXPECT_EQ(scanner.scan(2), GATTLIB_TIMEOUT);
  // Verify scanner is not scanning
  EXPECT_FALSE(scanner.isScanning());
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
  blecpp::GattlibScanner scanner(m_fakeAdapter, *m_functions);

  constexpr int kTenSeconds = 10;
  // Expect the scan to be called and start successfully
  EXPECT_CALL(*m_mock, adapter_scan_enable(m_fakeAdapter, _, kTenSeconds, _))
    .WillOnce(DoAll(
      SaveArg<1>(&m_savedCallback),
      SaveArg<3>(&m_savedUserData),
      Return(GATTLIB_SUCCESS)
    ));
  EXPECT_CALL(*m_mock, adapter_wait_scan_stopped(m_fakeAdapter));
  EXPECT_CALL(*m_mock, adapter_close(m_fakeAdapter)).Times(Exactly(0));

  // Start scan in background since it's blocking
  auto scanFuture = std::async(std::launch::async, [&scanner]() {
    return scanner.scan(kTenSeconds);
  });

  // Wait for scan to start
  while (!scanner.isScanning()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Simulate multiple device discoveries
  if (m_savedCallback != nullptr) {
    // First discovery - normal
    m_savedCallback(
      m_fakeAdapter, "D4:28:C8:F3:7F:A1", "Metawear-1", m_savedUserData
    );
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Second discovery - normal
    m_savedCallback(
      m_fakeAdapter, "D4:28:C8:F3:7F:A2", "Metawear-2", m_savedUserData
    );
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Third discovery - with null adapter, this should cause failure
    m_savedCallback(
      nullptr, "D4:28:C8:F3:7F:A3", "Metawear-3", m_savedUserData
    );
  }

  // Verify scanner handles failure correctly
  EXPECT_NE(scanFuture.get(), GATTLIB_SUCCESS);

  // Test but this time nullify the adapter from ouside the callback.
}

/**
 * @brief Tests the behavior of GattlibScanner when the adapter becomes null
 * during scanning
 * @details Similar to AdapterNullTest but simulates the adapter becoming null
 * during normal operation, rather than being passed as null to the callback.
 */
TEST_F(GattlibScannerWithGMainLoop, AdapterNullifiedTest) {
  blecpp::GattlibScanner scanner(m_fakeAdapter, *m_functions);

  constexpr int kTenSeconds = 10;
  // Expect the scan to be called and start successfully
  EXPECT_CALL(*m_mock, adapter_scan_enable(m_fakeAdapter, _, kTenSeconds, _))
    .WillOnce(DoAll(
      SaveArg<1>(&m_savedCallback), SaveArg<3>(&m_savedUserData),
      Return(GATTLIB_SUCCESS)
    ));
  EXPECT_CALL(*m_mock, adapter_wait_scan_stopped(m_fakeAdapter));
  EXPECT_CALL(*m_mock, adapter_close(m_fakeAdapter)).Times(Exactly(0));

  // Start scan in background since it's blocking
  auto scanFuture = std::async(std::launch::async, [&scanner]() {
    return scanner.scan(kTenSeconds);
  });

  // Wait for scan to start
  while (!scanner.isScanning()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Simulate normal device discovery
  if (m_savedCallback != nullptr) {
    m_savedCallback(
      m_fakeAdapter, "D4:28:C8:F3:7F:A1", "Metawear-1", m_savedUserData
    );
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Nullify the adapter directly
    m_fakeAdapter = nullptr;

    // Try another discovery with the now-null adapter
    m_savedCallback(
      m_fakeAdapter, "D4:28:C8:F3:7F:A2", "Metawear-2", m_savedUserData
    );
  }

  // Verify scanner handles failure correctly
  EXPECT_NE(scanFuture.get(), GATTLIB_SUCCESS);
}

/**
 * @brief Tests that concurrent scan calls are rejected
 * @details Verifies that:
 * 1. Cannot start a new scan while one is in progress
 * 2. Multiple attempts to start concurrent scans all fail with GATTLIB_BUSY
 */
TEST_F(GattlibScannerWithGMainLoop, ConcurrentScansTest) {
  blecpp::GattlibScanner scanner(m_fakeAdapter, *m_functions);
  std::string macAddress = "D4:28:C8:F3:7F:A1";

  constexpr int kTenSeconds = 10;
  // Set up expectations for the main scan
  EXPECT_CALL(*m_mock, adapter_scan_enable(m_fakeAdapter, _, kTenSeconds, _))
    .WillOnce(DoAll(
      SaveArg<1>(&m_savedCallback),
      SaveArg<3>(&m_savedUserData),
      Return(GATTLIB_SUCCESS)
    ));
  EXPECT_CALL(*m_mock, adapter_scan_disable(m_fakeAdapter))
    .WillOnce(Return(GATTLIB_SUCCESS));
  EXPECT_CALL(*m_mock, adapter_wait_scan_stopped(m_fakeAdapter));
  EXPECT_CALL(*m_mock, adapter_close(m_fakeAdapter)).Times(Exactly(0));

  // Start the main scan in background
  auto mainScan = std::async(std::launch::async, [&scanner, macAddress]() {
    return scanner.scan(kTenSeconds, macAddress);
  });

  constexpr int kOneHundredMilliseconds = 100;
  // Wait for scan to start
  while (!scanner.isScanning()) {
    std::this_thread::sleep_for(
      std::chrono::milliseconds(kOneHundredMilliseconds)
    );
  }

  // Try multiple concurrent scans while the main scan is running
  auto concurrentScan1 = std::async(std::launch::async, [&scanner]() {
    return scanner.scan(kTenSeconds);
  });
  EXPECT_EQ(concurrentScan1.get(), GATTLIB_BUSY);

  auto concurrentScan2 = std::async(std::launch::async, [&scanner]() {
    return scanner.scan(kTenSeconds);
  });
  EXPECT_EQ(concurrentScan2.get(), GATTLIB_BUSY);

  auto concurrentScan3 = std::async(std::launch::async, [&scanner]() {
    return scanner.scan(kTenSeconds);
  });
  EXPECT_EQ(concurrentScan3.get(), GATTLIB_BUSY);

  // Simulate device discovery and let main scan complete
  if (m_savedCallback != nullptr) {
    // First discovery
    m_savedCallback(
      m_fakeAdapter, macAddress.c_str(), "Device-1", m_savedUserData
    );
  }

  EXPECT_EQ(mainScan.get(), GATTLIB_SUCCESS);
}

/**
 * @brief Tests that scanner aborts gracefully when receiving a signal
 * @details Verifies that:
 * 1. Scanner can be aborted via signal
 * 2. Resources are properly cleaned up in correct order
 * 3. Scan operation returns GATTLIB_SUCCESS after abort
 */
TEST_F(GattlibScannerWithGMainLoop, SignalAbortTest) {
  blecpp::GattlibScanner scanner(m_fakeAdapter, *m_functions);
  std::string macAddress = "D4:28:C8:F3:7F:A1";

  // Set up signal handler
  struct sigaction signalHandler {}; // Zero-initialize the struct
  signalHandler.sa_handler = [](int) {
    // This is just a placeholder, actual abort happens in the test
  };
  sigemptyset(&signalHandler.sa_mask);
  signalHandler.sa_flags = 0;
  ASSERT_NE(sigaction(SIGUSR1, &signalHandler, nullptr), -1)
    << "Failed to set up signal handler";

  // Set up expectations for the scan and shutdown sequence
  {
    // Use strict ordering to verify shutdown sequence
    testing::InSequence seq;
    constexpr int kThirtySeconds = 30;
    // 1. Initial scan enable
    EXPECT_CALL(
      *m_mock, adapter_scan_enable(m_fakeAdapter, _, kThirtySeconds, _)
    )
      .WillOnce(DoAll(
        SaveArg<1>(&m_savedCallback), SaveArg<3>(&m_savedUserData),
        Return(GATTLIB_SUCCESS)
      ));

    // 2. Scan disable during shutdown
    EXPECT_CALL(*m_mock, adapter_scan_disable(m_fakeAdapter))
      .WillOnce(Return(GATTLIB_SUCCESS));

    // 3. Wait for scan to stop
    EXPECT_CALL(*m_mock, adapter_wait_scan_stopped(m_fakeAdapter));

    // 4. Close adapter during shutdown
    EXPECT_CALL(*m_mock, adapter_close(m_fakeAdapter)).Times(Exactly(0));
  }
  constexpr int kOneHundredMilliseconds = 100;
  constexpr int kThirtySeconds = 30;
  // Start scan in background
  auto scanFuture = std::async(std::launch::async, [&]() {
    try {
      return scanner.scan(kThirtySeconds, macAddress);
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Scan threw unexpected exception: " << e.what();
      return GATTLIB_DEVICE_ERROR;
    }
  });

  // Wait for scan to start
  while (!scanner.isScanning()) {
    std::this_thread::sleep_for(
      std::chrono::milliseconds(kOneHundredMilliseconds)
    );
  }

  // Simulate some device discoveries
  if (m_savedCallback == nullptr) {
    // First discovery - should continue scanning
    m_savedCallback(
      m_fakeAdapter, "D4:28:C8:F3:7F:B1", "Other-Device", m_savedUserData
    );
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Launch abort in background
  auto abortFuture = std::async(std::launch::async, [&]() {
    // Small delay to ensure scan is running
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // Send signal to self
    EXPECT_EQ(kill(getpid(), SIGUSR1), 0) << "Failed to send signal";
    // Call abort
    scanner.abort();
  });

  // Simulate discovery after abort signal - should trigger shutdown
  if (m_savedCallback == nullptr) {
    m_savedCallback(
      m_fakeAdapter, macAddress.c_str(), "Target-Device", m_savedUserData
    );
  }

  // Wait for abort to complete
  abortFuture.get();

  // Verify scan completes with UNEXPECTED status after abort
  EXPECT_EQ(scanFuture.get(), GATTLIB_UNEXPECTED)
    << "Scan did not complete successfully after abort";

  // Verify scanner is no longer scanning
  EXPECT_FALSE(scanner.isScanning())
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
  blecpp::GattlibScanner scanner(m_fakeAdapter, *m_functions);
  std::string targetMac = "D4:28:C8:F3:7F:A1";
  std::string otherMac = "00:11:22:33:44:55";
  std::atomic<bool> discoveryRunning{true};
  bool success = false;

  static constexpr auto kCallbackWaitMs = std::chrono::milliseconds(100);

  // Initialize callback to nullptr
  m_savedCallback = nullptr;
  m_savedUserData = nullptr;

  // Expect multiple scan attempts
  EXPECT_CALL(*m_mock, adapter_scan_enable(m_fakeAdapter, _, 5, _))
    .Times(AtLeast(2))
    .WillRepeatedly(DoAll(
      SaveArg<1>(&m_savedCallback),
      SaveArg<3>(&m_savedUserData),
      Return(GATTLIB_SUCCESS)
    ));
  EXPECT_CALL(*m_mock, adapter_scan_disable(m_fakeAdapter))
    .Times(AtLeast(2))
    .WillRepeatedly(Return(GATTLIB_SUCCESS));

  EXPECT_CALL(*m_mock, adapter_wait_scan_stopped(m_fakeAdapter))
    .Times(AtLeast(2));
  
  EXPECT_CALL(*m_mock, adapter_close(m_fakeAdapter)).Times(Exactly(0));

  // Start discovery simulation thread
  auto discoveryThread = std::thread([&]() {
    auto startTime = std::chrono::steady_clock::now();
    constexpr int kTenSeconds = 10;
    constexpr int kTwentySeconds = 20;
    while (discoveryRunning.load()) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - startTime)
          .count();

      // Stop after 20 seconds
      if (elapsed >= kTwentySeconds) {
        discoveryRunning.store(false);
        break;
      }

      // Get current callback state under lock
      gattlib_discovered_device_t localCallback = nullptr;
      void* localUserData = nullptr;
      {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        localCallback = m_savedCallback;
        localUserData = m_savedUserData;
      }

      if (localCallback == nullptr) {
        std::this_thread::sleep_for(kCallbackWaitMs);
        continue;
      }

      if (elapsed < kTenSeconds) {
        // First 10 seconds: discover wrong MAC
        localCallback(
          m_fakeAdapter, otherMac.c_str(), "Other-Device", localUserData
        );
      } else {
        // 10-20 seconds: discover correct MAC
        localCallback(
          m_fakeAdapter, targetMac.c_str(), "Target-Device", localUserData
        );
        discoveryRunning.store(false); // Stop after finding target MAC
      }

      // Sleep for 1 second between discoveries
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });

  constexpr int kFiveSeconds = 5;
  // Keep scanning until success
  while (!success) {
    auto scanFuture = std::async(std::launch::async, [&scanner, &targetMac]() {
      return scanner.scan(kFiveSeconds, targetMac);
    });

    int result = scanFuture.get();
    if (result == GATTLIB_SUCCESS) {
      success = true;
    } else {
      EXPECT_EQ(result, GATTLIB_TIMEOUT);
    }
  }

  // Always join discovery thread at the end
  if (discoveryThread.joinable()) {
    discoveryThread.join();
  }
}
