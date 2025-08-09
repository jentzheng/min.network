#include "c74_min.h"
#include "webrtc_client.h"

using namespace c74;
using namespace c74::min;

class webrtc : public c74::min::object<webrtc> {
public:
    MIN_DESCRIPTION { "Send and Recive video(it will support audio in the future) from browser through WebRTC." };
    MIN_AUTHOR { "Cycling '74" };
    MIN_TAGS { "video" };
    MIN_RELATED { "jitter" };

    inlet<> input_matrix { this, "(matrix) Input", "matrix" };
    // inlet<> input_signal { this, "(signal) Input", "signal" }; // TODO

    // outlet<> output_signal { this, "(signal) Output", "signal" }; // TODO
    outlet<> output_dict { this, "(dict) Output", "dictionary" };
    argument<symbol> host_arg
    {
        this, "host", "Remote Socket.IO server", MIN_ARGUMENT_FUNCTION
        {
            m_host = arg;
        }
    };

    argument<symbol> name_arg
    {
        this, "name", "username", MIN_ARGUMENT_FUNCTION
        {
            m_name = arg;
        }
    };

    webrtc(const atoms& args = {})
    {
        // initialize the webrtc client with the host, name, and room
        if (args.size() > 0) {
            m_host = args[0];
        }
        if (args.size() > 1) {
            m_name = args[1];
        }

        m_client = std::make_unique<WebRTCClient>(
            // callback log from WebRTCClient
            [this](const std::string& webrtc_log) {
                cout << webrtc_log << c74::min::endl;
            },
            // callback datachannel message from WebRTCClient
            [this](const std::string& dc_message) {
                // TODO
            },
            // callack argb from WebRTCClient's h264 decoder
            [this](const std::string& remote_username, const uint8_t* argb, size_t size, int width, int height) {
                symbol matrix_name(remote_username);
                void* remote_matrix = max::jit_object_findregistered(matrix_name);

                if (remote_matrix) {
                    auto out_savelock = max::jit_object_method(remote_matrix, max::_jit_sym_lock, reinterpret_cast<void*>(1));
                    max::t_jit_matrix_info info;
                    max::jit_matrix_info_default(&info);
                    info.dimcount = 2;
                    info.dim[0] = width;
                    info.dim[1] = height;
                    info.planecount = 4;
                    info.type = max::_jit_sym_char;
                    max::jit_object_method(remote_matrix, max::_jit_sym_setinfo_ex, &info);
                    max::jit_object_method(remote_matrix, max::_jit_sym_data, argb);
                    //                    max::jit_object_method(remote_matrix, max::_jit_sym_matrix_calc); // doesn't work...
                    max::object_method(remote_matrix, max::gensym("bang")); // doesn't work...
                    max::jit_object_method(remote_matrix, max::_jit_sym_lock, out_savelock);
                }
            });
    };

    ~webrtc()
    {

        cout << "out_matrix_obj free" << endl;
    };

    // A min::queue creates an element that,
    // when set, will be executed by Max’s low-priority queue.
    queue<> deferrer_dict_out
    {
        this,
            MIN_FUNCTION
        {
            symbol dict_name(true);
            dict m_dict(dict_name);
            m_dict["username"] = "username";
            output_dict.send("dictionary", dict_name);
            return {};
        }
    };

    message<threadsafe::no> connect
    {
        this, "connect", "connect to socketIO", MIN_FUNCTION
        {
            if (m_client) {
                m_client->connect(m_host.c_str(), m_name.c_str());
            }
            return {};
        }
    };

    message<threadsafe::no> disconnect
    {
        this, "disconnect", "disconnect the server", MIN_FUNCTION
        {
            if (m_client) {
                m_client->disconnect();
            }
            return {};
        }
    };

    message<> bang
    {
        this, "bang", "Post the greeting.",
            MIN_FUNCTION
        {

            return {};
        }
    };

    message<threadsafe::no> jit_matrix
    {
        this, "jit_matrix", "Process Jitter matrix", MIN_FUNCTION
        {
            if (args.size() > 0) {

                symbol matrix_name = args[0];
                void* jit_matrix = max::jit_object_findregistered(matrix_name);
                if (!jit_matrix)
                    return {};

                max::t_jit_matrix_info matrix_info;
                void* matrix_data = nullptr;

                max::object_method(jit_matrix, max::_jit_sym_getinfo, &matrix_info);
                max::object_method(jit_matrix, max::_jit_sym_getdata, &matrix_data);

                uint8_t* argb_data = static_cast<uint8_t*>(matrix_data);
                long width = matrix_info.dim[0];
                long height = matrix_info.dim[1];
                long planes = matrix_info.planecount;
                long size = matrix_info.size;

                if (matrix_data && size > 0) {
                    m_client->capture_matrix(argb_data,
                        static_cast<int>(width),
                        static_cast<int>(height),
                        static_cast<int>(planes));
                }
            }
            return {};
        }
    };

private:
    // Max members
    c74::min::mutex m_mutex;
    symbol m_host { "http://localhost:5173" };
    symbol m_name { "Max#0" };
    number log_level = 0;

    // WebRTCClient members
    std::unique_ptr<WebRTCClient> m_client;
};

MIN_EXTERNAL(webrtc);
