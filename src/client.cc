
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <iterator>
#include <openssl/ssl.h>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "client.h"

using namespace std::chrono_literals;

/**
 * Reference:
 * https://github.com/xtreemfs/xtreemfs/blob/master/cpp/src/rpc/client.cpp
 */

/**
 * \brief HTTP Client Constructor
 */
Client::Client(int repeat_requests_count,
               const std::string& url,
               const std::string& post_data,
               bool verbose,
               bool silent,
               bool verify_peer,
               bool override_verify_tls,
               bool debug_verify_tls,
               long ssl_options)
    : repeat_requests_count_(repeat_requests_count),
      url_(url),
      post_data_(post_data),
      verbose_(verbose),
      silent_(silent),
      verify_peer_(verify_peer),
      override_verify_tls_(override_verify_tls),
      debug_verify_tls_(debug_verify_tls),
      ssl_options_(ssl_options)
{
}

/**
 * \brief Destructor
 */
Client::~Client()
{
}

void Client::spawn_requests() const
{
  try
  {
    boost::regex expression(
        //   proto             host                 port     rest
        "(\?:([^:/\?#]+)://)\?(\\w+[^/\?#:]*)(\?::(\\d+))*(?:)(.*)");
    boost::sregex_token_iterator iter(url_.begin(), url_.end(), expression, {1, 2, 3, 4});
    boost::sregex_token_iterator end;
    std::vector<std::string> parsed_url(iter, end);

    if (parsed_url.size() == 4)
    {
      const auto start_dns_lookup_time_point = std::chrono::steady_clock::now();
      boost::asio::io_context io;
      boost::asio::ip::tcp::resolver resolver(io);
      // Resolve the server hostname and service
      boost::asio::ip::tcp::resolver::query query(parsed_url[1], parsed_url[0]);
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
      const auto end_dns_lookup_time_point = std::chrono::steady_clock::now();
      std::chrono::duration<double, std::milli> dns_lookup_duration = end_dns_lookup_time_point - start_dns_lookup_time_point;

      if (!silent_ && verbose_)
      {
        std::cout << "DNS lookup duration (once per thread): " << dns_lookup_duration << std::endl;
      }

      // If path is empty, then just set to '/'
      if (parsed_url[3].empty())
      {
        parsed_url[3] = '/';
      }

      // Spawn multiple async http requests
      for (int i = 0; i < repeat_requests_count_; ++i)
      {
        boost::asio::co_spawn(io,
                              this->async_request(io, endpoint_iterator, dns_lookup_duration, std::move(parsed_url[0]), std::move(parsed_url[1]),
                                                  std::move(parsed_url[2]), std::move(parsed_url[3]), std::move(post_data_)),
                              boost::asio::detached);
      }
      // Run the tasks concurrently
      io.run();
    }
    else
    {
      std::cerr << "Error: Could not parse the URL correctly. Exit!" << std::endl;
      exit(1);
    }
  }
  catch (std::exception& e)
  {
    std::printf("Exception: %s\n", e.what());
  }
}

boost::asio::awaitable<ResultResponse> Client::async_request(boost::asio::io_context& io_context,
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
      request_stream << "GET " + pathParams + " HTTP/1.0\r\n";
    }
    else
    {
      request_stream << "POST " + pathParams + " HTTP/1.0\r\n";
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
      result = Client::handle_request(socket, request);
      socket.close();
    }
    else if (protocol.compare("https") == 0)
    {
      // Create and connect the socket using the TLS protocol
      // TODO: Give the user more control about the context, like tlsv1.2 maybe?
      boost::asio::ssl::context tls_context(boost::asio::ssl::context::tlsv13_client);
      // Only allow TLS v1.2 & v1.3 by default
      tls_context.set_options(ssl_options_);

      // Verify TLS connection by default
      if (verify_peer_)
      {
        // Verify TLS connection
        tls_context.set_verify_mode(boost::asio::ssl::verify_peer);
        // Set default CA paths
        tls_context.set_default_verify_paths();

        // Verify the remote host's certificate
        if (debug_verify_tls_)
        {
          tls_context.set_verify_callback(boost::bind(&Client::verify_certificate_callback, this, boost::placeholders::_1, boost::placeholders::_2));
        }
        else
        {
          // By default use the built-in host_name_verification()
          tls_context.set_verify_callback(boost::asio::ssl::host_name_verification(host));
        }
      }
      else
      {
        tls_context.set_verify_mode(boost::asio::ssl::context::verify_none);
      }

      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket(io_context, tls_context);

      // Set SNI
      SSL_set_tlsext_host_name(socket.native_handle(), host.c_str());

      boost::asio::connect(socket.next_layer(), endpoint_iterator);

      // Note: end of socket connect time point is the start of the handshake time point
      const auto end_socket_connect_time_point = std::chrono::steady_clock::now();
      socket_connect_time_duration = end_socket_connect_time_point - end_prepare_request_time_point;

      // Perform TLS handshake
      socket.handshake(boost::asio::ssl::stream_base::client);
      const auto end_handshake_time_point = std::chrono::steady_clock::now();
      handshake_time_duration = end_handshake_time_point - end_socket_connect_time_point;

      result = Client::handle_request(socket, request);
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

    // TODO: We return the result, print it outside of this method.
    if (!silent_ && verbose_)
    {
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
      std::cout << "------------------------------------------------------\n\r" << std::endl;
    }
    else if (!silent_)
    {
      // TODO: Something is off with result.reply.status_message (hidden special chars?)
      std::cout << "Response: " << std::to_string(result.reply.status_code) << " in " << result.duration.total_without_dns << std::endl;
    }
  }
  catch (const boost::system::system_error& e)
  {
    std::cerr << "Error: Could not perform the HTTP(s) request: " << e.what() << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: Something went wrong during the request: " << e.what() << std::endl;
  }
  co_return result;
}

/**
 * Debug certificate validation callback method
 */
bool Client::verify_certificate_callback(bool preverified, boost::asio::ssl::verify_context& context) const
{
  X509_STORE_CTX* sctx = context.native_handle();
  X509* cert = X509_STORE_CTX_get_current_cert(sctx);

  X509_NAME* subject_name = X509_get_subject_name(cert);
  BIO* subject_name_out = BIO_new(BIO_s_mem());
  X509_NAME_print_ex(subject_name_out, subject_name, 0, XN_FLAG_RFC2253);

  char *subject_start = NULL, *subject = NULL;
  long subject_length = BIO_get_mem_data(subject_name_out, &subject_start);
  subject = new char[subject_length + 1];
  memcpy(subject, subject_start, subject_length);
  subject[subject_length] = '\0';

  BIO_free(subject_name_out);

  if (!silent_)
    std::cout << "Verifying subject '" << subject << "'." << std::endl;

  int sctx_error = X509_STORE_CTX_get_error(sctx);
  if (sctx_error != 0 && !silent_)
  {
    std::cout << "OpenSSL verify error: " << sctx_error << std::endl;
  }

  if (!silent_)
    std::cout << "Verification of subject '" << subject << "' was " << (preverified ? "successful." : "unsuccessful.")
              << ((!preverified && override_verify_tls_) ? " Overriding because of user settings." : "") << std::endl;

  delete[] subject;

  return preverified || override_verify_tls_;
}

/**
 * \brief Handle HTTP(s) request
 * \param[in] socket Socket connection
 * \param[in] request Request data
 */
template <typename SyncReadStream> ResultResponse Client::handle_request(SyncReadStream& socket, boost::asio::streambuf& request)
{
  ResultResponse result;

  const auto start_request_time_point = std::chrono::steady_clock::now();
  boost::asio::write(socket, request);

  // Note: End _request_ time point is now also the start of the _response_ time point
  const auto end_request_time_point = std::chrono::steady_clock::now();
  result.duration.request = end_request_time_point - start_request_time_point;

  result.reply = Client::parse_response(socket);

  const auto end_response_time_point = std::chrono::steady_clock::now();
  result.duration.response = end_response_time_point - end_request_time_point;

  return result;
}

/**
 * \brief Parse response: HTTP status, headers and body
 * \param[in] socket Socket connection
 */
template <typename SyncReadStream> Reply Client::parse_response(SyncReadStream& socket)
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
  bool chunked = false;
  int response_content_length = -1;
  std::regex content_length_pattern(R"xx(^content-length:\s+(\d+))xx", std::regex_constants::icase);
  std::regex transfer_encoding_pattern(R"xx(^transfer-encoding:\s+chunked)xx", std::regex_constants::icase);
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
    if (std::regex_search(header_line, match, content_length_pattern))
      response_content_length = std::stoi(match[1]);
    if (std::regex_search(header_line, match, transfer_encoding_pattern))
      chunked = true;
  }

  if (chunked)
  {
    std::cerr << "Error: We do not support chunked responses." << std::endl;
  }

  // Get body response using the length indicated by the content-length. Or read all, if header was not present.
  if (response_content_length != -1)
  {
    response_content_length -= response.size();
    if (response_content_length > 0)
      boost::asio::read(socket, response, boost::asio::transfer_exactly(response_content_length));

    std::stringstream re;
    re << &response;
    reply.body = re.str();
  }
  else
  {
    // Read all non-chunked data at once
    boost::system::error_code error;
    boost::asio::read(socket, response, boost::asio::transfer_all(), error);
    // We also ignore stream truncated errors
    if (error != boost::asio::error::eof && error != boost::asio::ssl::error::stream_truncated && error != boost::system::errc::success)
    {
      std::cerr << "Error: Error during reading HTTP response: " << error.message() << std::endl;
    }

    std::stringstream re;
    re << &response;
    reply.body = re.str();
  }

  return reply;
}
