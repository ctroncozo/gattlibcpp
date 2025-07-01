#include "gattlib_client.hpp"
#include "log_macros.hpp"
#include "mock_gattlib.hpp"

#include <future>

// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
// NOLINTEND(*)

#include <memory>

#include <gtest/gtest.h>

using namespace testing;

class GattlibClientConnectTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize the mock gattlib instance (get singleton instance)
    m_mock = &MockGattlib::getInstance();

    // Create a fake adapter pointer using a properly sized allocation
    m_fakeAdapter =
      static_cast<gattlib_adapter_t *>(::operator new(sizeof(void *)));

    // Create a fake connection pointer to be used in the connect callback
    m_fakeConnection =
      static_cast<gattlib_connection_t *>(::operator new(sizeof(void *)));
  }

  void TearDown() override {
    // Clean up the fake adapter pointer
    ::operator delete(m_fakeAdapter);

    // Clean up the fake connection pointer
    ::operator delete(m_fakeConnection);

    // Clear mock expectations
    testing::Mock::VerifyAndClearExpectations(m_mock);
  }

  /// Mock gattlib implementation
  MockGattlib *m_mock; // NOLINT
  /// Fake BLE adapter pointer
  gattlib_adapter_t *m_fakeAdapter; // NOLINT
  /// Fake connection pointer
  gattlib_connection_t *m_fakeConnection; // NOLINT
  /// Saved callback for device discovery
  gattlib_discovered_device_t m_savedDiscoveredDeviceCallback; // NOLINT
  /// Saved callback for connection
  gatt_connect_cb_t m_savedConnectCallback; // NOLINT
  /// Saved user data for connection callback
  void* m_savedConnectUserData; // NOLINT
};

TEST_F(GattlibClientConnectTest, ConnectSuccess) {
  // Set up expectations for the adapter_open call
  EXPECT_CALL(*m_mock, adapter_open(testing::A<const char*>(), testing::A<gattlib_adapter_t**>()))
    .WillOnce(DoAll(SetArgPointee<1>(m_fakeAdapter), Return(GATTLIB_SUCCESS)));
    
  // Set up expectations for the adapter_scan_enable call
  EXPECT_CALL(
    *m_mock, adapter_scan_enable(
               testing::A<gattlib_adapter_t *>(),
               testing::A<gattlib_discovered_device_t>(), testing::A<size_t>(),
               testing::A<void *>()
             )
  )
    .WillOnce(DoAll(Invoke([this](
                             gattlib_adapter_t * /*adapter*/,
                             gattlib_discovered_device_t callback /*callback*/,
                             size_t /*timeout*/, void * /*userData*/
                           ) {
      BLECPP_LOG_INFO("adapter_scan_enable called");
      m_savedDiscoveredDeviceCallback = callback; 
          return GATTLIB_SUCCESS;
        }
      ),
      Return(GATTLIB_SUCCESS)
    ));

  // Set up expectations for the connect call
  // This mock will simply print a message when called and return success
  EXPECT_CALL(*m_mock, connect(
      testing::A<gattlib_adapter_t*>(),
      testing::A<const char*>(),
      testing::A<unsigned long>(),
      testing::A<gatt_connect_cb_t>(),
      testing::A<void*>()))
    .WillOnce(DoAll(
      Invoke(
        [this](gattlib_adapter_t * /*adapter*/, const char * /*destination*/, 
           unsigned long /*options*/, gatt_connect_cb_t connectCallback, void * userData) {
          BLECPP_LOG_DEBUG("gattlib_connect called");
          
          // Save the callback and user data for later use
          m_savedConnectCallback = connectCallback;
          m_savedConnectUserData = userData;
          
          return GATTLIB_SUCCESS;
        }
      ),
      Return(GATTLIB_SUCCESS)
    ));

  // Create the client with our mock functions
  auto functions = MockGattlib::getMockFunctions();
  blecpp::GattlibClient client(functions);

  // Connect to a device
  static constexpr int32_t kConnectionTimeoutSeconds = 5;
  int result = client.connect(kConnectionTimeoutSeconds, "00:11:22:33:44:55");

  // Verify the result
  EXPECT_EQ(result, GATTLIB_SUCCESS);
}