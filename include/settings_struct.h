#pragma once

#include <string>

struct Settings
{
  int repeat_thread_count;
  int repeat_requests_count;

  std::string url; // TODO: Support multiple URLs as a vector
  std::string post_data;
  bool disable_peer_verification;
  bool override_verify_tls;
  bool debug;
  bool silent;
};
