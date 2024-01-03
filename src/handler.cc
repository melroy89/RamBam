#include <iostream>

#include "client.h"
#include "handler.h"
#include "thread-pool.h"

/**
 * \brief Start the threads
 * \param settings The settings struct
 */
void Handler::start_threads(const Settings& settings)
{
  int total = 0;
  ThreadPool pool(settings.repeat_thread_count);
  // Show test information
  if (!settings.silent)
    test_info(pool, settings);

  // Prepare client
  Client client(settings, pool.get_context());

  // Prepare thread-pool
  pool.start();

  const auto start_test_time_point = std::chrono::steady_clock::now();
  if (settings.duration_sec > 0)
  {
    std::chrono::seconds duration(settings.duration_sec);
    const auto now = std::chrono::system_clock::now();
    while (std::chrono::system_clock::now() - now < duration)
    {
      pool.enqueue(&Client::do_request, &client);
      // std::this_thread::yield();
      std::this_thread::sleep_for(std::chrono::nanoseconds(200000));
      ++total;
    }
  }
  else
  {
    for (int i = 0; i < settings.repeat_requests_count; ++i)
    {
      pool.enqueue(&Client::do_request, &client);
    }
  }

  const auto end_test_queue_time_point = std::chrono::steady_clock::now();
  // Wait until all threads are finished
  pool.stop();
  const auto end_test_time_point = std::chrono::steady_clock::now();

  std::chrono::duration<double, std::milli> total_queue_time = end_test_queue_time_point - start_test_time_point;
  std::chrono::duration<double, std::milli> total_test_duration = end_test_time_point - start_test_time_point;

  // Show test report
  if (!settings.silent)
    test_report(settings, total, total_queue_time, total_test_duration);
}

// Print info about the test
void Handler::test_info(const ThreadPool& pool, const Settings& settings)
{
  std::cout << "======================================" << std::endl;
  std::cout << "URL under test: " << settings.url << std::endl;
  if (settings.duration_sec == 0)
  {
    // Number of Requests test
    std::cout << "Type of test: Number of Requests" << std::endl;
    std::cout << "Total requests: " << settings.repeat_requests_count << std::endl;
  }
  else
  {
    // Duration test
    std::cout << "Type of test: Duration" << std::endl;
    std::cout << "Duration: " << settings.duration_sec << " seconds" << std::endl;
  }
  std::cout << "Threads: " << pool.get_number_threads() << std::endl;
  std::cout << "======================================" << std::endl;
}

// Print test report
void Handler::test_report(const Settings& settings,
                          int total,
                          std::chrono::duration<double, std::milli> total_queue_time,
                          std::chrono::duration<double, std::milli> total_test_duration)
{
  std::cout << "=============== Report ===============" << std::endl;
  float total_seconds = total_test_duration.count() / 1000.0;
  if (settings.duration_sec == 0)
  {
    // Number of Requests test
    std::cout << "Type of test: Number of Requests" << std::endl;
    std::cout << "Request count input: " << settings.repeat_requests_count << std::endl;
    std::cout << "Average reqs/sec: " << settings.repeat_requests_count / total_seconds << std::endl;
  }
  else
  {
    // Duration test
    std::cout << "Type of test: Duration" << std::endl;
    std::cout << "Duration input: " << settings.duration_sec << " second(s)" << std::endl;
    std::cout << "Total requests executed: " << total << std::endl;
    std::cout << "Average reqs/sec: " << total / total_seconds << std::endl;
  }
  std::cout << "Total internal queue time: " << total_queue_time.count() << " milliseconds" << std::endl;
  std::cout << "Total test duration: " << total_test_duration.count() << " milliseconds" << std::endl;
  std::cout << "========== Test Completed ! ==========" << std::endl;
}
