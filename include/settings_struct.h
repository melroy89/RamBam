#pragma once

#include <string>

struct Settings
{
  int threads;
  int requests;
  int duration_sec;

  std::string url; // TODO: Support multiple URLs as a vector
  std::string post_data;
  bool verify_peer;
  bool override_verify_tls;
  bool verbose;
  bool silent;
  bool debug;
  long ssl_options;
};
