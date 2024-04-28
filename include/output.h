#pragma once

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

// Forward declaration
class Settings;

class Output
{
public:
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

private:
  Output() = delete;
};