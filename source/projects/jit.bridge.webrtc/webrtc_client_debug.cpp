#include "webrtc_client.h"
#include <atomic>
#include <csignal>
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

	std::cout << "=== Simple Test ===" << std::endl;
	auto logger = [](const std::string& msg) { std::cout << msg << std::endl; };
	auto dc_logger = [](const std::string& msg) { std::cout << msg << std::endl; };
	auto argb_cb = [](const uint8_t* argb, size_t size, int width, int height) {
		std::cout << "[argb cb]: " << width << " " << height << " " << size << std::endl;
	};

	WebRTCClient client(logger, dc_logger, argb_cb);

	std::cout << "Connecting..." << std::endl;
	client.connect("http://localhost:5173", "C++Client", "JitterBridge");

	// client.set_message_callback([](
	// 								const std::string& username,
	// 								const std::string& message) {
	// 	//
	// 	std::cout << username << ": " << message << std::endl;
	// });

	const std::vector<std::pair<int, int>> dims = {
		{ 1920, 1080 },
		{ 320, 320 },
		{ 640, 480 },
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
			std::cout << "[Debug] Switch to " << width << "x" << height << std::endl;
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

	std::cout << "[Debug] Clean exit." << std::endl;
	return 0;
}