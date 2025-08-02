#include <vector>
#include <cstdint>
#include <iostream>
#include <chrono>
#include "rtc/common.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

class DecoderLibav {
public:
	DecoderLibav();
	~DecoderLibav();

	std::tuple<const uint8_t*, size_t, int, int>
	decodeFrame(rtc::binary binary, size_t binary_size);

private:
	AVCodecContext* codec_ctx = nullptr;
	AVFrame* frame = nullptr;
	AVFrame* frame_argb = nullptr;
	SwsContext* sws_ctx = nullptr;
};