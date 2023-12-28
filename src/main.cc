#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "project_config.h"
#include "settings_struct.h"
#include "thread.h"

/**
 * \brief Helper function to parse the application input options.
 */
Settings processArguments(const cxxopts::ParseResult& result, cxxopts::Options& options)
{
  Settings settings;

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

  // Repeat the requests x times in parallel using threads
  settings.repeat_thread_count = result["thread-count"].as<int>();
  // Repeat the requests inside each thread again with x times
  // So a total of: repeat_thread_count * repeat_requests_count
  settings.repeat_requests_count = result["request-count"].as<int>();
  settings.disable_peer_verification = result["disable-peer-verify"].as<bool>();
  settings.override_verify_tls = result["override-verify-tls"].as<bool>();
  settings.debug = result["debug"].as<bool>();
  settings.silent = result["silent"].as<bool>();

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
      // TODO: ...
      std::cout << "URL: " << url << std::endl;
    }
  }
  else
  {
    // TODO: This is a manual override, will be removed in the future
    settings.url = "https://melroy.org/";
    // settings.url = "http://localhost/test/";
  }
  return settings;
}

/**
 * \brief Main entry point
 */
int main(int argc, char* argv[])
{
  cxxopts::Options options("rambam", "Stress test your API or website");
  // clang-format off
  options.add_options()
    ("s,silent", "Silent (no output)", cxxopts::value<bool>()->default_value("false"))    
    ("t,thread-count", "Thread count", cxxopts::value<int>()->default_value("4"))
    ("r,request-count", "Request count PER thread, meaning:\nTotal requests = thread count * request count", cxxopts::value<int>()->default_value("10"))
    ("d,debug", "Enable debugging (eg. debug TLS)", cxxopts::value<bool>()->default_value("false"))
    ("p,disable-peer-verify", "Disable peer certificate verification", cxxopts::value<bool>()->default_value("false"))
    ("o,override-verify-tls", "Override TLS peer certificate verification", cxxopts::value<bool>()->default_value("false"))
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
    Settings settings = processArguments(result, options);
    if (!settings.silent) {
      std::cout << "==========================================" << std::endl;
      std::cout << "URL under test: " << settings.url << std::endl;
      std::cout << "Using total threads: " << settings.repeat_thread_count << std::endl;
      std::cout << "==========================================" << std::endl;
    }
    // Start threads, blocking call until all threads are finished
    Thread::start_threads(settings);
  }
  catch (const cxxopts::exceptions::exception& error)
  {
    std::cerr << "Error: " << error.what() << '\n';
    std::cout << options.help() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
