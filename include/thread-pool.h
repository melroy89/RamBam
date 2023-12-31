#pragma once

#include <boost/asio.hpp>
#include <thread>

/**
 * \class ThreadPool
 * \brief Thread pool class for Asio IO service.
 */
class ThreadPool
{
public:
  ThreadPool(const size_t num_threads = 0);
  ~ThreadPool();

  void start();
  void stop();
  bool is_started() const;
  int get_number_threads() const
  {
    return number_;
  }

  /**
   * \brief Enqueue work to the thread pool using the io_service.
   * \param work The work to be done.
   */
  template <typename Func, typename... Args> void enqueue(Func&& func, Args&&... args)
  {
    // Post the work to the io_service
    boost::asio::post(io_service_, std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
  }
  boost::asio::io_service& get_context()
  {
    return std::ref(io_service_);
  }

private:
  bool is_started_;
  std::size_t number_; // Number of threads
  boost::asio::io_service io_service_;
  std::vector<std::thread> threads_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
};
