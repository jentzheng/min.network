#include "videodecoder_libav.h"

VideoDecoderLibav::VideoDecoderLibav()
{
    int ret;
    // av_log_set_level(AV_LOG_DEBUG);
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);

    if (!codec) {
        std::cerr << "[ERROR] AV_CODEC_ID_H264 decoder not found!" << std::endl;
    }

    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        std::cerr << "[ERROR] cannot allocate context" << std::endl;
    }

    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
        NULL, NULL, 0);

    if (ret < 0) {
        std::cerr << "[ERROR] av_hwdevice_ctx_create err" << std::endl;
    }

    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    ctx->thread_count = 1;

    ret = avcodec_open2(ctx, codec, nullptr);
    if (ret < 0) {
        std::cerr << "[ERROR] avcodec_open2 err" << std::endl;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    sw_frame = av_frame_alloc();
};

VideoDecoderLibav::~VideoDecoderLibav()
{
    avcodec_flush_buffers(ctx);
    sws_freeContext(sws_ctx);
    av_frame_free(&sw_frame);
    av_frame_free(&frame);
    av_buffer_unref(&hw_device_ctx);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
};

// from webrtc remote video
bool VideoDecoderLibav::decodeFrame(std::vector<std::byte> binary, uint32_t timestamp)
{

    int ret;
    pkt->data = reinterpret_cast<uint8_t*>(binary.data());
    pkt->size = static_cast<int>(binary.size());
    pkt->pts = static_cast<int64_t>(timestamp);

    ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0) {
        std::cerr << "avcodec_send_packet failed: " << ret << std::endl;
        return false;
    }

    ret = avcodec_receive_frame(ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_packet_unref(pkt);
        return false;
    } else if (ret < 0) {
        std::cerr << "avcodec_receive_frame failed: " << ret << std::endl;
        return false;
    }

    av_packet_unref(pkt);

    if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
        //  transfer gpu frame to sw_frame
        ret = av_hwframe_transfer_data(sw_frame, frame, 0);
        if (ret < 0) {
            std::cerr << "av_hwframe_transfer_data failed: " << ret << std::endl;
            return false;
        }
    } else {
        std::cerr << "frame->format not support" << std::endl;
        // av_frame_ref(sw_frame, frame);
        return false;
    }

    av_frame_unref(frame);

    width = sw_frame->width;
    height = sw_frame->height;

    sws_ctx = sws_getCachedContext(
        sws_ctx,
        sw_frame->width, sw_frame->height, (AVPixelFormat)sw_frame->format,
        sw_frame->width, sw_frame->height, AV_PIX_FMT_ARGB,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws_ctx) {
        std::cerr << "Failed to get SwsContext" << std::endl;
        return false;
    }

    decoded_buffer.resize(4 * sw_frame->width * sw_frame->height);
    uint8_t* dst_data[4] = { decoded_buffer.data(), nullptr, nullptr, nullptr };
    int dst_linesize[4] = { 4 * sw_frame->width, 0, 0, 0 };

    ret = sws_scale(
        sws_ctx,
        sw_frame->data,
        sw_frame->linesize,
        0,
        sw_frame->height,
        dst_data,
        dst_linesize);

    if (ret < 0) {
        std::cerr << "Failed to sws_scale" << std::endl;
        return false;
    }
    av_frame_unref(sw_frame);
    return true;
};
