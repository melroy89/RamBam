
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <iterator>
#include <openssl/ssl.h>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "client.h"
#include "project_config.h"

using namespace std::chrono_literals;

/**
 * \brief HTTP Client Constructor
 */
Client::Client(const Settings& settings, boost::asio::io_context& io_context)
    : repeat_requests_count_(settings.repeat_requests_count),
      duration_sec_(settings.duration_sec),
      url_(settings.url),
      post_data_(settings.post_data),
      verbose_(settings.verbose),
      silent_(settings.silent),
      verify_peer_(settings.verify_peer),
      override_verify_tls_(settings.override_verify_tls),
      debug_verify_tls_(settings.debug),
      ssl_options_(settings.ssl_options),
      io_context_(io_context)
{
  if (ssl_options_ == 0)
  {
    // Default Asio SSL options
    ssl_options_ = (boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3 |
                    boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1);
  }

  try
  {
    std::regex expression("([\\w]+)://([^:/]+)(?::(\\d+))?(/.*)?");
    std::smatch matched_url;
    if (std::regex_match(url_, matched_url, expression))
    {
      const auto start_dns_lookup_time_point = std::chrono::steady_clock::now();
      boost::asio::ip::tcp::resolver resolver(io_context_);
      // Resolve the server hostname and service
      boost::asio::ip::tcp::resolver::query query(matched_url[2], matched_url[1]);
      endpoint_iterator_ = resolver.resolve(query);
      const auto end_dns_lookup_time_point = std::chrono::steady_clock::now();
      dns_lookup_duration_ = end_dns_lookup_time_point - start_dns_lookup_time_point;

      if (!silent_ && verbose_)
      {
        std::cout << "DNS lookup duration: " << dns_lookup_duration_.count() << "ms" << std::endl;
      }

      // If path is empty, then just set to '/'
      std::string rest = matched_url[4].str().empty() ? std::string("/") : matched_url[4];

      // Store the parsed URL
      protocol_ = std::move(matched_url[1]);
      host_ = std::move(matched_url[2]);
      port_ = std::move(matched_url[3]);
      path_params_ = std::move(rest);
    }
    else
    {
      std::cerr << "Error: URL did not match the expected pattern. Exit!" << std::endl;
      exit(1);
    }
  }
  catch (std::exception& e)
  {
    std::printf("Exception: %s\n", e.what());
  }
}

/**
 * \brief Destructor
 */
Client::~Client()
{
}

/**
 * \brief Do the HTTP(s) request reusing the same settings for each request.
 */
void Client::do_request() const
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
    if (empty(post_data_))
    {
      request_stream << "GET " + path_params_ + " HTTP/1.0\r\n";
    }
    else
    {
      request_stream << "POST " + path_params_ + " HTTP/1.0\r\n";
    }
    std::string hostname(host_);
    if (!empty(port_))
    {
      hostname.append(":" + port_);
    }
    request_stream << "Host: " + hostname + "\r\n";
    request_stream << "User-Agent: RamBam/" << PROJECT_VER << "\r\n";
    if (!empty(post_data_))
    {
      request_stream << "Content-Type: application/json; charset=utf-8\r\n";
      request_stream << "Accept: */*\r\n"; // We should be able to override this (eg. application/json)
      request_stream << "Content-Length: " << post_data_.length() << "\r\n";
    }
    request_stream << "Connection: close\r\n\r\n"; // End is a double line feed
    if (!empty(post_data_))
    {
      request_stream << post_data_;
    }

    // Note: the end of prepare request time point is the start of socket connect time point
    const auto end_prepare_request_time_point = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> prepare_request_time_duration = end_prepare_request_time_point - start_prepare_request_time_point;

    // Pre-define to zero
    std::chrono::duration<double, std::milli> socket_connect_time_duration = std::chrono::milliseconds::zero();
    std::chrono::duration<double, std::milli> handshake_time_duration = std::chrono::milliseconds::zero();
    if (protocol_.compare("http") == 0)
    {
      // Create and connect the plain TCP socket
      boost::asio::ip::tcp::socket socket(io_context_);
      boost::asio::connect(socket, endpoint_iterator_);
      const auto end_socket_connect_time_point = std::chrono::steady_clock::now();
      socket_connect_time_duration = end_socket_connect_time_point - end_prepare_request_time_point;
      result = Client::handle_request(socket, request);
      socket.close();
    }
    else if (protocol_.compare("https") == 0)
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
          tls_context.set_verify_callback(boost::asio::ssl::host_name_verification(host_));
        }
      }
      else
      {
        tls_context.set_verify_mode(boost::asio::ssl::context::verify_none);
      }

      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket(io_context_, tls_context);

      // Set SNI
      SSL_set_tlsext_host_name(socket.native_handle(), host_.c_str());

      boost::asio::connect(socket.next_layer(), endpoint_iterator_);

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
    result.duration.dns = dns_lookup_duration_;
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
      std::cout << "Total duration: " << result.duration.total_without_dns.count() << "ms (prepare: " << result.duration.prepare_request.count()
                << "ms, socket connect: " << result.duration.connect.count() << "ms, handshake: " << result.duration.handshake.count()
                << "ms, request: " << result.duration.request.count() << "ms, response: " << result.duration.response.count() << "ms)" << std::endl;
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
      std::cout << "Response: " << std::to_string(result.reply.status_code) << " in " << result.duration.total_without_dns.count() << "ms"
                << std::endl;
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
