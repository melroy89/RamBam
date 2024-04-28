#pragma once

// Forward declaration
class Settings;

/**
 * \class Handler
 * \brief Handler for thread pool and HTTP(S) requests, class not can be an object.
 */
class Handler
{
public:
  static void start(const Settings& settings);

private:
  Handler() = delete;
};
