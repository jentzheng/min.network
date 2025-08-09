#pragma once
#include <mutex>

#include "rtc/rtc.hpp"
#include <rtc/websocket.hpp>
#include "rtc/track.hpp"
#include <rtc/h264rtpdepacketizer.hpp>
#include <nlohmann/json.hpp>
#include "videoencoder_libav.h"
#include "videodecoder_libav.h"

class WebRTCClient {

public:
	WebRTCClient(
		std::function<void(const std::string&)> log_callback,
		std::function<void(const std::string&)> dc_callback,
		std::function<void(
			const std::string& remote_username,
			const uint8_t* argb,
			size_t size,
			int width,
			int height)> video_data_callback);

	~WebRTCClient();

	void connect(const std::string& url, const std::string& name);
	void disconnect();
	void capture_matrix(uint8_t* argb_data, int width, int height, int planes);

private:
	std::mutex m_mutex;
	// max callbacks
	std::function<void(const std::string&)> log_callback;
	std::function<void(const std::string&)> dc_callback;
	std::function<void(
		const std::string& remote_username,
		const uint8_t* data,
		size_t size,
		int width,
		int height)>
		video_data_callback;

	void log(const std::string& message);
	void dc_send(const std::string& message);
	// static std::string generate_simple_id();

	// rtc members
	std::string localID;
	std::shared_ptr<rtc::WebSocket> ws;
	//	std::promise<void> wsPromise;

	std::shared_ptr<rtc::PeerConnection>
	createPeerConnection(
		std::weak_ptr<rtc::WebSocket> wws,
		const std::string& user_id,
		const std::string& username);

	void removePeerConnection(
		const std::string& user_id);

	struct ConnectionInfo {
		std::shared_ptr<rtc::PeerConnection> pc;
		std::shared_ptr<rtc::Track> video_track;
		std::shared_ptr<rtc::DataChannel> data_channel;
		std::unique_ptr<VideoDecoderLibav> decoder;
	};

	std::unordered_map<std::string, ConnectionInfo> peerConnectionMap;
	std::unique_ptr<VideoEncoderLibav> m_encoder;
};
