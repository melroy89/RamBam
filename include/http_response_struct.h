#pragma once

#include <chrono>
#include <string>
#include <vector>

struct HttpResponse
{
  std::string http_version;
  unsigned int status_code;
  std::string status_message;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
  std::chrono::duration<double, std::milli> total_time_duration;
};
