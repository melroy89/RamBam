#pragma once

#include <string>
#include <vector>

struct Reply
{
  std::string http_version;
  unsigned int status_code;
  std::string status_message;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
};
