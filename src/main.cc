#include <cxxopts.hpp>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp> 

#include "project_config.h"

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
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

awaitable<HttpResponse> async_http_call(boost::asio::io_context& io_context,
                                        const std::string& protocol,
                                        const std::string& host,
                                        const std::string& port,
                                        const std::string& path,
                                        const std::string& file,
                                        const std::string& parameters,
                                        const std::string& post_data)
{
  HttpResponse result;

  try
  {
    const auto start_time_point = std::chrono::steady_clock::now();
    // Resolve the server hostname and service
    tcp::resolver resolver(io_context);
    auto endpoint_iterator = co_await resolver.async_resolve(host, protocol, boost::asio::use_awaitable);

    // Create and connect the socket
    tcp::socket socket(io_context);
    co_await boost::asio::async_connect(socket, endpoint_iterator, boost::asio::use_awaitable);
    // Compose the HTTP request
    std::string request = "GET " + path + " HTTP/1.1\r\n"
                          "Host: localhost\r\n"
                          "Connection: close\r\n\r\n";

    // Send the HTTP request
    co_await boost::asio::async_write(socket, boost::asio::buffer(request), boost::asio::use_awaitable);

    // Read and print the HTTP response
    boost::asio::streambuf response;
    // Get till all the headers
    co_await boost::asio::async_read_until(socket, response, "\r\n\r\n", boost::asio::use_awaitable);

    std::istream response_stream(&response);
    response_stream >> result.http_version;
    response_stream >> result.status_code;

    std::string status_message;
    std::getline(response_stream, result.status_message);

    // Extract headers
    std::string header_line;
    while (std::getline(response_stream, header_line) && header_line != "\r")
    {
      size_t colon_pos = header_line.find(':');
      if (colon_pos != std::string::npos)
      {
        std::string header_name = header_line.substr(0, colon_pos);
        std::string header_value = header_line.substr(colon_pos + 2); // Skip ': ' after colon
        result.headers.emplace_back(std::move(header_name), std::move(header_value));
      }
    }

    // Extract body content
    std::string body(boost::asio::buffers_begin(response.data()), boost::asio::buffers_end(response.data()));
    response.consume(response.size());
    // Close connection
    socket.close();

    const auto end_time_point = std::chrono::steady_clock::now();
    result.total_time_duration = end_time_point - start_time_point;

    result.body = std::move(body);

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

/**
void perform_request(const std::string& url, int repeat_requests_count, const std::string& post_data = "")
{
  // Store the asynchronous responses
  std::vector<std::future<Response>> futures;
  futures.reserve(repeat_requests_count);

  // Loop over the nr of repeats
  for (int i = 0; i < repeat_requests_count; ++i)
  {
    std::future<Response> response_future;
    if (post_data.empty())
    {
      // Get request by default
      response_future = get(url).add_header({.name = "User-Agent", .value = "RamBam/1.0"}).send_async<512>();
    }
    else
    {
      // JSON Post request
      response_future = post(url)
                            .add_header({.name = "User-Agent", .value = "RamBam/1.0"})
                            .add_header({.name = "Content-Type", .value = "application/json"})
                            .set_body(post_data)
                            .send_async<512>();
    }
    // Push the future into the vector store
    futures.emplace_back(std::move(response_future));
  }

  for (auto& future : futures)
  {
    while (future.wait_for(1ms) != std::future_status::ready)
    {
      // Wait for the response to become ready
    }

    try
    {
      auto response = future.get();
      process_request(response);
    }
    catch (const std::exception& e)
    {
      std::cerr << "Error: Unable to fetch URL with error:" << e.what() << std::endl;
    }
  }
}
*/

void spawn_http_requests(int repeat_requests_count, const std::string& url, const std::string& post_data = "")
{
  try
  {
    std::vector<std::string> parsed_url;
    boost::regex expression(
        //   proto                 host               port
        "^(\?:([^:/\?#]+)://)\?(\\w+[^/\?#:]*)(\?::(\\d+))\?"
        //   path                  file       parameters
        "(/\?(\?:[^\?#/]*/)*)\?([^\?#]*)\?(\\\?(.*))\?");

    // Make a non-const copy of the string
    std::string url_copy = url;
    boost::algorithm::trim(url_copy);
    if (boost::regex_split(std::back_inserter(parsed_url), url_copy, expression))
    {
      boost::asio::io_context io;
      for (int i = 0; i < repeat_requests_count; ++i)
      {
        boost::asio::co_spawn(io,
                              async_http_call(io, std::move(parsed_url[0]), std::move(parsed_url[1]), std::move(parsed_url[2]),
                                              std::move(parsed_url[3]), std::move(parsed_url[4]), std::move(parsed_url[5]), std::move(post_data)),
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
  int repeat_requests_count = 30;

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
