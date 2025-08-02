#include "c74_min.h"
#include "webrtc_client.h"

using namespace c74;
using namespace c74::min;

using host = symbol;
using name = symbol;
using room = symbol;
using debug = number;

class webrtc : public c74::min::object<webrtc> {
public:
	MIN_DESCRIPTION { "Send and Recive video(it will support audio in the future) from browser through WebRTC." };
	MIN_AUTHOR { "Cycling '74" };
	MIN_TAGS { "video" };
	MIN_RELATED { "jitter" };

	inlet<> input_matrix { this, "(matrix) Input", "matrix" };
	outlet<> output_matrix { this, "(matrix) Output", "matrix" };
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
	argument<symbol> room_arg
	{
		this, "room", "room", MIN_ARGUMENT_FUNCTION
		{
			m_room = arg;
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
		if (args.size() > 2) {
			m_room = args[2];
		}

		out_matrix_obj = max::jit_object_new(max::_jit_sym_jit_matrix);

		m_client
			= std::make_unique<WebRTCClient>(
				// callback log from WebRTCClient
				[this](const std::string& rtc_log) {
					cout << rtc_log << c74::min::endl;
				},
				// callback message(TODO it will be in json type) from WebRTCClient
				[this](const std::string& dc_message) {
					// can not use output_dict.send() here as Max will crash.
					// cout << dc_message << c74::min::endl;
					// cout << "dc_message" << dc_message << c74::min::endl;
					m_pending_message = dc_message;
					deferrer_dict_out.set();
				},
				// callack argb from WebRTCClient's h264 decoder
				[this](const uint8_t* argb, size_t size, int width, int height) {
					// how to assign argb to a matrix and use output_matrix.send(name) to max
				});
	};

	~webrtc()
	{
		// max::jit_object_free(out_matrix_obj);
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
			m_dict["message"] = m_pending_message;
			output_dict.send("dictionary", dict_name);
			return {};
		}
	};

	queue<> deferrer_matrix_out
	{
		this,
			MIN_FUNCTION
		{
			// if (out_matrix_obj) {
			// 	max::t_jit_matrix_info info;
			// 	max::jit_matrix_info_default(&info);
			// 	info.type = max::_jit_sym_char;
			// 	info.dimcount = 2;
			// 	info.dim[0] = m_pending_width;
			// 	info.dim[1] = m_pending_height;
			// 	info.planecount = 4;
			// 	max::jit_object_method(out_matrix_obj, max::_jit_sym_setinfo_ex, &info);
			// 	long matrix_bytes = m_pending_width * m_pending_height * 4;
			// 	char* argb_data = (char*)max::sysmem_newptr(matrix_bytes);
			// 	max::object_method(out_matrix_obj, max::_jit_sym_data, argb_data);
			// 	cout << "matrix size: " << info.size << endl;
			// 	output_matrix.send("jit_matrix", out_matrix_name);
			// }
			return {};
		}
	};

	message<threadsafe::yes> connect
	{
		this, "connect", "connect to socketIO", MIN_FUNCTION
		{
			if (m_client) {
				m_client->connect(m_host.c_str(), m_name.c_str(), m_room.c_str());
			}
			return {};
		}
	};

	message<threadsafe::yes> disconnect
	{
		this, "disconnect", "disconnect the server", MIN_FUNCTION
		{
			if (m_client) {
				m_client->disconnect();
			}
			return {};
		}
	};

	message<threadsafe::yes> jit_matrix
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
				int width = matrix_info.dim[0];
				int height = matrix_info.dim[1];
				int planes = matrix_info.planecount;
				int size = matrix_info.size;

				if (matrix_data && size > 0) {
					m_client->capture_matrix(argb_data, width, height, planes);
				}
			}
			return {};
		}
	};

private:
	// Max members
	c74::min::mutex m_mutex;
	host m_host { "http://localhost:5173" };
	name m_name { "Max#0" };
	room m_room { "JitterBridge" };

	std::string m_pending_message;
	std::vector<uint8_t> m_pending_argb;
	int m_pending_width = 0;
	int m_pending_height = 0;

	// void* out_matrix_data = nullptr;
	void* out_matrix_obj;
	// symbol out_matrix_name;

	// WebRTCClient members
	std::unique_ptr<WebRTCClient> m_client;

	// message<> jitclass_setup
	// {
	// 	this, "jitclass_setup",
	// 		MIN_FUNCTION
	// 	{
	// 		cout << "jitclass_setup" << endl;
	// 		return {};
	// 	}
	// };

	// message<> maxclass_setup
	// {
	// 	this, "maxclass_setup",
	// 		MIN_FUNCTION
	// 	{
	// 		cout << "maxclass_setup" << endl;
	// 		return {};
	// 	}
	// };

	message<> setup
	{
		this, "setup", MIN_FUNCTION
		{
			cout << "setup" << endl;

			// void* mop = max::jit_object_new(max::_jit_sym_jit_mop, 1, 1); // 1 inlet, 1 outlet
			// max::jit_object_method(mop, max::_jit_sym_outputmatrix, this->output_matrix); // 绑定 outlet
			// max::jit_object_method(mop, max::_jit_sym_register, max::gensym("your_jit_class"));

			return {};
		}
	};

	message<> maxob_setup
	{
		this, "maxob_setup", MIN_FUNCTION
		{
			cout << "maxob_setup" << endl;

			// t_object* mob = maxob_from_jitob(maxobj());
			// long dim = object_attr_getlong(mob, max::_jit_sym_dim);

			// if (join == symbol())
			// 	object_attr_setlong(mob, max::_jit_sym_dim, args.size() > 0 ? (long)args[0] : dim);
			// object_attr_setlong(mob, max::_jit_sym_planecount, 1);
			// object_attr_setsym(mob, max::_jit_sym_type, max::_jit_sym_float32);

			// dumpoutlet = max_jit_obex_dumpout_get(mob);

			return {};
		}
	};
};

MIN_EXTERNAL(webrtc);