#include "videoencoder_libav.h"

// Example
// https://github.com/libav/libav/blob/master/doc/examples/encode_video.c
// https://gist.github.com/sdumetz/961585ea70f82e4fb27aadf66b2c9cb2

VideoEncoderLibav::VideoEncoderLibav(int width_, int height_)
{
    // Mac Only
    codec = avcodec_find_encoder_by_name("h264_videotoolbox");
    if (!codec) {
        std::cerr << "[ERROR] h264_videotoolbox encoder not found!" << std::endl;
    }
    // av_log_set_level(AV_LOG_DEBUG);
    init(width_, height_);
}

VideoEncoderLibav::~VideoEncoderLibav()
{
    cleanup();
}

void VideoEncoderLibav::init(int width_, int height_)
{
    width = width_;
    height = height_;

    int fps = 25;

    // using CPU
    sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_ARGB,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    sws_frame = av_frame_alloc();
    sws_frame->format = AV_PIX_FMT_ARGB;
    sws_frame->width = width;
    sws_frame->height = height;

    av_frame_get_buffer(sws_frame, 0);

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

    avcodec_open2(ctx, codec, nullptr);

    frame = av_frame_alloc();
    frame->format = ctx->pix_fmt;
    frame->width = width;
    frame->height = height;

    av_frame_get_buffer(frame, 0);

    pkt = av_packet_alloc();
}

void VideoEncoderLibav::cleanup()
{
    if (pkt)
        av_packet_free(&pkt);
    if (frame)
        av_frame_free(&frame);
    if (sws_frame)
        av_frame_free(&sws_frame);
    if (sws_ctx)
        sws_freeContext(sws_ctx);
    if (ctx)
        avcodec_free_context(&ctx);
    pkt = nullptr;
    frame = nullptr;
    sws_frame = nullptr;
    sws_ctx = nullptr;
    ctx = nullptr;
}

void VideoEncoderLibav::reinit(int new_width, int new_height)
{
    cleanup();
    init(new_width, new_height);
}

int VideoEncoderLibav::getWidth() const { return width; }
int VideoEncoderLibav::getHeight() const { return height; }
void VideoEncoderLibav::encodeFrame(uint8_t* data)
{
    int ret;

    static int64_t pts_counter = 0;
    frame->pts = pts_counter++;

    ret = av_image_fill_arrays(
        sws_frame->data,
        sws_frame->linesize,
        data,
        AV_PIX_FMT_ARGB,
        width,
        height,
        1);

    if (ret < 0) {
        std::cerr << "av_image_fill_arrays failed: " << ret << std::endl;
        return;
    }

    // argb->yuv420
    ret = sws_scale(
        sws_ctx,
        sws_frame->data,
        sws_frame->linesize,
        0,
        height,
        frame->data,
        frame->linesize);

    if (ret < 0) {
        std::cerr << "sws_scale failed: " << ret << std::endl;
        return;
    }

    // std::cout << "[Encoder] frame format=" << frame->format
    // 		  << ", width=" << frame->width
    // 		  << ", height=" << frame->height << std::endl;
    // encode

    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        std::cerr << "avcodec_send_frame failed: " << ret << std::endl;
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        }
        if (ret < 0) {
            std::cerr << "avcodec_receive_packet failed: " << ret << std::endl;
            return;
        }

        encoded_data.assign(pkt->data, pkt->data + pkt->size);

        av_packet_unref(pkt);
    }
}
