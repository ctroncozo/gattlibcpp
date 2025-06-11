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
  EXPECT_FALSE(manager.isRunning());

  // Start should succeed
  EXPECT_TRUE(manager.start());
  EXPECT_TRUE(manager.isRunning());

  // Second start should be idempotent
  EXPECT_TRUE(manager.start());
  EXPECT_TRUE(manager.isRunning());

  // Stop should succeed
  manager.stop();
  EXPECT_FALSE(manager.isRunning());

  // Second stop should be safe
  manager.stop();
  EXPECT_FALSE(manager.isRunning());
}

TEST_F(GMainLoopManagerTest, ConcurrentStartStop) {
  blecpp::GMainLoopManager manager;
  std::atomic<int> successfulStarts {0};
  std::atomic<bool> keepRunning {true};

  // Launch multiple threads that try to start/stop the manager
  std::vector<std::thread> threads;
  constexpr int kFourThreads = 4;
  threads.reserve(kFourThreads);
  for (int i = 0; i < kFourThreads; ++i) {
    threads.emplace_back([&]() {
      while (keepRunning) {
        if (manager.start()) {
          successfulStarts++;
        }
        std::this_thread::sleep_for(1ms);
        manager.stop();
        std::this_thread::sleep_for(1ms);
      }
    });
  }

  // Let the threads run for a bit
  std::this_thread::sleep_for(100ms);
  keepRunning = false;

  // Join all threads
  for (auto &thread : threads) {
    thread.join();
  }

  // Verify we had some successful starts and the manager is in a valid state
  EXPECT_GT(successfulStarts.load(), 0);
  EXPECT_FALSE(manager.isRunning());
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

  EXPECT_TRUE(manager1.isRunning());
  EXPECT_TRUE(manager2.isRunning());

  // Stop them in reverse order
  manager2.stop();
  EXPECT_FALSE(manager2.isRunning());
  EXPECT_TRUE(manager1.isRunning());

  manager1.stop();
  EXPECT_FALSE(manager1.isRunning());
}

TEST_F(GMainLoopManagerTest, RapidStartStopSequence) {
  blecpp::GMainLoopManager manager;
  constexpr int kFiveTimes = 5;
  // Rapidly start and stop the manager multiple times
  for (int i = 0; i < kFiveTimes; ++i) {
    EXPECT_TRUE(manager.start()) << "Failed to start on iteration " << i;
    EXPECT_TRUE(manager.isRunning());
    std::this_thread::sleep_for(1ms);
    manager.stop();
    EXPECT_FALSE(manager.isRunning());
    std::this_thread::sleep_for(1ms);
  }
}

TEST_F(GMainLoopManagerTest, StopTimeout) {
  blecpp::GMainLoopManager manager;
  EXPECT_TRUE(manager.start());
  constexpr int kFiveSeconds = 5000;
  constexpr int kOneSecond = 1000;
  // Add a long-running source that should be interrupted by stop
  g_timeout_add(
    kFiveSeconds,
    [](gpointer) -> gboolean { return G_SOURCE_CONTINUE; },
    nullptr
  );

  // Stop should complete quickly despite the long-running source
  auto startTime = std::chrono::steady_clock::now();
  manager.stop();
  auto stopDuration = std::chrono::steady_clock::now() - startTime;

  EXPECT_LT(
    std::chrono::duration_cast<std::chrono::milliseconds>(stopDuration)
      .count(),
    kOneSecond // Should take less than 1 second
  );
  EXPECT_FALSE(manager.isRunning());
}
