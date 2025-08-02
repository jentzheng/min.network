#include "encoder_libav.h"

EncoderLibav::EncoderLibav(int width_, int height_, int planes_, int fps_)
{
	init(width_, height_, planes_, fps_);
}

EncoderLibav::~EncoderLibav()
{
	cleanup();
}

void EncoderLibav::init(int width_, int height_, int planes_, int fps_)
{
	width = width_;
	height = height_;
	planes = planes_;
	fps = fps_;

	// Mac Only
	const AVCodec* codec = avcodec_find_encoder_by_name("h264_videotoolbox");
	if (!codec) {
		std::cerr << "[ERROR] h264_videotoolbox encoder not found!" << std::endl;
		return;
	}

	// using CPU
	sws_ctx = sws_getContext(
		width, height, AV_PIX_FMT_ARGB,
		width, height, AV_PIX_FMT_YUV420P,
		SWS_BILINEAR, nullptr, nullptr, nullptr);

	sws_frame = av_frame_alloc();
	sws_frame->format = AV_PIX_FMT_ARGB;
	sws_frame->width = width;
	sws_frame->height = height;

	if (av_image_alloc(sws_frame->data, sws_frame->linesize, width, height, AV_PIX_FMT_ARGB, 32) < 0) {
		std::cerr << "[ERROR] av_image_alloc for sws_frame failed" << std::endl;
		av_frame_free(&sws_frame);
		sws_frame = nullptr;
		return;
	}

	// using GPU
	ctx = avcodec_alloc_context3(codec);
	ctx->width = width;
	ctx->height = height;
	ctx->time_base = AVRational { 1, fps };
	ctx->framerate = AVRational { fps, 1 };
	ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	ctx->color_range = AVCOL_RANGE_MPEG;

	ctx->thread_count = 1;
	ctx->gop_size = fps;
	ctx->max_b_frames = 0;
	ctx->bit_rate = 800000;
	ctx->rc_buffer_size = 0;

	av_opt_set(ctx->priv_data, "realtime", "1", 0);

	if (avcodec_open2(ctx, codec, nullptr) < 0) {
		std::cerr << "[ERROR] Could not open VideoToolbox encoder!" << std::endl;
		avcodec_free_context(&ctx);
		ctx = nullptr;
		return;
	}

	frame = av_frame_alloc();
	frame->format = ctx->pix_fmt;
	frame->width = width;
	frame->height = height;
	if (av_image_alloc(frame->data, frame->linesize, width, height, ctx->pix_fmt, 32) < 0) {
		std::cerr << "[ERROR] av_image_alloc for frame failed" << std::endl;
		av_frame_free(&frame);
		frame = nullptr;
		return;
	}

	pkt = av_packet_alloc();
}

void EncoderLibav::cleanup()
{
	if (sws_ctx) {
		sws_freeContext(sws_ctx);
		sws_ctx = nullptr;
	}

	if (ctx) {
		avcodec_free_context(&ctx);
		ctx = nullptr;
	}

	if (frame) {
		std::cout << "before freep av_freep(&frame->data[0])" << std::endl;
		av_freep(&frame->data[0]);
		std::cout << "before av_frame_free(&frame);" << std::endl;
		av_frame_free(&frame);
		std::cout << "before frame = nullptr" << std::endl;
		frame = nullptr;
	}

	if (sws_frame) {
		std::cout << "before freep sws_frame_data" << std::endl;
		av_freep(&sws_frame->data[0]);
		std::cout << "before freep sws_frame" << std::endl;
		av_frame_free(&sws_frame);
		std::cout << "before sws_frame = nullptr" << std::endl;
		sws_frame = nullptr;
	}

	if (pkt) {
		av_packet_free(&pkt);
		pkt = nullptr;
	}
}

void EncoderLibav::reinit(int new_width, int new_height, int new_planes, int new_fps)
{
	std::cout << "cleaning up old mem" << std::endl;
	cleanup();
	std::cout << "reinit encoder: " << new_width << "x" << new_height << "planes" << new_planes << std::endl;
	init(new_width, new_height, new_planes, new_fps);
}

int EncoderLibav::getWidth() const { return width; }
int EncoderLibav::getHeight() const { return height; }

std::tuple<const uint8_t*, size_t, int64_t>
EncoderLibav::encodeFrame(uint8_t* argb_data)
{
	static int64_t pts_counter = 0;
	frame->pts = pts_counter++;

	memcpy(sws_frame->data[0], argb_data, width * height * planes);
	sws_frame->linesize[0] = width * planes;

	sws_scale(
		sws_ctx,
		sws_frame->data,
		sws_frame->linesize,
		0,
		height,
		frame->data,
		frame->linesize);

	// encode
	int ret = avcodec_send_frame(ctx, frame);

	if (ret < 0) {
		std::cerr << "[ERROR] avcodec_send_frame failed: " << ret << std::endl;
		return { nullptr, 0, 0 };
	}

	ret = avcodec_receive_packet(ctx, pkt);
	if (ret == 0) {
		encoded_data.assign(pkt->data, pkt->data + pkt->size);
		av_packet_unref(pkt);

		return {
			encoded_data.data(),
			encoded_data.size(),
			frame->pts
		};

	} else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return { nullptr, 0, 0 };
	} else {
		std::cerr << "[ERROR] avcodec_receive_packet failed: " << ret << std::endl;

		return { nullptr, 0, 0 };
	}
}
