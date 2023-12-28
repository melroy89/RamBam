#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "reply_struct.h"
#include "result_response_struct.h"

/**
 * \class Client
 * \brief HTTP Client
 */
class Client
{
public:
  explicit Client(int repeat_requests_count,
                  const std::string& url,
                  const std::string& post_data = "",
                  bool verbose = false,
                  bool silent = false,
                  bool verify_peer = true,
                  bool override_verify_tls_ = false,
                  bool debug_verify_tls = false,
                  long ssl_options = (boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                                      boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::no_tlsv1 |
                                      boost::asio::ssl::context::no_tlsv1_1));
  virtual ~Client();

  void spawn_requests() const;

private:
  int repeat_requests_count_;
  std::string url_;
  std::string post_data_;
  bool verbose_;
  bool silent_;
  bool verify_peer_;
  bool override_verify_tls_;
  bool debug_verify_tls_;
  long ssl_options_;

  boost::asio::awaitable<ResultResponse> async_request(boost::asio::io_context& io_context,
                                                       boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,
                                                       const std::chrono::duration<double, std::milli>& dns_lookup_duration,
                                                       const std::string& protocol,
                                                       const std::string& host,
                                                       const std::string& port,
                                                       const std::string& pathParams,
                                                       const std::string& post_data) const;
  bool verify_certificate_callback(bool preverified, boost::asio::ssl::verify_context& context) const;
  template <typename SyncReadStream> static ResultResponse handle_request(SyncReadStream& socket, boost::asio::streambuf& request);
  template <typename SyncReadStream> static Reply parse_response(SyncReadStream& socket);
};
