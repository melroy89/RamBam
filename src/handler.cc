#include <algorithm>
#include <asio.hpp>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>

#include "client.h"
#include "handler.h"

/**
 * \brief Start the threads
 * \param settings The settings struct
 */
void Handler::start(const Settings& settings)
{
  int total = 0;

  // By default, use the number of concurrent threads supported (only a hint),
  // could return '0' when not computable.
  std::size_t number_of_threads = (settings.repeat_thread_count == 0) ? std::thread::hardware_concurrency() : settings.repeat_thread_count;
  // Fallback to 4 threads if the number of concurrent threads cannot be computed
  if (number_of_threads == 0)
    number_of_threads = 4;

  // Show test information
  if (!settings.silent)
  {
    test_info(number_of_threads, settings);
    std::cout << std::endl;
  }

  asio::thread_pool pool(settings.repeat_thread_count);
  asio::io_context io_context;

  Client client(settings, io_context);
  auto worker = [client]() { client.do_request(); };

  const auto start_test_time_point = std::chrono::steady_clock::now();
  if (settings.duration_sec > 0)
  {
    std::chrono::seconds execution_duration(settings.duration_sec);
    auto now = std::chrono::steady_clock::now;
    auto stop_time = now() + execution_duration;
    while (true)
    {
      auto current_time = now();
      if (current_time < stop_time)
      {
        asio::post(pool, worker);
        ++total;
        // Only do the below once every 1000 requests, so we use less resource / CPU time
        if (!settings.silent && (total % 1000) == 0)
        {
          auto remaining_time = std::chrono::duration_cast<std::chrono::seconds>(stop_time - current_time).count();
          int percentage = 100 - (remaining_time * 100 / settings.duration_sec);
          display_progress_bar(percentage, remaining_time);
        }
      }
      else
      {
        // Time is up! Stop the thread pool as fast as possible.
        pool.stop();
        // Then break the while loop
        break;
      }
    }
  }
  else
  {
    for (int i = 0; i < settings.repeat_requests_count; ++i)
    {
      asio::post(pool, worker);
      // Only do the below every 1000 requests, so we use less resource / CPU time
      if (!settings.silent && (i % 1000) == 0)
      {
        int percentage = (i + 1) * 100 / settings.repeat_requests_count;
        display_progress_bar(percentage, -1, (settings.repeat_requests_count - i - 1));
      }
    }
  }

  // Wait until all threads are finished
  pool.join();
  const auto end_test_time_point = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> total_test_duration = end_test_time_point - start_test_time_point;

  // Show test report
  if (!settings.silent)
  {
    display_progress_bar(100); // Always set to 100% now
    std::cout << std::endl;
    test_report(settings, total, total_test_duration);
  }
}

/**
 * \brief Display a progress bar
 * \param percentage The percentage of the progress
 * \param remaining_time The remaining time in seconds
 * \param remaining_requests The remaining requests
 */
void Handler::display_progress_bar(int percentage, int remaining_time, int remaining_requests)
{
  int numBars = percentage / 3;
  std::cout << "\r" << percentage << "% [";
  for (int i = 0; i < 30; ++i)
  {
    if (i < numBars)
      std::cout << "#";
    else
      std::cout << " ";
  }
  std::cout << "]";

  if (remaining_time != -1)
  {
    std::cout << " " << remaining_time << "s remaining" << std::flush;
  }
  else if (remaining_requests != -1)
  {
    std::cout << " " << remaining_requests << " requests remaining" << std::flush;
  }
  else
  {
    std::cout << std::string(80, ' ') << std::flush;
  }
}

/**
 * \brief Print info about the test
 * \param num_threads The number of threads
 * \param settings The settings struct
 * \details Print the test information, like URL, type of test, number of threads, etc.
 */
void Handler::test_info(const std::size_t num_threads, const Settings& settings)
{
  std::vector<std::vector<std::string>> info = {{"URL under test:", settings.url}};
  if (settings.duration_sec == 0)
  {
    // Number of Requests test
    info.push_back({"Type of test:", "Number of Requests"});
    info.push_back({"Request count input:", std::to_string(settings.repeat_requests_count)});
  }
  else
  {
    // Duration test
    info.push_back({"Type of test:", "Duration"});
    info.push_back({"Duration input:", std::to_string(settings.duration_sec) + " seconds"});
  }
  info.push_back({"Threads:", std::to_string(num_threads)});
  print_table(info);
}

// Print test report
void Handler::test_report(const Settings& settings, int total, std::chrono::duration<double, std::milli> total_test_duration)
{
  float total_seconds = total_test_duration.count() / 1000.0;
  std::vector<std::vector<std::string>> report = {{"Type of test:", (settings.duration_sec == 0) ? "Number of Requests" : "Duration"}};
  if (settings.duration_sec == 0)
  {
    // Number of Requests test
    report.push_back({"Request count input:", std::to_string(settings.repeat_requests_count)});
    report.push_back({"Average reqs/sec:", to_string_with_precision(settings.repeat_requests_count / total_seconds)});
  }
  else
  {
    // Duration test
    report.push_back({"Duration input:", std::to_string(settings.duration_sec) + " s"});
    report.push_back({"Total requests executed:", std::to_string(total)});
    report.push_back({"Average reqs/sec:", to_string_with_precision(total / total_seconds)});
  }
  report.push_back({"Total test duration:", to_string_with_precision(total_test_duration.count(), 4) + " ms"});

  print_table(report, "Report", "Test Completed!");
}

void Handler::print_table(const std::vector<std::vector<std::string>>& table, const std::string& header, const std::string& footer)
{
  // Calculate column widths
  std::vector<size_t> col_widths(table[0].size(), 0);
  for (const auto& row : table)
  {
    for (size_t i = 0; i < row.size(); ++i)
    {
      col_widths[i] = std::max(col_widths[i], row[i].size());
    }
  }

  // Calculate total width of the table
  size_t total_width = std::accumulate(col_widths.begin(), col_widths.end(), 0) + 3 * table[0].size() + 1;

  // Print header
  if (!header.empty())
  {
    std::cout << "╔" << std::string(total_width - 2, '=') << "╗" << std::endl;
    std::cout << "║ " << std::left << std::setw(total_width - 4) << std::setfill(' ') << header << " ║" << std::endl;
  }

  // Print top border
  for (size_t i = 0; i < table[0].size(); ++i)
  {
    std::cout << "+-" << std::string(col_widths[i], '-') << "-";
  }
  std::cout << "+" << std::endl;

  // Print table contents with borders
  for (const auto& row : table)
  {
    for (size_t i = 0; i < row.size(); ++i)
    {
      if (i == 0)
      {
        std::cout << "║ ";
      }
      else
      {
        std::cout << "| ";
      }
      std::cout << std::left << std::setw(col_widths[i]) << std::setfill(' ') << row[i] << " ";
    }
    std::cout << "║" << std::endl;
  }

  // Print bottom border
  for (size_t i = 0; i < table[0].size(); ++i)
  {
    std::cout << "+-" << std::string(col_widths[i], '-') << "-";
  }
  std::cout << "+" << std::endl;

  // Print footer
  if (!footer.empty())
  {
    std::cout << "║ " << std::left << std::setw(total_width - 4) << std::setfill(' ') << footer << " ║" << std::endl;
    std::cout << "╚" << std::string(total_width - 2, '=') << "╝" << std::endl;
  }
}