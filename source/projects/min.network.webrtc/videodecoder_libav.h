#pragma once

#include <vector>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

struct DecodedData {
	const uint8_t* data;
	size_t size;
	int width;
	int height;
};

class VideoDecoderLibav {
public:
	VideoDecoderLibav();
	~VideoDecoderLibav();

	void decodeFrame(std::vector<std::byte> binary, size_t binary_size);

	DecodedData getDecodedData() const
	{
		return { decoded_buffer.data(), decoded_buffer.size(), width, height };
	};

private:
	int width, height;
	AVCodecContext* ctx;
	AVBufferRef* hw_device_ctx;
	AVPacket* pkt;
	AVFrame* frame;
	AVFrame* sw_frame;
	SwsContext* sws_ctx;

	std::vector<uint8_t> decoded_buffer;
};
