
#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/regex.hpp>

#include "http_client.h"

using namespace std::chrono_literals;

/**
 * \brief Constructor
 */
HttpClient::HttpClient(int repeat_requests_count, const std::string& url, const std::string& post_data)
    : repeat_requests_count_(repeat_requests_count),
      url_(url),
      post_data_(post_data)
{
}

/**
 * \brief Destructor
 */
HttpClient::~HttpClient()
{
}

void HttpClient::spawn_http_requests() const
{
  try
  {
    std::vector<std::string> parsed_url;
    boost::regex expression(
        //   proto             host                 port     rest
        "(\?:([^:/\?#]+)://)\?(\\w+[^/\?#:]*)(\?::(\\d+))*(?:)(.*)");
    // Make a const copy of the URL
    std::string url_copy(url_);
    boost::algorithm::trim(url_copy);
    // TODO: Move to regex_token_iterator()
    if (boost::regex_split(std::back_inserter(parsed_url), url_copy, expression))
    {
      boost::asio::io_context io;
      boost::asio::ip::tcp::resolver resolver(io);
      // Resolve the server hostname and service
      boost::asio::ip::tcp::resolver::query query(parsed_url[1], parsed_url[0]);
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

      // Spawn multiple async http requests
      for (int i = 0; i < repeat_requests_count_; ++i)
      {
        boost::asio::co_spawn(io,
                              this->async_http_call(io, endpoint_iterator, std::move(parsed_url[1]), std::move(parsed_url[2]),
                                                    std::move(parsed_url[3]), std::move(post_data_)),
                              boost::asio::detached);
      }
      // Run the tasks concurrently
      io.run();
    }
  }
  catch (std::exception& e)
  {
    std::printf("Exception: %s\n", e.what());
  }
}

boost::asio::awaitable<HttpResponse> HttpClient::async_http_call(boost::asio::io_context& io_context,
                                                                 boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,
                                                                 const std::string& host,
                                                                 const std::string& port,
                                                                 const std::string& pathParams,
                                                                 const std::string& post_data) const
{
  HttpResponse result;

  try
  {
    boost::asio::streambuf request;

    // Start time measurement
    const auto start_time_point = std::chrono::steady_clock::now();

    // Create and connect the socket
    boost::asio::ip::tcp::socket socket(io_context);
    boost::asio::connect(socket, endpoint_iterator);

    // Compose the HTTP request
    std::ostream request_stream(&request);
    // We should also support: DELETE, PUT, PATCH
    if (empty(post_data))
    {
      request_stream << "GET " + pathParams + " HTTP/1.1\r\n";
    }
    else
    {
      request_stream << "POST " + pathParams + " HTTP/1.1\r\n";
    }
    std::string hostname(host);
    if (!empty(port))
    {
      hostname.append(":" + port);
    }
    request_stream << "Host: " + hostname + "\r\n";
    request_stream << "User-Agent: RamBam/1.0\r\n";
    if (!empty(post_data))
    {
      request_stream << "Content-Type: application/json; charset=utf-8\r\n";
      request_stream << "Accept: */*\r\n"; // We should be able to override this (eg. application/json)
      request_stream << "Content-Length: " << post_data.length() << "\r\n";
    }
    request_stream << "Connection: close\r\n\r\n"; // End is a double line feed
    if (!empty(post_data))
    {
      request_stream << post_data;
    }

    // Send the HTTP request
    boost::asio::write(socket, request);

    // Read and print the HTTP response
    boost::asio::streambuf response;
    // Read until the status line.
    boost::asio::read_until(socket, response, "\r\n");

    std::istream response_stream(&response);
    response_stream >> result.http_version;
    response_stream >> result.status_code;
    std::getline(response_stream, result.status_message);

    // Get till all the headers, can we improve the performance of this call?
    boost::asio::read_until(socket, response, "\r\n\r\n");
    // Extract headers
    std::string header_line;
    int response_content_length = -1;
    std::regex clregex(R"xx(^content-length:\s+(\d+))xx", std::regex_constants::icase);
    while (std::getline(response_stream, header_line) && header_line != "\r")
    {
      size_t colon_pos = header_line.find(':');
      if (colon_pos != std::string::npos)
      {
        std::string header_name = header_line.substr(0, colon_pos);
        std::string header_value = header_line.substr(colon_pos + 2); // Skip ': ' after colon
        result.headers.emplace_back(std::move(header_name), std::move(header_value));
      }
      std::smatch match;
      if (std::regex_search(header_line, match, clregex))
        response_content_length = std::stoi(match[1]);
    }

    // Get body response using the length indicated by the content-length. Or read all, if header was not present.
    if (response_content_length != -1)
    {
      response_content_length -= response.size();
      if (response_content_length > 0)
        boost::asio::read(socket, response, boost::asio::transfer_exactly(response_content_length));
    }
    else
    {
      boost::system::error_code ec;
      boost::asio::read(socket, response, boost::asio::transfer_all(), ec);
      if (ec)
      {
        std::cerr << "Unable to read HTTP response: " << ec.message() << std::endl;
      }
    }

    std::stringstream re;
    re << &response;
    // Close connection
    socket.close();

    result.body = re.str();

    const auto end_time_point = std::chrono::steady_clock::now();
    result.total_time_duration = end_time_point - start_time_point;

    std::cout << "Response: " << result.http_version << " " << std::to_string(result.status_code) << " " << result.status_message << std::endl;
    std::cout << "Total duration: " << result.total_time_duration << std::endl;
    std::cout << "Body Content:\n" << result.body << std::endl;
    std::cout << "Headers:\n" << std::endl;
    for (const auto& header : result.headers)
    {
      std::cout << "Name: " << header.first << " Value: " << header.second << std::endl;
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
  co_return result;
}
