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
  ThreadPool pool(settings.repeat_thread_count);
  if (!settings.silent)
  {
    info(pool, settings);
  }

  // Prepare client
  Client client(settings, pool.get_context());

  pool.start();

  if (settings.repeat_requests_count > 0)
  {
    for (int i = 0; i < settings.repeat_requests_count; ++i)
    {
      pool.enqueue(&Client::do_request, &client);
    }
  }
  else
  {
    std::chrono::seconds duration(settings.duration_sec);
    const auto now = std::chrono::system_clock::now();
    int total = 0;
    while (std::chrono::system_clock::now() - now < duration)
    {
      pool.enqueue(&Client::do_request, &client);
      // std::this_thread::yield();
      std::this_thread::sleep_for(std::chrono::milliseconds(4));
      ++total;
    }
    if (!settings.silent)
    {
      std::cout << "--------------------------------------" << std::endl;
      std::cout << "Total requests executed: " << total << std::endl;
    }
  }

  pool.stop();
  if (!settings.silent)
    std::cout << "=========== Test Completed ===========" << std::endl;
}

// Print info about the test
void Handler::info(const ThreadPool& pool, const Settings& settings)
{
  std::cout << "======================================" << std::endl;
  std::cout << "URL under test: " << settings.url << std::endl;
  if (settings.repeat_requests_count > 0)
  {
    // Request count test
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
