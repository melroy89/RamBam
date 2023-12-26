
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
 * More detailed implementation, see:
 * https://github.com/cpp-netlib/cpp-netlib/blob/552ce94bd91c055f11ba524adf0ca0712063d711/boost/network/protocol/http/client/connection/ssl_delegate.ipp
 */

/**
 * \brief Constructor
 */
HttpClient::HttpClient(int repeat_requests_count, const std::string& url, const std::string& post_data, long ssl_options, bool verify_peer)
    : repeat_requests_count_(repeat_requests_count),
      url_(url),
      post_data_(post_data),
      ssl_options_(ssl_options),
      verify_peer_(verify_peer)
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
    // Prepare HTTP request
    boost::asio::streambuf request;
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
      result = HttpClient::handle_request(socket, request);
      socket.close();
    }
    else if (protocol.compare("https") == 0)
    {
      // Create and connect the socket using the TLS protocol
      boost::asio::ssl::context tls_context(boost::asio::ssl::context::tlsv12_client); // What about v1.3 client?
      // Only allow TLS v1.2 & v1.3 by default
      tls_context.set_options(ssl_options_);
      if (verify_peer_)
      {
        // Verify TLS connection
        // tls_context.set_verify_mode(boost::asio::ssl::verify_peer);
        // Set default CA paths
        tls_context.set_default_verify_paths();
      }
      else
      {
        tls_context.set_verify_mode(boost::asio::ssl::context::verify_none);
      }

      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket(io_context, tls_context);
      boost::asio::connect(socket.next_layer(), endpoint_iterator);

      // Note: end of socket connect time point is the start of the handshake time point
      const auto end_socket_connect_time_point = std::chrono::steady_clock::now();
      socket_connect_time_duration = end_socket_connect_time_point - end_prepare_request_time_point;

      // Verify the remote host's certificate.
      if (verify_peer_)
      {
        socket.set_verify_callback(boost::asio::ssl::host_name_verification(host));
      }

      // Perform TLS handshake
      socket.handshake(boost::asio::ssl::stream_base::client);
      const auto end_handshake_time_point = std::chrono::steady_clock::now();
      handshake_time_duration = end_handshake_time_point - end_socket_connect_time_point;

      result = HttpClient::handle_request(socket, request);
      socket.next_layer().close();
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

    std::cout << "Response: " << result.reply.http_version << " " << std::to_string(result.reply.status_code) << " " << result.reply.status_message
              << std::endl;
    std::cout << "Total duration: " << result.duration.total_without_dns << " (prepare: " << result.duration.prepare_request
              << ", socket connect: " << result.duration.connect << ", handshake: " << result.duration.handshake
              << ", request: " << result.duration.request << ", response: " << result.duration.response << ")" << std::endl;
    std::cout << "Body Content:\n" << result.reply.body << std::endl;
    std::cout << "Headers:\n" << std::endl;
    for (const auto& header : result.reply.headers)
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
 * \brief Handle HTTP(s) request
 * \param[in] socket Socket connection
 * \param[in] request Request data
 */
template <typename SyncReadStream> ResultResponse HttpClient::handle_request(SyncReadStream& socket, boost::asio::streambuf& request)
{
  ResultResponse result;

  const auto start_request_time_point = std::chrono::steady_clock::now();
  boost::asio::write(socket, request);

  // Note: End _request_ time point is now also the start of the _response_ time point
  const auto end_request_time_point = std::chrono::steady_clock::now();
  result.duration.request = end_request_time_point - start_request_time_point;

  result.reply = HttpClient::parse_response(socket);

  const auto end_response_time_point = std::chrono::steady_clock::now();
  result.duration.response = end_response_time_point - end_request_time_point;

  return result;
}

/**
 * \brief Parse response: HTTP status, headers and body
 * \param[in] socket Socket connection
 */
template <typename SyncReadStream> Reply HttpClient::parse_response(SyncReadStream& socket)
{
  Reply reply;
  boost::asio::streambuf response;

  // Read until the status line
  boost::asio::read_until(socket, response, "\r\n");

  // Get HTTP Status lines
  std::istream response_stream(&response);
  response_stream >> reply.http_version;
  response_stream >> reply.status_code;
  std::getline(response_stream, reply.status_message);

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
      reply.headers.emplace_back(std::move(header_name), std::move(header_value));
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
  reply.body = re.str();

  return reply;
}
