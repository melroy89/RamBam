#include <iostream>
#include <vector>
#include <thread>

#include <cpp20_http_client.hpp>

void perform_request(const std::string& initial_url, int repeat_count_multi, const std::string& post_data = "") {
    std::string url (initial_url);
    auto response_count = 0;
    while (true) {
        http_client::Response response;
        if (post_data.empty()) {
            // Get request by default
            response = http_client::get(url)
                .add_header({.name="User-Agent", .value="RamBam/1.0"})
                .send();
        } else {
            // JSON Post request
            response = http_client::post(url)
                .add_header({.name="User-Agent", .value="RamBam/1.0"})
                .add_header({.name="Content-Type", .value="application/json"})
                .set_body(post_data)
                .send();
        }

        std::cout << "\nHeaders " << response_count++ << ": \n" << response.get_headers_string() << '\n';

        // Follow 30x status codes
        if (response.get_status_code() == http_client::StatusCode::MovedPermanently ||
            response.get_status_code() == http_client::StatusCode::Found) 
        {
            if (auto const new_url = response.get_header_value("location")) {
                url = *new_url; // Override URL
                continue;
            }
        } else {
            std::cout << "Response: " << response.get_body_string() << std::endl;
        }
        break;
    }
}

int main() {
    std::string url = "http://localhost/test/";

    // Repeat the requests x times in parallel using threads
    int repeat_thread_count = 40;
    // Repat the requests inside the thread again with x times
    // So a total of: repeat_thread_count * repeat_count_multi
    int repeat_count_multi = 1;

    // Perform parallel HTTP requests
    std::vector<std::thread> threads;
    for (int i = 0; i < repeat_thread_count; ++i) {
        threads.emplace_back([url, repeat_count_multi]() {
            // Perform the HTTP request for the current thread
            perform_request(url, repeat_count_multi);
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
