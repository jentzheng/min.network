#include "webrtc_client.h"

using namespace sio;
using namespace std;

WebRTCClient::WebRTCClient(
	std::function<void(const std::string&)> log_callback,
	std::function<void(const std::string&)> dc_callback,
	ARGBCallback argb_callback)
	: log_callback(log_callback)
	, dc_callback(dc_callback)
	, argb_callback(argb_callback)
{

	// create listener only when the this obj inited
	sio_client.set_open_listener([this]() {
		log("SocketIO connected");
		current_socket = sio_client.socket();
		bind_sio_client_events();
	});

	sio_client.set_close_listener([this](sio::client::close_reason const& reason) {
		log("SocketIO closed");
	});

	sio_client.set_reconnecting_listener([this]() {
		log("Socket.IO reconnecting...");
	});

	sio_client.set_fail_listener([this]() {
		log("Socket.IO connection failed!");
	});
}

WebRTCClient::~WebRTCClient()
{
	log("WebRTCClient dismount");

	sio_client.clear_socket_listeners();
	sio_client.clear_con_listeners();
	sio_client.close();
}

// public method can be trigger by Max
void WebRTCClient::connect(const std::string& url, const std::string& name, const std::string& room)
{
	log("max trigger connect");

	std::map<std::string, std::string> query;
	query["username"] = name;
	query["roomId"] = room;
	query["role"] = "host";

	sio_client.connect(url, query);
}

void WebRTCClient::disconnect()
{
	log("max trigger disconnect");

	if (current_socket) {
		current_socket->close();
		current_socket->off_all();
	}

	for (auto& [user_id, connection] : m_connections) {
		if (connection->rtc_connection) {

			log("current pc closing " + user_id);
			connection->rtc_connection->clearStats();
			connection->rtc_connection->close();
		}

		if (connection->data_channel) {
			log("current dc closing " + user_id);
			connection->data_channel->close();
		}

		if (connection->video_track) {
			log("current videotrack closing " + user_id);
			connection->video_track->close();
		}
	}

	sio_client.close();
	m_connections.clear();
}

void WebRTCClient::bind_sio_client_events()
{

	current_socket->on("newUser",
		sio::socket::event_listener_aux(
			[this](string const& name, message::ptr const& data, bool isAck, message::list& ack_resp) {
				std::lock_guard<std::mutex> lock(m_mutex);
				string user_id = data->get_map()["from"]->get_string();
				string username = data->get_map()["username"]->get_string();
				log("New User: " + user_id + " " + username);
				// create peer connection for this user
				auto connection = createPeerConnection(user_id, username);
				if (connection) {
					m_connections[user_id] = connection;
					auto desc = connection->rtc_connection->createOffer();
				}
			}));

	current_socket->on("requestOffer",
		sio::socket::event_listener_aux(
			[this](string const& name, message::ptr const& data, bool isAck, message::list& ack_resp) {
				std::lock_guard<std::mutex> lock(m_mutex);

				string user_id = data->get_map()["from"]->get_string();
				string username = data->get_map()["username"]->get_string();
				auto connection = createPeerConnection(user_id, username);
				if (connection) {
					m_connections[user_id] = connection;
					create_offer(user_id);
				}
			}));

	current_socket->on("signal",
		sio::socket::event_listener_aux(
			[this](string const& name, message::ptr const& data, bool isAck, message::list& ack_resp) {
				std::lock_guard<std::mutex> lock(m_mutex);
				if (data->get_flag() == sio::message::flag_object) {
					string user_id = data->get_map()["from"]->get_string();

					auto it = m_connections.find(user_id);
					if (it == m_connections.end()) {
						return;
					}
					auto connection = it->second;
					if (!connection->rtc_connection) {
						return;
					}
					// handle description
					auto description = data->get_map()["description"];
					if (description && description->get_flag() == sio::message::flag_object) {
						string sdp = description->get_map()["sdp"]->get_string();
						string type = description->get_map()["type"]->get_string();

						rtc::Description desc(sdp, type);
						connection->rtc_connection->setRemoteDescription(desc);
					}
				}
			}));

	current_socket->on("icecandidate",
		sio::socket::event_listener_aux(
			[this](string const& name, message::ptr const& data, bool isAck, message::list& ack_resp) {
				std::lock_guard<std::mutex> lock(m_mutex);

				if (data->get_flag() == sio::message::flag_object) {
					string user_id = data->get_map()["from"]->get_string();
					auto it = m_connections.find(user_id);
					if (it == m_connections.end()) {
						return;
					}
					auto connection = it->second;
					if (!connection->rtc_connection) {
						return;
					}

					auto candidate_obj = data->get_map()["candidate"];
					if (candidate_obj && candidate_obj->get_flag() == sio::message::flag_object) {

						auto sdp_msg = candidate_obj->get_map()["candidate"];
						auto mid_msg = candidate_obj->get_map()["sdpMid"];
						if (sdp_msg && mid_msg) {
							std::string sdp = sdp_msg->get_string();
							std::string mid = mid_msg->get_string();
							rtc::Candidate candidate(sdp, mid);
							connection->rtc_connection->addRemoteCandidate(candidate);
						}
					}
				}
			}));

	current_socket->on(
		"userLeft", sio::socket::event_listener_aux([&](string const& name, message::ptr const& data, bool isAck, message::list& ack_resp) {
			std::lock_guard<std::mutex> lock(m_mutex);

			string from = data->get_map()["from"]->get_string();
			log("Connections before erase: " + std::to_string(m_connections.size()));
			auto it = m_connections.find(from);
			if (it != m_connections.end()) {
				if (it->second->rtc_connection) {
					it->second->rtc_connection->clearStats();
					it->second->rtc_connection->close();
				}
				if (it->second->data_channel) {
					it->second->data_channel->close();
				}
				m_connections.erase(it);
			}
			log("Connections after erase: " + std::to_string(m_connections.size()));
		}));
}

// handle RTC
std::shared_ptr<PeerConnectionStruct>
WebRTCClient::createPeerConnection(
	const std::string& user_id,
	const std::string& username)
{
	auto it = m_connections.find(user_id);
	if (it != m_connections.end()) {
		return it->second;
	}

	auto peer_conn = std::make_shared<PeerConnectionStruct>();
	peer_conn->user_id = user_id;
	peer_conn->username = username;
	rtc::Configuration config;
	peer_conn->rtc_connection = std::make_shared<rtc::PeerConnection>(config);

	// create peer connection callback at first
	peer_conn->rtc_connection->onLocalDescription([this, user_id](rtc::Description description) {
		std::lock_guard<std::mutex> lock(m_mutex);

		std::string sdp = std::string(description);
		std::string type = description.typeString();
		auto obj = sio::object_message::create();
		auto description_obj = sio::object_message::create();
		description_obj->get_map()["sdp"] = sio::string_message::create(sdp);
		description_obj->get_map()["type"] = sio::string_message::create(type);
		obj->get_map()["to"] = sio::string_message::create(user_id);
		obj->get_map()["description"] = description_obj;
		current_socket->emit("signal", obj);
	});

	peer_conn->rtc_connection->onSignalingStateChange([this, user_id, username](rtc::PeerConnection::SignalingState state) {
		std::string state_str;
		switch (state) {
		case rtc::PeerConnection::SignalingState::Stable:
			state_str = "Stable";
			break;
		case rtc::PeerConnection::SignalingState::HaveLocalOffer:
			state_str = "HaveLocalOffer";
			break;
		case rtc::PeerConnection::SignalingState::HaveRemoteOffer:
			state_str = "HaveRemoteOffer";
			break;
		case rtc::PeerConnection::SignalingState::HaveLocalPranswer:
			state_str = "HaveLocalPranswer";
			break;
		case rtc::PeerConnection::SignalingState::HaveRemotePranswer:
			state_str = "HaveRemotePranswer";
			break;
		default:
			state_str = "Unknown";
			break;
		}
		log("Signaling stage change: " + state_str);
	});

	peer_conn->rtc_connection->onLocalCandidate([this, user_id](rtc::Candidate candidate) {
		std::lock_guard<std::mutex> lock(m_mutex);

		auto obj = sio::object_message::create();
		auto obj_candidate = sio::object_message::create();
		obj->get_map()["to"] = sio::string_message::create(user_id);
		obj->get_map()["type"] = sio::string_message::create("icecandidate");
		obj_candidate->get_map()["candidate"] = sio::string_message::create(std::string(candidate));
		obj_candidate->get_map()["sdpMid"] = sio::string_message::create(std::string(candidate.mid()));
		obj->get_map()["candidate"] = obj_candidate;
		current_socket->emit("icecandidate", obj);
	});

	// create data channel
	peer_conn->data_channel = peer_conn->rtc_connection->createDataChannel("maxdatachannel");
	peer_conn->data_channel->onOpen([this, username]() {
		log("DataChannel opened with " + username);
	});

	peer_conn->data_channel->onMessage([this, username](rtc::message_variant data) {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (std::holds_alternative<std::string>(data)) {
			string message = std::get<std::string>(data);
			if (dc_callback) {
				dc_callback(message);
			}
		}
	});

	// add video track
	rtc::Description::Video media("video", rtc::Description::Direction::SendRecv);
	static uint32_t connection_counter = 0;
	const uint32_t ssrc = 1000 + (++connection_counter);

	const uint8_t payloadType = 96;
	const std::string msid = "1" + username;
	const std::string cname = "jittervideo" + username;

	media.addH264Codec(payloadType);
	media.addSSRC(ssrc, cname, msid, cname);
	peer_conn->video_track = peer_conn->rtc_connection->addTrack(media);

	// create RTP configuration
	auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
		ssrc, cname, payloadType, rtc::H264RtpPacketizer::ClockRate);
	// // // create packetizer
	auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, rtpConfig);
	auto srReporter = std::make_shared<rtc::RtcpSrReporter>(rtpConfig);
	auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
	packetizer->addToChain(srReporter);
	packetizer->addToChain(nackResponder);

	auto depacketizer = std::make_shared<rtc::H264RtpDepacketizer>(rtc::NalUnit::Separator::StartSequence);
	// auto recvSession = std::make_shared<rtc::RtcpReceivingSession>();
	packetizer->addToChain(depacketizer);

	peer_conn->video_track->setMediaHandler(packetizer);

	peer_conn->video_track->onOpen([this, peer_conn]() {
		log("Video track opened.");
		peer_conn->video_track->requestKeyframe(); // So the receiver can start playing immediately
	});

	peer_conn->video_track->onFrame([this](rtc::binary data, rtc::FrameInfo info) {
		auto timestamp = info.timestamp;
		if (!m_decoder) {
			m_decoder = std::make_unique<DecoderLibav>();
		}

		auto [argb, size, width, height] = m_decoder->decodeFrame(data, data.size());
		if (argb_callback) {
			argb_callback(argb, size, width, height);
		}
	});

	return peer_conn;
};

void WebRTCClient::create_offer(const std::string& user_id)
{
	auto it = m_connections.find(user_id);
	if (it == m_connections.end()) {
		return;
	}
	auto connection = it->second;
	if (!connection->rtc_connection) {
		return;
	}

	connection->rtc_connection->createOffer();
}

void WebRTCClient::capture_matrix(uint8_t* argb_data, int width, int height, int planes)
{
	if (!sio_client.opened()) {
		// Dont encode frame if socket server is not connected.
		return;
	}

	if (!m_encoder) {
		m_encoder = std::make_unique<EncoderLibav>(width, height, planes, 25);
	} else if (m_encoder->getWidth() != width || m_encoder->getHeight() != height) {
		log("Dimention switched, calling encoder to reinit.");
		m_encoder->reinit(width, height, planes, 25);
	}
	// pass the buffer only
	auto [data, size, info] = m_encoder->encodeFrame(argb_data);

	for (auto& [user_id, connection] : m_connections) {
		if (!connection->video_track || !connection->video_track->isOpen()) {
			continue;
		}

		if (data && size > 0) {
			try {
				connection->video_track->sendFrame(
					reinterpret_cast<const rtc::byte*>(data),
					size,
					info);
			} catch (const std::exception& e) {
				log(std::string("sendFrame exception: ") + e.what());
			}
		}
	}
}