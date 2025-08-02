#pragma once
#include <vector>
#include <cstdint>
#include <iostream>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

class EncoderLibav {
public:
	EncoderLibav(int width, int height, int planes, int fps);
	~EncoderLibav();

	int getWidth() const;
	int getHeight() const;

	std::tuple<const uint8_t*, size_t, int64_t> // data, size, pts
	encodeFrame(uint8_t* argb_data);

	void reinit(int width, int height, int planes, int fps);

private:
	void init(int width, int height, int planes, int fps);
	void cleanup();

	int width, height, planes, fps;

	AVCodecContext* ctx = nullptr;
	AVFrame* frame = nullptr;
	AVFrame* sws_frame = nullptr;
	AVPacket* pkt = nullptr;
	SwsContext* sws_ctx = nullptr;
	std::vector<uint8_t> encoded_data;
};