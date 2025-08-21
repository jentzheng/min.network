#include "webrtc_client.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <random>

std::atomic<bool> g_should_exit { false };

void signal_handler(int signal)
{
    if (signal == SIGINT) {
        std::cout << "\n[LOG] SIGINT received, exiting..." << std::endl;
        g_should_exit = true;
    }
}

int main()
{
    std::signal(SIGINT, signal_handler);
    std::cout << "webrtc_cilent conosle demo" << std::endl;

    std::cout << "Press Ctrl+C to exit..." << std::endl;

    WebRTCClient client([](const std::string& msg) {
			// logger
			std::cout << msg << std::endl; },
        [](const std::string& msg) {
            // dc callback
            std::cout << msg << std::endl;
        },
        [](const std::string& remote_username, const uint8_t* argb, size_t size, int width, int height) {
            // argb callback, print every 5 secs..
            static auto last_print = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_print).count() >= 5) {
                std::cout << "[argb cb]: " << remote_username << " " << size << std::endl;
                last_print = now;
            }
        });

    std::string host = "ws://localhost:5173/ws";

    client.connect(host, "C++Client#1");

    std::thread input_thread([&]() {
        std::string line;
        while (!g_should_exit) {
            std::getline(std::cin, line);
            if (line == "1") {
                std::cout << "[Debug] Connect triggered" << std::endl;
                client.connect(host, "C++Client");
            } else if (line == "2") {
                std::cout << "[Debug] Disconnect triggered" << std::endl;
                client.disconnect();
            }
        }
    });

    const std::vector<std::pair<int, int>> dims = {
        { 1920, 1080 },
        { 800, 600 },
        { 320, 240 },
    };
    size_t dim_idx = 0;
    auto last_switch = std::chrono::steady_clock::now();

    int width = dims[dim_idx].first;
    int height = dims[dim_idx].second;
    int planes = 4;
    int size = width * height * planes;

    std::vector<uint8_t> argb_data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    while (!g_should_exit) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_switch).count() >= 5) {
            dim_idx = (dim_idx + 1) % dims.size();
            width = dims[dim_idx].first;
            height = dims[dim_idx].second;
            size = width * height * planes;
            argb_data.resize(size);
            last_switch = now;
            // std::cout << "[Debug] Switch to " << width << "x" << height << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
        uint8_t r = dis(gen);
        uint8_t g = dis(gen);
        uint8_t b = dis(gen);
        for (int i = 0; i < size; i += 4) {
            argb_data[i + 0] = 255;
            argb_data[i + 1] = r;
            argb_data[i + 2] = g;
            argb_data[i + 3] = b;
        }

        client.capture_matrix(argb_data.data(), width, height, planes);
    }

    if (input_thread.joinable()) {
        input_thread.join();
    }

    std::cout << "[Debug] Clean exit......" << std::endl;

    return 0;
}
