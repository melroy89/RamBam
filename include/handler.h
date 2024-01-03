#pragma once

#include <chrono>

#include "settings_struct.h"

// Forward declaration
class ThreadPool;

/**
 * \class Handler
 * \brief Handler for thread pool and HTTP(S) requests, class not can be an object.
 */
class Handler
{
public:
  static void start_threads(const Settings& settings);

private:
  Handler() = delete;

  static void test_info(const ThreadPool& pool, const Settings& settings);
  static void test_report(const Settings& settings,
                          int total,
                          std::chrono::duration<double, std::milli> total_queue_time,
                          std::chrono::duration<double, std::milli> total_test_duration);
};
