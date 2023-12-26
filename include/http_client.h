#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "reply_struct.h"
#include "result_response_struct.h"

/**
 * \class HTTPClient
 */
class HttpClient
{
public:
  explicit HttpClient(int repeat_requests_count,
                      const std::string& url,
                      const std::string& post_data = "",
                      long ssl_options = (boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                                          boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::no_tlsv1 |
                                          boost::asio::ssl::context::no_tlsv1_1),
                      bool verify_peer = true);
  virtual ~HttpClient();

  void spawn_http_requests() const;

private:
  int repeat_requests_count_;
  std::string url_;
  std::string post_data_;
  long ssl_options_;
  bool verify_peer_;

  boost::asio::awaitable<ResultResponse> async_http_call(boost::asio::io_context& io_context,
                                                         boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,
                                                         const std::chrono::duration<double, std::milli>& dns_lookup_duration,
                                                         const std::string& protocol,
                                                         const std::string& host,
                                                         const std::string& port,
                                                         const std::string& pathParams,
                                                         const std::string& post_data) const;
  template <typename SyncReadStream> static ResultResponse handle_request(SyncReadStream& socket, boost::asio::streambuf& request);
  template <typename SyncReadStream> static Reply parse_response(SyncReadStream& socket);
};
