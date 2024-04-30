#include <asio.hpp>

#include "client.h"
#include "handler.h"
#include "output.h"
#include "settings_struct.h"

/**
 * \brief Start the threads
 * \param settings The settings struct
 */
void Handler::start(const Settings& settings)
{
  int total = 0;

  // By default, use the number of concurrent threads supported (only a hint),
  // could return '0' when not computable.
  std::size_t number_of_threads = (settings.threads == 0) ? std::thread::hardware_concurrency() : settings.threads;
  // Fallback to 4 threads if the number of concurrent threads cannot be computed
  if (number_of_threads == 0)
    number_of_threads = 4;

  // Show test information
  if (!settings.silent)
  {
    Output::test_info(number_of_threads, settings);
  }

  asio::thread_pool pool(number_of_threads);
  asio::io_context io_context;

  Client client(settings, io_context);
  auto worker = [client]() { client.do_request(); };

  const auto start_test_time_point = std::chrono::steady_clock::now();
  // Duration test?
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
        // Only do the below once every 1000 requests, meaning less resources / CPU time
        if (!settings.silent && (total % 1000) == 0)
        {
          auto remaining_time = std::chrono::duration_cast<std::chrono::seconds>(stop_time - current_time).count();
          int percentage = 100 - (remaining_time * 100 / settings.duration_sec);
          Output::display_progress_bar(percentage, remaining_time);
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
  // Number of requests test?
  else
  {
    for (int i = 0; i < settings.requests; ++i)
    {
      asio::post(pool, worker);
      // Only do the below every 1000 requests, meaning less resources / CPU time
      if (!settings.silent && (i % 1000) == 0)
      {
        int percentage = (i + 1) * 100 / settings.requests;
        Output::display_progress_bar(percentage, -1, (settings.requests - i - 1));
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
    Output::display_progress_bar(100); // Always set to 100% now
    Output::test_report(settings, total, total_test_duration);
  }
}
