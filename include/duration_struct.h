#pragma once

#include <chrono>

struct Duration
{
  std::chrono::duration<double, std::milli> dns;
  std::chrono::duration<double, std::milli> prepare_request;
  std::chrono::duration<double, std::milli> connect;
  std::chrono::duration<double, std::milli> handshake;
  std::chrono::duration<double, std::milli> request;
  std::chrono::duration<double, std::milli> response;
  std::chrono::duration<double, std::milli> total_without_dns;
  std::chrono::duration<double, std::milli> total; // Note: DNS is only done once per thread (not for every coroutine)
};
