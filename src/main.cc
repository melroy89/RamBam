#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/regex.hpp>

#include "project_config.h"

using namespace std::chrono_literals;

struct HttpResponse
{
  std::string http_version;
  unsigned int status_code;
  std::string status_message;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
  std::chrono::duration<double, std::milli> total_time_duration;
};

boost::asio::awaitable<HttpResponse> async_http_call(boost::asio::io_context& io_context,
                                                     boost::asio::ip::tcp::resolver::iterator& endpoint_iterator,
                                                     const std::string& host,
                                                     const std::string& port,
                                                     const std::string& pathParams,
                                                     const std::string& post_data)
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

void spawn_http_requests(int repeat_requests_count, const std::string& url, const std::string& post_data = "")
{
  try
  {
    std::vector<std::string> parsed_url;
    boost::regex expression(
        //   proto             host                 port     rest
        "(\?:([^:/\?#]+)://)\?(\\w+[^/\?#:]*)(\?::(\\d+))*(?:)(.*)");
    // Make a non-const copy of the string
    std::string url_copy = url;
    boost::algorithm::trim(url_copy);
    if (boost::regex_split(std::back_inserter(parsed_url), url_copy, expression))
    {
      boost::asio::io_context io;
      boost::asio::ip::tcp::resolver resolver(io);
      // Resolve the server hostname and service
      boost::asio::ip::tcp::resolver::query query(parsed_url[1], parsed_url[0]);
      boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

      for (int i = 0; i < repeat_requests_count; ++i)
      {
        boost::asio::co_spawn(io,
                              async_http_call(io, endpoint_iterator, std::move(parsed_url[1]), std::move(parsed_url[2]), std::move(parsed_url[3]),
                                              std::move(post_data)),
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

void processArguments(const cxxopts::ParseResult& result, cxxopts::Options& options)
{
  if (result.count("version"))
  {
    std::cout << "RamBam version " << PROJECT_VER << '\n';
    exit(0);
  }

  if (result.count("help"))
  {
    std::cout << options.help() << std::endl;
    exit(0);
  }

  // TODO: will be mandatory
  // if (!result.count("urls"))
  //{
  //  std::cerr << "error: missing URL(s) as last argument\n";
  //  std::cout << options.help() << std::endl;
  //  exit(0);
  //}

  if (result.count("urls"))
  {
    for (const auto& url : result["urls"].as<std::vector<std::string>>())
    {
      std::cout << "URL: " << url << std::endl;
    }
  }
}

int main(int argc, char* argv[])
{
  std::string url;

  cxxopts::Options options("rambam", "Stress test your API or website");

  // clang-format off
  options.add_options()
    ("b,bar", "Param bar", cxxopts::value<std::string>())
    ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
    ("f,foo", "Param foo", cxxopts::value<int>()->default_value("10"))
    ("urls", "URL(s) under test (space separated)", cxxopts::value<std::vector<std::string>>())
    ("v,version", "Show the version")
    ("h,help", "Print usage");
  // clang-format on 
  options.positional_help("<URL(s)>");
  options.parse_positional({"urls"});
  options.show_positional_help();

  try
  {
    auto result = options.parse(argc, argv);
    processArguments(result, options);
    // TODO: Assign argument to URL
    url = "http://localhost/test/";
  }
  catch (const cxxopts::exceptions::exception& error)
  {
    std::cerr << "Error: " << error.what() << '\n';
    std::cout << options.help() << std::endl;
    return EXIT_FAILURE;
  }

  // Repeat the requests x times in parallel using threads
  int repeat_thread_count = 4;
  // Repat the requests inside the thread again with x times
  // So a total of: repeat_thread_count * repeat_requests_count
  int repeat_requests_count = 180;

  // Perform parallel HTTP requests using C++ Threads
  std::vector<std::thread> threads;
  threads.reserve(repeat_thread_count);
  for (int i = 0; i < repeat_thread_count; ++i)
  {
    threads.emplace_back([url, repeat_requests_count]() {
      // Perform the HTTP request for the current thread
      spawn_http_requests(repeat_requests_count, url);
    });
  }

  // Wait for all threads to finish
  for (auto& thread : threads)
  {
    thread.join();
  }

  return 0;
}
