#pragma once
#include <mutex>
#include <condition_variable>
#include <functional>
#include "sio_client.h"
#include "rtc/rtc.hpp"
#include "rtc/track.hpp"
#include <rtc/h264rtpdepacketizer.hpp>

#include "encoder_libav.h"
#include "decoder_libav.h"
// #include "c74_min.h"

class EncoderLibav;

struct PeerConnectionStruct {
	std::string user_id;
	std::string username;
	std::shared_ptr<rtc::PeerConnection> rtc_connection;
	std::shared_ptr<rtc::DataChannel> data_channel;
	std::shared_ptr<rtc::Track> video_track;
};

class WebRTCClient {

public:
	using ARGBCallback = std::function<void(const uint8_t* argb, size_t size, int width, int height)>;

	WebRTCClient(
		std::function<void(const std::string&)> log_callback,
		std::function<void(const std::string&)> dc_callback,
		ARGBCallback argb_callback);

	~WebRTCClient();

	void connect(const std::string& url, const std::string& name, const std::string& room);
	void disconnect();
	void capture_matrix(uint8_t* argb_data, int width, int height, int planes);

private:
	// max log
	std::function<void(const std::string&)>
		log_callback;
	std::function<void(const std::string&)>
		dc_callback;
	ARGBCallback argb_callback;

	std::mutex m_mutex;
	sio::client sio_client;
	sio::socket::ptr current_socket;
	bool connect_finish = false;
	std::condition_variable_any _cond;

	void log(const std::string& message)
	{
		if (log_callback) {
			log_callback("[WebRTCClient]: " + message);
		}
	}

	void dc_send(const std::string& message)
	{
		if (dc_callback) {
			// TODO it should be in json
			dc_callback(message);
		}
	}
	void bind_sio_client_events();
	// rtc members
	std::map<std::string, std::shared_ptr<PeerConnectionStruct>>
		m_connections;
	std::shared_ptr<PeerConnectionStruct> createPeerConnection(
		const std::string& user_id,
		const std::string& username);
	void remove_connection(const std::string& user_id);
	void create_offer(const std::string& user_id);

	// encoder member
	std::unique_ptr<EncoderLibav> m_encoder;
	std::unique_ptr<DecoderLibav> m_decoder;
};