#pragma once

#include <asio/io_context.hpp>
#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ssl.hpp>
#include <asio/streambuf.hpp>
#include <string>

#include "reply_struct.h"
#include "result_response_struct.h"
#include "settings_struct.h"

/**
 * \class Client
 * \brief HTTP Client
 */
class Client
{
public:
  explicit Client(const Settings& settings, asio::io_context& io_context);
  virtual ~Client();

  void do_request() const;

private:
  int repeat_requests_count_;
  int duration_sec_;
  std::string url_;
  std::string post_data_;
  bool verbose_;
  bool silent_;
  bool verify_peer_;
  bool override_verify_tls_;
  bool debug_verify_tls_;
  long ssl_options_;
  asio::io_context& io_context_;

  asio::ip::basic_resolver<asio::ip::tcp>::results_type resolve_result_;
  std::chrono::duration<double, std::milli> dns_lookup_duration_;
  std::string protocol_;
  std::string host_;
  std::string port_;
  std::string path_params_;

  bool verify_certificate_callback(bool preverified, asio::ssl::verify_context& context) const;
  template <typename SyncReadStream> static ResultResponse handle_request(SyncReadStream& socket, asio::streambuf& request);
  template <typename SyncReadStream> static Reply parse_response(SyncReadStream& socket);
};
