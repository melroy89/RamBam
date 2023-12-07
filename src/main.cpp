#include <future>
#include <iostream>
#include <vector>
#include <thread>

#include <cpp20_http_client.hpp>

using namespace http_client;
using namespace std::chrono_literals;

void process_request(const Response& response) {
    std::cout << "Status code: " << static_cast<int>(response.get_status_code()) << std::endl;
    if (response.get_status_code()  == StatusCode::Ok) {
        std::cout << "Response: " << response.get_body_string() << std::endl;
    }
}

void perform_request(const std::string& url, int repeat_requests_count, const std::string& post_data = "") {
    // Store the asynchronous responses
    std::vector<std::future<Response>> futures;
    futures.reserve(repeat_requests_count);

    // Loop over the nr of repeats
    for (int i = 0; i < repeat_requests_count; ++i) {
        std::future<Response> response_future;
         if (post_data.empty()) {
            // Get request by default
            response_future = get(url)
                .add_header({.name="User-Agent", .value="RamBam/1.0"})
                .send_async<512>();
        } else {
            // JSON Post request
            response_future = post(url)
                .add_header({.name="User-Agent", .value="RamBam/1.0"})
                .add_header({.name="Content-Type", .value="application/json"})
                .set_body(post_data)
                .send_async<512>();
        }
        // Push the future into the vector store
        futures.emplace_back(std::move(response_future));
    }

    for (auto& future : futures) {
        while (future.wait_for(1ms) != std::future_status::ready) {
            // Wait for the response to become ready
        }

        try {
            auto response = future.get();

            if (response.get_status_code() == StatusCode::MovedPermanently ||
                response.get_status_code() == StatusCode::Found) {
                if (auto const new_url = response.get_header_value("location")) {
                    auto new_response_future = get(*new_url)
                        .add_header({.name="User-Agent", .value="RamBam/1.0"})
                        .send_async<512>();
                    auto const new_response = new_response_future.get();
                    process_request(new_response);
                } else {
                    std::cerr << "Error: Got 301 or 302, but no new URL." << std::endl;
                    process_request(response);
                }
            } else {
                process_request(response);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: Unable to fetch URL with error:" << e.what() << std::endl;
        }
    }
}

int main() {
    std::string url = "http://localhost/test/";

    // Repeat the requests x times in parallel using threads
    int repeat_thread_count = 6;
    // Repat the requests inside the thread again with x times
    // So a total of: repeat_thread_count * repeat_requests_count
    int repeat_requests_count = 80;

    // Perform parallel HTTP requests
    std::vector<std::thread> threads;
    threads.reserve(repeat_thread_count);
    for (int i = 0; i < repeat_thread_count; ++i) {
        threads.emplace_back([url, repeat_requests_count]() {
            // Perform the HTTP request for the current thread
            perform_request(url, repeat_requests_count);
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
