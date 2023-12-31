#pragma once

#include "settings_struct.h"

// Forward declaration
class ThreadPool;

/**
 * \class Handler
 * \brief Handler for thread pool and HTTP(S) requests, class not can be an object.
 */
class Handler
{
public:
  static void start_threads(const Settings& settings);

private:
  Handler() = delete;

  static void info(const ThreadPool& pool, const Settings& settings);
};
