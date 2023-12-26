#include <thread>

#include "client.h"
#include "thread.h"

void Thread::start_threads(int repeat_thread_count, int repeat_requests_count, const std::string& url, const std::string& post_data)
{
  // Perform parallel HTTP requests using C++ Threads
  std::vector<std::thread> threads;
  threads.reserve(repeat_thread_count);

  // Create HTTP Client object
  Client client(repeat_requests_count, url, post_data);
  // Create multiple threads
  for (int i = 0; i < repeat_thread_count; ++i)
  {
    threads.emplace_back(
        [client]()
        {
          // Perform multiple HTTP requests using Asio co_spawn, within the current thread
          client.spawn_requests();
        });
  }

  // Wait for all threads to finish
  for (auto& thread : threads)
  {
    thread.join();
  }
}
