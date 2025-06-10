/**
 * @file gmain_loop_tests.cpp
 * @brief Unit tests for the GMainLoopManager class.
 *
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-10
 *
 * This file provides a set of unit tests for the GMainLoopManager class,
 * which is a wrapper around the GLib main event loop for handling BLE
 * operations.
 */

#include "gmain_loop_manager.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

class GMainLoopManagerTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(GMainLoopManagerTest, StartStop) {
  blecpp::GMainLoopManager manager;
  EXPECT_FALSE(manager.is_running());

  // Start should succeed
  EXPECT_TRUE(manager.start());
  EXPECT_TRUE(manager.is_running());

  // Second start should be idempotent
  EXPECT_TRUE(manager.start());
  EXPECT_TRUE(manager.is_running());

  // Stop should succeed
  manager.stop();
  EXPECT_FALSE(manager.is_running());

  // Second stop should be safe
  manager.stop();
  EXPECT_FALSE(manager.is_running());
}

TEST_F(GMainLoopManagerTest, ConcurrentStartStop) {
  blecpp::GMainLoopManager manager;
  std::atomic<int> successful_starts {0};
  std::atomic<bool> keep_running {true};

  // Launch multiple threads that try to start/stop the manager
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&]() {
      while (keep_running) {
        if (manager.start()) {
          successful_starts++;
        }
        std::this_thread::sleep_for(1ms);
        manager.stop();
        std::this_thread::sleep_for(1ms);
      }
    });
  }

  // Let the threads run for a bit
  std::this_thread::sleep_for(100ms);
  keep_running = false;

  // Join all threads
  for (auto &t : threads) {
    t.join();
  }

  // Verify we had some successful starts and the manager is in a valid state
  EXPECT_GT(successful_starts.load(), 0);
  EXPECT_FALSE(manager.is_running());
}

TEST_F(GMainLoopManagerTest, DestructorDuringRun) {
  // Create a manager that will be destroyed while running
  {
    blecpp::GMainLoopManager manager;
    EXPECT_TRUE(manager.start());
    // Let it run for a bit
    std::this_thread::sleep_for(10ms);
    // Destructor should handle cleanup
  }

  // Create a new manager to verify we can still use GLib
  {
    blecpp::GMainLoopManager manager;
    EXPECT_TRUE(manager.start());
    manager.stop();
  }
}

TEST_F(GMainLoopManagerTest, MultipleInstances) {
  blecpp::GMainLoopManager manager1;
  blecpp::GMainLoopManager manager2;

  // Both should be able to start and run concurrently
  EXPECT_TRUE(manager1.start());
  EXPECT_TRUE(manager2.start());

  EXPECT_TRUE(manager1.is_running());
  EXPECT_TRUE(manager2.is_running());

  // Stop them in reverse order
  manager2.stop();
  EXPECT_FALSE(manager2.is_running());
  EXPECT_TRUE(manager1.is_running());

  manager1.stop();
  EXPECT_FALSE(manager1.is_running());
}

TEST_F(GMainLoopManagerTest, RapidStartStopSequence) {
  blecpp::GMainLoopManager manager;

  // Rapidly start and stop the manager multiple times
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(manager.start()) << "Failed to start on iteration " << i;
    EXPECT_TRUE(manager.is_running());
    std::this_thread::sleep_for(1ms);
    manager.stop();
    EXPECT_FALSE(manager.is_running());
    std::this_thread::sleep_for(1ms);
  }
}

TEST_F(GMainLoopManagerTest, StopTimeout) {
  blecpp::GMainLoopManager manager;
  EXPECT_TRUE(manager.start());

  // Add a long-running source that should be interrupted by stop
  g_timeout_add(
    5000, [](gpointer) -> gboolean { return G_SOURCE_CONTINUE; }, nullptr
  );

  // Stop should complete quickly despite the long-running source
  auto start_time = std::chrono::steady_clock::now();
  manager.stop();
  auto stop_duration = std::chrono::steady_clock::now() - start_time;

  EXPECT_LT(
    std::chrono::duration_cast<std::chrono::milliseconds>(stop_duration)
      .count(),
    1000 // Should take less than 1 second
  );
  EXPECT_FALSE(manager.is_running());
}
