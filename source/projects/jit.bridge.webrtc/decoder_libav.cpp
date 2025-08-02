#include "decoder_libav.h"

DecoderLibav::DecoderLibav()
{

	const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	codec_ctx = avcodec_alloc_context3(codec);
	// enable-hwaccel
	avcodec_open2(codec_ctx, codec, nullptr);
	frame = av_frame_alloc();
	frame_argb = av_frame_alloc();
	sws_ctx = nullptr;
};

DecoderLibav::~DecoderLibav()
{
	//
	if (codec_ctx)
		avcodec_free_context(&codec_ctx);
	if (frame)
		av_frame_free(&frame);
	if (frame_argb)
		av_frame_free(&frame_argb);
	if (sws_ctx)
		sws_freeContext(sws_ctx);
};

// int DecoderLibav::getWidth() const { return width; }
// int DecoderLibav::getHeight() const { return height; }

std::tuple<const uint8_t*, size_t, int, int>
DecoderLibav::decodeFrame(rtc::binary binary, size_t binary_size)
{
	AVPacket pkt = {};

	const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(binary.data());
	pkt.data = const_cast<uint8_t*>(data_ptr);
	pkt.size = static_cast<int>(binary_size);

	int ret = avcodec_send_packet(codec_ctx, &pkt);
	if (ret < 0) {
		std::cerr << "avcodec_send_packet failed: " << ret << std::endl;
	}

	ret = avcodec_receive_frame(codec_ctx, frame);
	if (ret < 0) {
		std::cerr << "avcodec_receive_frame failed: " << ret << std::endl;
	}

	int width = frame->width;
	int height = frame->height;
	sws_ctx = sws_getCachedContext(
		sws_ctx,
		width, height, (AVPixelFormat)frame->format,
		width, height, AV_PIX_FMT_BGRA,
		SWS_BILINEAR, nullptr, nullptr, nullptr);

	int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, width, height, 1);
	std::vector<uint8_t> argb_buf(num_bytes);
	uint8_t* dst_data[4] = { argb_buf.data(), nullptr, nullptr, nullptr };
	int dst_linesize[4] = { 4 * width, 0, 0, 0 };

	sws_scale(
		sws_ctx,
		frame->data, frame->linesize,
		0, height,
		dst_data, dst_linesize);

	if (!argb_buf.empty()) {
		return { argb_buf.data(), argb_buf.size(), width, height };
	}
	return { nullptr, 0, 0, 0 };
};
