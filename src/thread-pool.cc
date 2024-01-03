#include "thread-pool.h"

/**
 * \brief Constructor.
 * \param num_threads The number of threads to create in the thread pool (default 0 means use number of threads supported).
 */
ThreadPool::ThreadPool(const std::size_t num_threads) : is_started_(false), io_context_(), work_guard_(asio::make_work_guard(io_context_))
{
  // By default, use the number of concurrent threads supported (only a hint),
  // could return '0' when not computable.
  number_ = (num_threads == 0) ? std::thread::hardware_concurrency() : num_threads;
  // Fallback to 4 threads if the number of concurrent threads cannot be computed
  if (number_ == 0)
    number_ = 4;
}

// Destructor
ThreadPool::~ThreadPool()
{
  stop();
}

// Start the thread pool
void ThreadPool::start()
{
  if (is_started_)
    return;
  is_started_ = true;

  threads_.reserve(number_);
  // Create a thread pool of number of threads
  for (std::size_t i = 0; i < number_; ++i)
  {
    threads_.emplace_back(
        [this]()
        {
          // Block until there is work to do due to the worker guard
          io_context_.run();
        });
  }
}

// Stop the thread pool
void ThreadPool::stop()
{
  if (!is_started_)
    return;

  // Destroy the work guard, so that the threads can exit
  work_guard_.reset();
  // Wait for all threads to finish
  for (auto& thread : threads_)
  {
    thread.join();
  }
  io_context_.stop();
  io_context_.restart();
  is_started_ = false;
}

// Get the number of threads in the thread pool
int ThreadPool::get_number_threads() const
{
  return number_;
}
