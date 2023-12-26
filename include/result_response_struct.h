#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "duration_struct.h"

struct ResultResponse
{
  std::string http_version;
  unsigned int status_code;
  std::string status_message;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
  Duration duration;
};
