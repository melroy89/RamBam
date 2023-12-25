#pragma once

#include "http_response_struct.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>

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

  boost::asio::awaitable<HttpResponse> async_http_call(boost::asio::io_context& io_context,
                                                       boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,
                                                       const std::string& host,
                                                       const std::string& port,
                                                       const std::string& pathParams,
                                                       const std::string& post_data) const;
};
