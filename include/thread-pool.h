#pragma once

#include <asio.hpp>
#include <thread>

/**
 * \class ThreadPool
 * \brief Thread pool class for Asio IO service.
 */
class ThreadPool
{
public:
  explicit ThreadPool(const size_t num_threads = 0);
  ~ThreadPool();

  void start();
  void stop();
  int get_number_threads() const;

  /**
   * \brief Enqueue work to the thread pool using the io_service.
   * \param work The work to be done.
   */
  template <typename Func, typename... Args> void enqueue(Func&& func, Args&&... args)
  {
    // Post the work to the io_service
    asio::post(io_context_, std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
  }

  asio::io_context& get_context()
  {
    return std::ref(io_context_);
  }

private:
  bool is_started_;
  std::size_t number_; // Number of threads
  asio::io_context io_context_;
  std::vector<std::thread> threads_;
  asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
};
