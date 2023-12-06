#include <iostream>
#include <vector>
#include <thread>
#include <curl/curl.h>

// Callback function for libcurl to write response data
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    if (static_cast<std::streamsize>(total_size) < 0) {
         throw std::runtime_error("Buffer Size overflowing");
    } else {
        output->append((char*)contents, total_size);
    }
    return total_size;
}

void perform_request(const std::string& url, int repeat_count_multi, const std::string& post_data = "") {
    std::string generic_error;
    std::vector<CURL*> curl_request_handles;
    int still_running = 1;
    int msgs_left; // Don't care
    CURLMsg *msg = NULL;
    std::vector<std::string> get_info_errors;

    // Perform parallel HTTP requests
    CURLM* multi_handle = curl_multi_init();
    if (!multi_handle) {
        std::cerr << "Failed to initialize libcurl multi handle." << std::endl;
        return;
    }

    // Create easy handles and add them to the multi handle
    for (int i = 0; i < repeat_count_multi; ++i) {
        CURL* curl = curl_easy_init();
        if (curl) {
            // TODO: Maybe create some kind of Request object to store stuff into: https://github.com/PCSX2/pcsx2/blob/f6154032c7bbdf130aabb2f3d234237984964fc8/common/HTTPDownloader.h#L42 ?
            // But it seems a bit overkill
            std::string* response_data_ptr = new std::string("");

            // Set the URL
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

            if (!post_data.empty())
            {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
            }

            // https://curl.se/libcurl/c/CURLOPT_WRITEDATA.html
            std::string response_data;
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_data_ptr); // Or: &response_data

	        curl_easy_setopt(curl, CURLOPT_PRIVATE, response_data_ptr);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "RamBam/1.0");

            // Add the easy handle to the multi handle
            // https://curl.se/libcurl/c/curl_multi_add_handle.html 
            curl_multi_add_handle(multi_handle, curl);
            curl_request_handles.push_back(curl);
        } else {
            std::cerr << "Failed to initialize libcurl easy handle." << std::endl;
        }
    }

    do {
        // https://curl.se/libcurl/c/curl_multi_perform.html
        CURLMcode mc = curl_multi_perform(multi_handle, &still_running);

        if (!mc && still_running)
            // wait for activity, timeout or "nothing"
            // https://curl.se/libcurl/c/curl_multi_poll.html
            mc = curl_multi_poll(multi_handle, NULL, 0, 1, NULL);

        if (mc) {
            generic_error = std::string(curl_multi_strerror(mc));
            break;
        }
    } while (still_running);

    // Check if there was no error above.
    //if (generic_error.empty()) {
        while ((msg = curl_multi_info_read(multi_handle, &msgs_left)) != nullptr) {
            if (msg->msg == CURLMSG_DONE) {
                CURL *curl = msg->easy_handle;

                std::string* response_pointer;
                // First we receive the private pointer
                // So even if the status response is NOK, we can free the memory.
                if (curl_easy_getinfo(curl, CURLINFO_PRIVATE, &response_pointer) != CURLE_OK)
                {
                    std::cerr << "curl_easy_getinfo() failed on retrieving private pointer." << std::endl;
                    continue;
                }

                long status_code;
                CURLcode res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
                if (res == CURLE_OK) {
                    auto req_bytes_sent = long{};
                    auto total_time = double{};
                    char* primary_ip = nullptr;

                    if (curl_easy_getinfo(curl, CURLINFO_REQUEST_SIZE, &req_bytes_sent) != CURLE_OK) {
                        std::cerr << "curl_easy_getinfo() failed on request size." << std::endl;
                        continue;
                    }

                    if (curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time) != CURLE_OK) {
                        std::cerr << "curl_easy_getinfo() failed on total time." << std::endl;
                        continue;
                    }

                    if (curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &primary_ip) != CURLE_OK) {
                        std::cerr << "curl_easy_getinfo() failed on total time." << std::endl;
                        continue;
                    }

                    std::cout << "HTTP GET request successful! Bytes sent: " << req_bytes_sent << "\n";
                    std::cout << "Response Body:\n" << *response_pointer << std::endl;
                } else {
                    get_info_errors.push_back("HTTP GET request failed: " +  std::string(curl_easy_strerror(res)));
                }
                delete response_pointer; // free
            } else {
                std::cerr << "Unexpectd multi message: " << msg->msg << std::endl;
            }
        }
    //}

    // Remove the transfers and cleanup the handles
    for (auto& curl : curl_request_handles) {
        curl_multi_remove_handle(multi_handle, curl);
        curl_easy_cleanup(curl);
    }
}

int main() {
    std::string url = "http://localhost/test/";

    // Repeat the requests x times in parallel using threads
    int repeat_thread_count = 1;
    // Repat the requests inside the thread again with x times
    // So a total of: repeat_thread_count * repeat_count_multi
    int repeat_count_multi = 40;

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
