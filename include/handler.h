#pragma once

#include <chrono>
#include <sstream>
#include <vector>

#include "settings_struct.h"

/**
 * \class Handler
 * \brief Handler for thread pool and HTTP(S) requests, class not can be an object.
 */
class Handler
{
public:
  static void start(const Settings& settings);

private:
  Handler() = delete;

  // TODO: Move all these console print functions to a separate class
  static void display_progress_bar(int percentage, int remaining_time = -1, int remaining_requests = -1);
  static void test_info(const std::size_t num_threads, const Settings& settings);
  static void test_report(const Settings& settings, int total, std::chrono::duration<double, std::milli> total_test_duration);
  static void print_table(const std::vector<std::vector<std::string>>& table, const std::string& header = "", const std::string& footer = "");

  template <typename T> static std::string to_string_with_precision(const T a_value, const int n = 2)
  {
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return std::move(out).str();
  }
};
