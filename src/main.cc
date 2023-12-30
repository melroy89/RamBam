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
  if (result.count("post"))
    settings.post_data = result["post"].as<std::string>();
  settings.disable_peer_verification = result["disable-peer-verify"].as<bool>();
  settings.override_verify_tls = result["override-verify-tls"].as<bool>();
  settings.verbose = result["verbose"].as<bool>();
  settings.silent = result["silent"].as<bool>();
  settings.debug = result["debug"].as<bool>();

  if (result.count("urls"))
  {
    for (const std::string& url : result["urls"].as<std::vector<std::string>>())
    {
      // TODO: ... support multiple URLs
      if (url.compare(0, 7, "http://") != 0 && url.compare(0, 8, "https://") != 0)
      {
        // Assume HTTPS
        settings.url = "https://" + url;
      }
      else
      {
        settings.url = url;
      }
      break;
    }
  }
  else
  {
    // TODO: This is a manual override, will be removed in the future
    // For testing only now...
    settings.url = "http://localhost/test/";

    // TODO: will be mandatory
    //  std::cerr << "error: missing URL(s) as last argument\n";
    //  std::cout << options.help() << std::endl;
    //  exit(0);
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
    ("v,verbose", "Verbose (More output)", cxxopts::value<bool>()->default_value("false"))
    ("s,silent", "Silent (No output)", cxxopts::value<bool>()->default_value("false"))
    ("d,duration", "Test duration in seconds", cxxopts::value<int>()->default_value("10"))
    ("t,thread-count", "Thread count", cxxopts::value<int>()->default_value("2"))
    ("r,request-count", "Request count PER thread, meaning:\nTotal requests = thread count * request count", cxxopts::value<int>()->default_value("5"))
    ("p,post", "Post JSON data (request will be POST instead of GET)", cxxopts::value<std::string>())
    ("D,debug", "Enable debugging (eg. debug TLS)", cxxopts::value<bool>()->default_value("false"))
    ("disable-peer-verify", "Disable peer certificate verification", cxxopts::value<bool>()->default_value("false"))
    ("o,override-verify-tls", "Override TLS peer certificate verification", cxxopts::value<bool>()->default_value("false"))
    ("urls", "URL(s) under test (space separated)", cxxopts::value<std::vector<std::string>>())
    ("version", "Show the version")
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
      std::cout << "Total threads: " << settings.repeat_thread_count << ", with requests per thread: " << settings.repeat_requests_count <<  std::endl ;
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
