#include "webrtc_client.h"

using nlohmann::json;
using namespace std;
using namespace rtc;
using namespace std::chrono_literals;

static std::string generate_random_id()
{
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.reserve(8);
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    for (int i = 0; i < 8; ++i) {
        result += charset[std::rand() % 26];
    }
    return result;
}

std::string urlEncode(const std::string& str)
{
    std::ostringstream encodedStream;
    encodedStream << std::hex << std::uppercase << std::setfill('0');

    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encodedStream << c;
        } else {
            encodedStream << '%' << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(c));
        }
    }

    return encodedStream.str();
}

template <class T>
weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) { return ptr; }

WebRTCClient::WebRTCClient(
    function<void(const string&)> log_callback,
    function<void(const string&)> dc_callback,
    function<void(
        const std::string& remote_username,
        const uint8_t* argb,
        size_t size,
        int width,
        int height)> video_data_callback)

    : log_callback(log_callback)
    , dc_callback(dc_callback)
    , video_data_callback(video_data_callback)
{

    rtcInitLogger(RTC_LOG_INFO, nullptr);

    // init ws
    localID = generate_random_id();
    rtc::WebSocket::Configuration config;
    config.disableTlsVerification = true;
    ws = std::make_shared<rtc::WebSocket>(config);
}

WebRTCClient::~WebRTCClient()
{
    disconnect();
    m_encoder.reset();
}

void WebRTCClient::log(const string& message)
{
    log_callback("[WebRTCClient]: " + message);
}

void WebRTCClient::dc_send(const string& message)
{
    dc_callback(message);
}

// public method can be trigger by Max
void WebRTCClient::connect(
    const string& url,
    const string& name)
{
    log("WebRTCClient::connect()...");
    //	std::promise<void> wsPromise;
    //	auto wsFuture = wsPromise.get_future();

    string params = "?id=" + urlEncode(localID) + "&username=" + urlEncode(name) + "&role=Jitter";
    string full_url = url + params;
    log("WebSocket URL is " + full_url);

    ws->onOpen([this]() {
        log("WebSocket opened");
        // wsPromise.set_value();
    });

    ws->onError([this](std::string s) {
        log("Websocket onError: " + s);
    });

    ws->onClosed([this]() {
        log("WebSocket closed");
    });

    ws->onMessage([this, wws = make_weak_ptr(ws)](auto data) {
        if (!std::holds_alternative<std::string>(data))
            return;
        json message = json::parse(std::get<std::string>(data));
        string signalingType = message.at("signalingType");
        string sender = message.value("sender", "");
        string senderName = message.value("senderName", "");
        string target = message.value("target", "");

        if (!message.contains("content"))
            return;

        const json& content = message["content"];

        // if (signalingType == "Clients") {
        // 	if (content.is_array()) {
        // 	}
        // }

        if (signalingType == "ClientEnter") {
            string user_id = content.at("id");
            json properties = content.at("properties");
            string username = properties.at("username");
            string role = properties.at("role");
            // if (role != "Jitter") {
            // 	lock_guard<mutex> lock(m_mutex);
            // 	log("Creating peer for " + user_id + "/" + username);
            // 	createPeerConnection(wws, user_id);
            // }
        } else if (signalingType == "ClientExit") {
            string id = content.at("id");
            log("Clientexit " + id);
            for (const auto& [peer_id, conn] : peerConnectionMap) {
                log("Current peer: " + peer_id);
            }
            // removePeerConnection(id);
            log("current peer size: " + to_string(peerConnectionMap.size()));
        } else if (signalingType == "Offer") {
            // lock_guard<mutex> lock(m_mutex);
            string sdp = content.at("sdp");
            string type = content.at("type");

            if (auto jt = peerConnectionMap.find(sender); jt != peerConnectionMap.end()) {
                std::cout << "Setting remote description: " << type << " from user " << sender << std::endl;
                jt->second.pc->setRemoteDescription(rtc::Description(sdp, type));
            } else {

                std::cout << "Peer not exist, create and Answering to " + sender << std::endl;
                shared_ptr<rtc::PeerConnection> pc;
                pc = createPeerConnection(wws, sender, senderName);
                pc->setRemoteDescription(rtc::Description(sdp, type));
            }
        } else if (signalingType == "Answer") {
            // lock_guard<mutex> lock(m_mutex);

            if (auto jt = peerConnectionMap.find(sender); jt != peerConnectionMap.end()) {
                string sdp = content.at("sdp");
                string type = content.at("type");
                std::cout << "Setting remote description: " << type << " from user " << sender << std::endl;
                jt->second.pc->setRemoteDescription(rtc::Description(sdp, type));
            }

        } else if (signalingType == "Ice") {
            // lock_guard<mutex> lock(m_mutex);

            if (content.is_object() && content.contains("candidate") && content.contains("sdpMid")) {
                string candidate = content.at("candidate");
                string sdpMid = content.at("sdpMid");
                if (auto jt = peerConnectionMap.find(sender); jt != peerConnectionMap.end()) {
                    string candidate = content.at("candidate");
                    string sdpMid = content.at("sdpMid");
                    std::cout << "add remote candidate from user: " << sender << std::endl;
                    jt->second.pc->addRemoteCandidate(rtc::Candidate(candidate, sdpMid));
                }
            }
        }
    });

    try {
        ws->open(full_url);
    } catch (const std::exception& e) {
        log("WebSocket open error: " + string(e.what()));
    }
}

void WebRTCClient::disconnect()
{
    for (auto& [user_id, conn] : peerConnectionMap) {
        if (conn.decoder) {
            conn.decoder.reset();
        }

        if (conn.video_track) {
            log("current videotrack closing " + user_id);
            conn.video_track->close();
        }
        if (conn.pc) {
            log("current pc closing " + user_id);
            conn.pc->clearStats();
            conn.pc->resetCallbacks();
            conn.pc->close();
        }
    }
    peerConnectionMap.clear();
    ws->close();
    ws->resetCallbacks();
    log("WebRTCClient::disconnect()");
    return;
}

// handle RTC
rtc::shared_ptr<rtc::PeerConnection>
WebRTCClient::createPeerConnection(
    weak_ptr<rtc::WebSocket> wws,
    const string& remote_id,
    const string& remote_name)
{

    // rtc::Configuration config;
    auto pc = make_shared<rtc::PeerConnection>();

    // create peer connection callback
    pc->onLocalCandidate([wws, remote_id](rtc::Candidate rtcCandidate) {
        string candidate = rtcCandidate.candidate();
        string sdpMid = rtcCandidate.mid();
        json message = {
            { "target", remote_id },
            { "signalingType", "Ice" },
            { "content", { { "candidate", candidate }, { "sdpMid", sdpMid } } }
        };

        if (auto ws = wws.lock()) {
            ws->send(message.dump());
        }
    });

    pc->onLocalDescription([this, wws, remote_id, pc](rtc::Description description) {
        string sdp = string(description);
        std::string type = description.typeString();
        if (!type.empty()) {
            type[0] = std::toupper(type[0]);
        }

        json message = {
            // { "sender", socket.id },
            { "target", remote_id },
            { "signalingType", type },
            { "content", { { "type", description.typeString() }, { "sdp", sdp } } }
        };

        if (auto ws = wws.lock()) {
            ws->send(message.dump());
        }
    });

    pc->onStateChange([this, remote_id](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Closed) {
            log("remote id closing " + remote_id);
            removePeerConnection(remote_id);
        }
    });

    // std::shared_ptr<rtc::DataChannel> dc;
    pc->onDataChannel([this, remote_id](rtc::shared_ptr<rtc::DataChannel> dc) {
        auto it = peerConnectionMap.find(remote_id);
        if (it != peerConnectionMap.end()) {
            it->second.data_channel = dc;
        }

        dc->onOpen([this, remote_id, wdc = make_weak_ptr(dc)]() {
            if (auto dc = wdc.lock()) {
                log("DataChannel  from " + remote_id + " opened");
            }
        });

        dc->onClosed([this, remote_id]() {
            log("DataChannel from " + remote_id + " closed");
        });

        dc->onMessage([this](auto data) {
            if (std::holds_alternative<std::string>(data)) {
                dc_callback(std::get<std::string>(data));
            } else {
                std::cout << "Binary message: " << " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
            }
        });
    });

    rtc::Description::Video
        media("jitter-media", rtc::Description::Direction::SendRecv);

    // add video track
    const rtc::SSRC ssrc = 42;
    const int payloadType = 96;
    const string cname = "video-send";
    media.addH264Codec(payloadType);
    media.addSSRC(ssrc, cname);

    rtc::shared_ptr<rtc::Track> videoTrack = pc->addTrack(media);

    // create RTP configuration
    auto rtpConfig = make_shared<rtc::RtpPacketizationConfig>(
        ssrc, cname, payloadType, rtc::H264RtpPacketizer::ClockRate);
    // create packetizer
    auto packetizer = make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, rtpConfig);
    auto depacketizer = make_shared<rtc::H264RtpDepacketizer>(rtc::NalUnit::Separator::StartSequence);

    packetizer->addToChain(depacketizer);
    videoTrack->setMediaHandler(packetizer);

    videoTrack->onOpen([this]() {
        log("Video track opened.");
    });

    videoTrack->onFrame([this, remote_id, remote_name](rtc::binary data, rtc::FrameInfo info) {
        auto it = peerConnectionMap.find(remote_id);
        if (it == peerConnectionMap.end())
            return;

        // each incoming track should have its own decoder
        auto& decoder = it->second.decoder;
        if (!decoder) {
            decoder = std::make_unique<VideoDecoderLibav>();
        }

        if (decoder->decodeFrame(data, info.timestamp)) {
            auto decoded = decoder->getDecodedData();
            video_data_callback(
                remote_name,
                decoded.data,
                decoded.size,
                decoded.width,
                decoded.height);
        };
    });

    peerConnectionMap.emplace(remote_id,
        ConnectionInfo {
            pc,
            videoTrack,
            nullptr,
            std::make_unique<VideoDecoderLibav>() });

    return pc;
}

void WebRTCClient::capture_matrix(uint8_t* data, int width, int height, int planes)
{
    if (!ws->isOpen()) {
        return;
    }

    if (!m_encoder) {
        m_encoder = make_unique<VideoEncoderLibav>(width, height);
    } else if (m_encoder->getWidth() != width || m_encoder->getHeight() != height) {
        m_encoder->reinit(width, height);
    }

    m_encoder->encodeFrame(data);
    auto encoded = m_encoder->getEncodedData();

    for (auto& [user_id, conn] : peerConnectionMap) {
        if (!conn.video_track || !conn.video_track->isOpen())
            continue;

        if (encoded.size > 0) {
            conn.video_track->sendFrame(
                reinterpret_cast<const rtc::byte*>(encoded.data),
                static_cast<uint32_t>(encoded.size),
                static_cast<uint32_t>(encoded.pts & 0xFFFFFFFF));
        }
    }
}

void WebRTCClient::removePeerConnection(const std::string& remote_id)
{

    auto it = peerConnectionMap.find(remote_id);
    if (it == peerConnectionMap.end())
        return;

    if (it != peerConnectionMap.end()) {
        auto& conn = it->second;

        if (conn.pc) {
            conn.pc->close();
            conn.pc.reset();
        }

        if (conn.decoder) {
            conn.decoder.reset();
        }

        if (conn.video_track) {
            conn.video_track.reset();
        }

        if (conn.data_channel) {
            conn.data_channel.reset();
        }

        peerConnectionMap.erase(it);

        log("pc connection closed");
        log("Remove peer from map for id: " + remote_id);
    }
}
