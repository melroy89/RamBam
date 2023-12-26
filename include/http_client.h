#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "result_response_struct.h"
#include "reply_struct.h"

class HttpClient
{
public:
  explicit HttpClient(int repeat_requests_count, const std::string& url, const std::string& post_data = "");
  virtual ~HttpClient();

  void spawn_http_requests() const;

private:
  int repeat_requests_count_;
  std::string url_;
  std::string post_data_;

  boost::asio::awaitable<ResultResponse> async_http_call(boost::asio::io_context& io_context,
                                                         boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,
                                                         const std::chrono::duration<double, std::milli>& dns_lookup_duration,
                                                         const std::string& protocol,
                                                         const std::string& host,
                                                         const std::string& port,
                                                         const std::string& pathParams,
                                                         const std::string& post_data) const;
  template<typename SyncReadStream>
  static ResultResponse handle_request(SyncReadStream& socket, boost::asio::streambuf& request);
  template<typename SyncReadStream>
  static Reply parse_response(SyncReadStream& socket);
};
