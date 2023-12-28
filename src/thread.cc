#include <thread>

#include "client.h"
#include "thread.h"

void Thread::start_threads(const Settings& settings)
{
  // Perform parallel HTTP requests using C++ Threads
  std::vector<std::thread> threads;
  threads.reserve(settings.repeat_thread_count);

  // Create HTTP Client object
  Client client(settings.repeat_requests_count, settings.url, settings.post_data, settings.verbose, settings.silent,
                !settings.disable_peer_verification, settings.override_verify_tls, settings.debug);
  // Create multiple threads
  for (int i = 0; i < settings.repeat_thread_count; ++i)
  {
    threads.emplace_back(
        [client]()
        {
          // Perform multiple HTTP(s) requests using Asio co_spawn, within the current thread
          client.spawn_requests();
        });
  }

  // Wait for all threads to finish
  for (auto& thread : threads)
  {
    thread.join();
  }
}
