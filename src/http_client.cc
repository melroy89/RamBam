
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
#include <boost/asio/ssl.hpp>
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
      const auto start_dns_lookup_time_point = std::chrono::steady_clock::now();
      boost::asio::io_context io;
      boost::asio::ip::tcp::resolver resolver(io);
      // Resolve the server hostname and service
      boost::asio::ip::tcp::resolver::query query(parsed_url[1], parsed_url[0]);
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
      const auto end_dns_lookup_time_point = std::chrono::steady_clock::now();
      std::chrono::duration<double, std::milli> dns_lookup_duration = end_dns_lookup_time_point - start_dns_lookup_time_point;

      std::cout << "DNS lookup duration (once per thread): " << dns_lookup_duration << std::endl;

      // Spawn multiple async http requests
      for (int i = 0; i < repeat_requests_count_; ++i)
      {
        boost::asio::co_spawn(io,
                              this->async_http_call(io, endpoint_iterator, dns_lookup_duration, std::move(parsed_url[0]), std::move(parsed_url[1]),
                                                    std::move(parsed_url[2]), std::move(parsed_url[3]), std::move(post_data_)),
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

boost::asio::awaitable<ResultResponse> HttpClient::async_http_call(boost::asio::io_context& io_context,
                                                                   boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,
                                                                   const std::chrono::duration<double, std::milli>& dns_lookup_duration,
                                                                   const std::string& protocol,
                                                                   const std::string& host,
                                                                   const std::string& port,
                                                                   const std::string& pathParams,
                                                                   const std::string& post_data) const
{
  // Start time measurement
  const auto start_prepare_request_time_point = std::chrono::steady_clock::now();
  ResultResponse result;

  try
  {

    boost::asio::streambuf request;
    // Prepare HTTP request
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

    // Note: the end of prepare request time point is the start of socket connect time point
    const auto end_prepare_request_time_point = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> prepare_request_time_duration = end_prepare_request_time_point - start_prepare_request_time_point;

    // Pre-define to zero
    std::chrono::duration<double, std::milli> socket_connect_time_duration = std::chrono::milliseconds::zero();
    std::chrono::duration<double, std::milli> handshake_time_duration = std::chrono::milliseconds::zero();
    if (protocol.compare("http") == 0)
    {
      // Create and connect the plain TCP socket
      boost::asio::ip::tcp::socket socket(io_context);
      boost::asio::connect(socket, endpoint_iterator);
      const auto end_socket_connect_time_point = std::chrono::steady_clock::now();
      socket_connect_time_duration = end_socket_connect_time_point - end_prepare_request_time_point;
      result = http_request_response(socket, request);
    }
    else if (protocol.compare("https") == 0)
    {
      // Create and connect the socket using the TLS protocol
      boost::asio::ssl::context tls_context(boost::asio::ssl::context::tlsv12_client); // What about v1.3 client?
      // Only allow TLS v1.2 & v1.3
      tls_context.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                              boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1);
      // Set default CA paths
      tls_context.set_default_verify_paths();

      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket(io_context, tls_context);
      boost::asio::connect(socket.next_layer(), endpoint_iterator);

      // Note: end of socket connect time point is the start of the handshake time point
      const auto end_socket_connect_time_point = std::chrono::steady_clock::now();
      socket_connect_time_duration = end_socket_connect_time_point - end_prepare_request_time_point;

      // TODO: Verify TLS connection
      // socket.set_verify_mode(boost::asio::ssl::verify_peer |
      //                        boost::asio::ssl::verify_fail_if_no_peer_cert);
      // Do we also want: set_verify_callback()?

      // Perform TLS handshake
      socket.handshake(boost::asio::ssl::stream_base::client);
      const auto end_handshake_time_point = std::chrono::steady_clock::now();
      handshake_time_duration = end_handshake_time_point - end_socket_connect_time_point;

      result = https_request_response(socket, request);
    }
    else
    {
      std::cerr << "Error: Unsupported protocol (for now). Exit." << std::endl;
      exit(1);
    }
    result.duration.dns = dns_lookup_duration;
    result.duration.prepare_request = prepare_request_time_duration;
    result.duration.connect = socket_connect_time_duration;
    result.duration.handshake = handshake_time_duration;

    // We do not include DNS duration (because DNS is done only once per thread and not for each coroutine!)
    result.duration.total_without_dns =
        (result.duration.prepare_request + result.duration.connect + result.duration.handshake + result.duration.request + result.duration.response);
    // Total with DNS time (altough it's only done once per thread)
    result.duration.total = result.duration.total_without_dns + result.duration.dns;

    std::cout << "Response: " << result.http_version << " " << std::to_string(result.status_code) << " " << result.status_message << std::endl;
    std::cout << "Total duration: " << result.duration.total_without_dns << " (prepare: " << result.duration.prepare_request
              << ", socket connect: " << result.duration.connect << ", handshake: " << result.duration.handshake
              << ", request: " << result.duration.request << ", response: " << result.duration.response << ")" << std::endl;
    std::cout << "Body Content:\n" << result.body << std::endl;
    std::cout << "Headers:\n" << std::endl;
    for (const auto& header : result.headers)
    {
      std::cout << "Name: " << header.first << " Value: " << header.second << std::endl;
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error, exception: " << e.what() << std::endl;
  }
  co_return result;
}

/**
 * Plain HTTP Request helper method
 */
ResultResponse HttpClient::http_request_response(boost::asio::ip::tcp::socket& socket, boost::asio::streambuf& request)
{
  ResultResponse result;
  boost::asio::streambuf response;

  const auto start_request_time_point = std::chrono::steady_clock::now();
  // Send the HTTP request
  boost::asio::write(socket, request);

  // Note: End _request_ time point is now also the start of the _response_ time point
  const auto end_request_time_point = std::chrono::steady_clock::now();
  result.duration.request = end_request_time_point - start_request_time_point;

  // Read until the status line.
  boost::asio::read_until(socket, response, "\r\n");

  // Read the HTTP response
  // TODO: Reuse as much code as possible (also for the https method)
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
      std::cerr << "Error: Unable to read HTTP response: " << ec.message() << std::endl;
    }
  }

  std::stringstream re;
  re << &response;
  result.body = re.str();

  const auto end_response_time_point = std::chrono::steady_clock::now();
  result.duration.response = end_response_time_point - end_request_time_point;

  socket.close();

  return result;
}

/**
 * Encrypted HTTPS Request helper method
 */
ResultResponse HttpClient::https_request_response(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& socket, boost::asio::streambuf& request)
{
  ResultResponse result;
  boost::asio::streambuf response;

  const auto start_request_time_point = std::chrono::steady_clock::now();
  // Send the HTTP request
  boost::asio::write(socket, request);

  // Note: End _request_ time point is now also the start of the _response_ time point
  const auto end_request_time_point = std::chrono::steady_clock::now();
  result.duration.request = end_request_time_point - start_request_time_point;

  // Read until the status line.
  boost::asio::read_until(socket, response, "\r\n");

  // Read the HTTP response
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
      std::cerr << "Error: Unable to read HTTP response: " << ec.message() << std::endl;
    }
  }

  std::stringstream re;
  re << &response;

  result.body = re.str();

  const auto end_response_time_point = std::chrono::steady_clock::now();
  result.duration.response = end_response_time_point - end_request_time_point;

  socket.next_layer().close();

  return result;
}
