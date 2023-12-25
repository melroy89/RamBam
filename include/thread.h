#pragma once

#include <string>

class Thread
{
public:
  static void start_threads(int repeat_thread_count, int repeat_requests_count, const std::string& url, const std::string& post_data = "");
};
