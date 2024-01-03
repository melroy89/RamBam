#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "handler.h"
#include "project_config.h"
#include "settings_struct.h"

/**
 * \brief Process the command line arguments
 * \param result The result of the command line parsing
 * \param options The options object
 * \return The settings struct
 */
Settings process_arguments(const cxxopts::ParseResult& result, cxxopts::Options& options)
{
  Settings settings{};

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
  settings.repeat_requests_count = result["request-count"].as<int>();
  if (result.count("duration"))
    settings.duration_sec = result["duration"].as<int>();
  if (result.count("post"))
    settings.post_data = result["post"].as<std::string>();
  settings.verify_peer = !(result["disable-peer-verify"].as<bool>());
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
    ("t,thread-count", "Thread count the default is zero, but that use the supported number of current threads of the hardware", cxxopts::value<int>()->default_value("0"))
    ("r,request-count", "Total amount of requests", cxxopts::value<int>()->default_value("10000"))
    ("p,post", "Post JSON data (request will be POST instead of GET)", cxxopts::value<std::string>())
    ("d,duration", "Test duration in seconds", cxxopts::value<int>())
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
    Settings settings = process_arguments(result, options);
    // Start threads, blocking call until all threads are finished
    Handler::start_threads(settings);
  }
  catch (const cxxopts::exceptions::exception& error)
  {
    std::cerr << "Error: " << error.what() << '\n';
    std::cout << options.help() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
