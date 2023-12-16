#include <cpp20_http_client.hpp>
#include <cxxopts.hpp>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

#include "project_config.h"

using namespace http_client;
using namespace std::chrono_literals;

/**
Looking into: https://github.com/alibaba/yalantinglibs#coro_http
*/

void process_request(const Response& response)
{
  std::cout << "Status code: " << static_cast<int>(response.get_status_code()) << std::endl;
  if (response.get_status_code() == StatusCode::Ok)
  {
    std::cout << "Response body: " << response.get_body_string();
    std::cout << "Headers: " << response.get_headers_string() << std::endl;
    std::cout << "Status message: " << response.get_status_message() << "\n\r\n" << std::endl;
  }
}

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

      if (response.get_status_code() == StatusCode::MovedPermanently || response.get_status_code() == StatusCode::Found)
      {
        if (auto const new_url = response.get_header_value("location"))
        {
          auto new_response_future = get(*new_url).add_header({.name = "User-Agent", .value = "RamBam/1.0"}).send_async<512>();
          auto const new_response = new_response_future.get();
          process_request(new_response);
        }
        else
        {
          std::cerr << "Error: Got 301 or 302, but no new URL." << std::endl;
          process_request(response);
        }
      }
      else
      {
        process_request(response);
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << "Error: Unable to fetch URL with error:" << e.what() << std::endl;
    }
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
  cxxopts::Options options("rambam", "Stress test your API or website");

  // clang-format off
  options.add_options()
    ("b,bar", "Param bar", cxxopts::value<std::string>())
    ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
    ("f,foo", "Param foo", cxxopts::value<int>()->default_value("10"))
    ("urls", "URL(s) under test", cxxopts::value<std::vector<std::string>>())
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
  }
  catch (const cxxopts::OptionParseException& x)
  {
    std::cerr << "Error: " << x.what() << '\n';
    std::cout << options.help() << std::endl;
    return EXIT_FAILURE;
  }

  std::string url = "http://localhost/test/";

  // Repeat the requests x times in parallel using threads
  int repeat_thread_count = 1;
  // Repat the requests inside the thread again with x times
  // So a total of: repeat_thread_count * repeat_requests_count
  int repeat_requests_count = 4;

  // Perform parallel HTTP requests
  std::vector<std::thread> threads;
  threads.reserve(repeat_thread_count);
  for (int i = 0; i < repeat_thread_count; ++i)
  {
    threads.emplace_back([url, repeat_requests_count]() {
      // Perform the HTTP request for the current thread
      perform_request(url, repeat_requests_count);
    });
  }

  // Wait for all threads to finish
  for (auto& thread : threads)
  {
    thread.join();
  }

  return 0;
}
