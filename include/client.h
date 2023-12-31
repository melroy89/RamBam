#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>

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
  explicit Client(const Settings& settings, boost::asio::io_context& io_context);
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
  boost::asio::io_context& io_context_;

  boost::asio::ip::tcp::resolver::iterator endpoint_iterator_;
  std::chrono::duration<double, std::milli> dns_lookup_duration_;
  std::string protocol_;
  std::string host_;
  std::string port_;
  std::string path_params_;

  bool verify_certificate_callback(bool preverified, boost::asio::ssl::verify_context& context) const;
  template <typename SyncReadStream> static ResultResponse handle_request(SyncReadStream& socket, boost::asio::streambuf& request);
  template <typename SyncReadStream> static Reply parse_response(SyncReadStream& socket);
};
