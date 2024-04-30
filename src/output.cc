#include "output.h"
#include "settings_struct.h"

#include <iomanip>
#include <iostream>
#include <numeric>

/**
 * \brief Display a progress bar
 * \param percentage The percentage of the progress
 * \param remaining_time The remaining time in seconds
 * \param remaining_requests The remaining requests
 */
void Output::display_progress_bar(int percentage, int remaining_time, int remaining_requests)
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
void Output::test_info(const std::size_t num_threads, const Settings& settings)
{
  std::vector<std::vector<std::string>> info = {{"URL under test:", settings.url}};
  if (settings.duration_sec == 0)
  {
    // Number of Requests test
    info.push_back({"Type of test:", "Number of Requests"});
    info.push_back({"Request count input:", std::to_string(settings.requests)});
  }
  else
  {
    // Duration test
    info.push_back({"Type of test:", "Duration"});
    info.push_back({"Duration input:", std::to_string(settings.duration_sec) + " seconds"});
  }
  info.push_back({"Threads:", std::to_string(num_threads)});
  print_table(info);

  std::cout << std::endl;
}

// Print test report
void Output::test_report(const Settings& settings, int total, std::chrono::duration<double, std::milli> total_test_duration)
{
  float total_seconds = total_test_duration.count() / 1000.0;
  std::vector<std::vector<std::string>> report = {{"Type of test:", (settings.duration_sec == 0) ? "Number of Requests" : "Duration"}};
  if (settings.duration_sec == 0)
  {
    // Number of Requests test
    report.push_back({"Request count input:", std::to_string(settings.requests)});
    report.push_back({"Average reqs/sec:", to_string_with_precision(settings.requests / total_seconds)});
  }
  else
  {
    // Duration test
    report.push_back({"Duration input:", std::to_string(settings.duration_sec) + " s"});
    report.push_back({"Total requests executed:", std::to_string(total)});
    report.push_back({"Average reqs/sec:", to_string_with_precision(total / total_seconds)});
  }
  report.push_back({"Total test duration:", to_string_with_precision(total_test_duration.count(), 4) + " ms"});

  std::cout << std::endl;
  print_table(report, "Report", "Test Completed!");
}

void Output::print_table(const std::vector<std::vector<std::string>>& table, const std::string& header, const std::string& footer)
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