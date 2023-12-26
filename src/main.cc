#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "project_config.h"
#include "thread.h"

/**
 * \brief Helper function to parse the application input options.
 */
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

/**
 * \brief Main entry point
 */
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
    // TODO: Support TLS requests: https://github.com/electroneum/electroneum/blob/master/contrib/epee/src/net_ssl.cpp#L283
    // See also: https://github.com/Icinga/icinga2/blob/master/lib/base/tlsutility.cpp and https://github.com/NewCapital/catapult-server/blob/8d5be02a823d98dba6610f7183bd08f4f2d331d5/tools/ssl/SslClient.cpp#L41
    // or: https://github.com/rapp-project/rapp-api/blob/e842401e1a83754a51874b996646355e627a9b18/cpp/includes/cloud/asio_socket_https/asio_socket_https.cpp#L36
    //url = "https://melroy.org/";
    url = "http://localhost/test/";
  }
  catch (const cxxopts::exceptions::exception& error)
  {
    std::cerr << "Error: " << error.what() << '\n';
    std::cout << options.help() << std::endl;
    return EXIT_FAILURE;
  }

  // Repeat the requests x times in parallel using threads
  int repeat_thread_count = 1;
  // Repat the requests inside the thread again with x times
  // So a total of: repeat_thread_count * repeat_requests_count
  int repeat_requests_count = 2;

  // Start threads, blocking call until all threads are finished
  Thread::start_threads(repeat_thread_count, repeat_requests_count, url);
  return 0;
}
