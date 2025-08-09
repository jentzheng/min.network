#pragma once
#include <iostream>
#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h> //for av_image_alloc only
#include <libavutil/opt.h>

#include <libswscale/swscale.h>
}
struct EncodedFrame {
	const uint8_t* data;
	size_t size;
	int64_t pts;
};

class VideoEncoderLibav {
public:
	VideoEncoderLibav(int width, int height);
	~VideoEncoderLibav();

	int getWidth() const;
	int getHeight() const;

	void encodeFrame(uint8_t* data);

	EncodedFrame getEncodedData() const
	{
		return { encoded_data.data(), encoded_data.size(), frame->pts };
	}

	// if the incoming dim changed then it has to reinit the context
	void reinit(int width, int height);

private:
	int width, height;

	const AVCodec* codec = nullptr;
	AVCodecContext* ctx = nullptr;
	SwsContext* sws_ctx = nullptr;
	AVFrame* frame = nullptr;
	AVFrame* sws_frame = nullptr;
	AVPacket* pkt = nullptr;
	std::vector<uint8_t> encoded_data;

	void init(int width, int height);
	void cleanup();
};
