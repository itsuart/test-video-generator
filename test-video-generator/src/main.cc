#include <string>
#include <memory>
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <chrono>

#include "ffraii/AVFrameR.h"
#include "ffraii/AVFormatContextR.h"
#include "ffraii/AVCodecContextR.h"
#include "ffraii/AVDictionaryR.h"
#include "ffraii/SwsContextR.h"

namespace {
    std::string av_error_to_string(int error) {
        std::string errorMessageBuffer(4096, '\0');
        av_strerror(error, &errorMessageBuffer[0], errorMessageBuffer.length());

        return errorMessageBuffer;
    }

    ffraii::AVFrameR allocVideoFrame(AVPixelFormat pix_fmt, int width, int height) {
        ffraii::AVFrameR frame(av_frame_alloc());

        if (frame) {
            frame->format = pix_fmt;
            frame->width = width;
            frame->height = height;

            int ret = av_frame_get_buffer(frame, 32);
            if (ret < 0) {
                fprintf(stderr, "Could not allocate frame data:%s\n", av_error_to_string(ret).c_str());
            }
        }

        return frame;
    }
}

int wmain(int /*argc*/, const wchar_t** /*argv*/) {
    avcodec_register_all();
    av_register_all();

    const char* filename = "c:/tmp/test-video.mp4";

    ffraii::AVFormatContextR formatContext;
    {
        int ret = avformat_alloc_output_context2(formatContext.p_value(), nullptr, nullptr, filename);
        if (ret < 0) {
            std::printf("avformat_alloc_output_context2 failed:%s\n", av_error_to_string(ret).c_str());
            return 1;
        }
    }

    AVOutputFormat* fmt = formatContext->oformat;

    ffraii::AVDictionaryR allOptions;

    constexpr AVPixelFormat STREAM_PIX_FMT = AV_PIX_FMT_YUV420P;
    constexpr int VIDEO_WIDTH = 800;
    constexpr int VIDEO_HEGIHT = 600;
    constexpr unsigned FramesPerSecond = 25;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1.
     */
    const AVRational TimeBase { 1, FramesPerSecond };

    assert(fmt->video_codec != AV_CODEC_ID_NONE);
    AVCodec* videoCodec = avcodec_find_encoder(fmt->video_codec);

    if (!videoCodec) {
        fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(fmt->video_codec));
        return 1;
    }

    AVStream* pVideoStream = avformat_new_stream(formatContext, nullptr);
    if (not pVideoStream) {
        fprintf(stderr, "Could not allocate stream\n");
        return 1;
    }
    pVideoStream->id = formatContext->nb_streams - 1;
    pVideoStream->time_base = TimeBase;

    ffraii::AVCodecContextR videoCodecContext(avcodec_alloc_context3(videoCodec));
    if (!videoCodecContext) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        return 1;
    }

    videoCodecContext->codec_id = fmt->video_codec;
    videoCodecContext->bit_rate = 400000;
    videoCodecContext->width = VIDEO_WIDTH;
    videoCodecContext->height = VIDEO_HEGIHT;
    videoCodecContext->time_base = TimeBase;
    videoCodecContext->gop_size = 10; /* emit one intra frame every 10 frames at most */
    videoCodecContext->pix_fmt = STREAM_PIX_FMT;

    {
        ffraii::AVDictionaryR opt;
        av_dict_copy(opt.p_value(), allOptions, 0);

        /* open the codec */
        int ret = avcodec_open2(videoCodecContext, videoCodec, opt.p_value());

        if (ret < 0) {
            fprintf(stderr, "Could not open video codec: %s\n", av_error_to_string(ret).c_str());
            return 1;
        }
    }

    /* copy the stream parameters to the muxer */
    {
        int ret = avcodec_parameters_from_context(pVideoStream->codecpar, videoCodecContext);
        if (ret < 0) {
            fprintf(stderr, "Could not copy the stream parameters\n");
            return 1;
        }
    }

    av_dump_format(formatContext, 0, filename, 1);

    {
        int ret = avio_open(&formatContext->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", filename, av_error_to_string(ret).c_str());
            return 1;
        }
    }

    /* Write the stream header, if any. */
    {
        int ret = avformat_write_header(formatContext, allOptions.p_value());
        if (ret < 0) {
            fprintf(stderr, "Error occurred when opening output file: %s\n", av_error_to_string(ret).c_str());
            return 1;
        }
    }

    {
        ffraii::AVFrameR drawFrame
            = allocVideoFrame(AVPixelFormat::AV_PIX_FMT_0RGB, videoCodecContext->width, videoCodecContext->height);
        if (!drawFrame) {
            return 1;
        }

        // encoder input
        ffraii::AVFrameR videoFrame
            = allocVideoFrame(videoCodecContext->pix_fmt, videoCodecContext->width, videoCodecContext->height);
        if (!videoFrame) {
            return 1;
        }

        ffraii::SwsContextR scaler(
            sws_getContext(
                drawFrame->width, drawFrame->height, static_cast<AVPixelFormat>(drawFrame->format),
                videoFrame->width, videoFrame->height, static_cast<AVPixelFormat>(videoFrame->format),
                SWS_BICUBIC, nullptr, nullptr, nullptr)
        );

        if (!scaler) {
            fprintf(stderr, "Could not initialize the conversion context\n");
            return 1;
        }

        // now write a hour long video
        for (uint64_t second = 0; second < std::chrono::seconds(std::chrono::minutes(1)).count(); ++second) {
            //write 1 second of video (that is, 25 frames)
            for (unsigned frameId = 0; frameId < FramesPerSecond; ++frameId) {
                const int64_t pts = second * FramesPerSecond + frameId;
                const uint64_t millisecond = second * 1000 + frameId * 1000 / FramesPerSecond;

                {
                    //draw the frame in RGB and then convert it into YUV
                    std::memset(drawFrame->data[0], (uint8_t)pts, drawFrame->height * drawFrame->width * 4);
                    /*
                    uint32_t* pPixel = reinterpret_cast<uint32_t*>(drawFrame->data[0]);
                    uint32_t color = (uint32_t)pts;
                    for (int y = 0; y < drawFrame->height; ++y) {
                        for (int x = 0; x < drawFrame->width; ++x) {
                            pPixel[x + y * drawFrame->width] = color;
                        }
                    }
                    */
                    {
                        int ret = sws_scale(
                            scaler,
                            (const uint8_t * const *)drawFrame->data, drawFrame->linesize, 0
                            , videoFrame->height, videoFrame->data, videoFrame->linesize);
                        if (ret < 0) {
                            fprintf(stderr, "sws_scale failed:%s\n", av_error_to_string(ret).c_str());
                            return 1;
                        }
                    }
                }

                {
                    int ret = av_frame_make_writable(videoFrame);
                    assert(ret >= 0);
                }

                videoFrame->pts = pts;

                int ret = avcodec_send_frame(videoCodecContext, videoFrame);
                if (ret < 0) {
                    fprintf(stderr, "Error sending a frame for encoding:%s\n", av_error_to_string(ret).c_str());
                    return 1;
                }

                AVPacket pkt = {};
                av_init_packet(&pkt);

                while (ret >= 0) {
                    ret = avcodec_receive_packet(videoCodecContext, &pkt);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    else if (ret < 0) {
                        fprintf(stderr, "Error during encoding:%s\n", av_error_to_string(ret).c_str());
                        return 1;
                    }

                    /* rescale output packet timestamp values from codec to stream timebase */
                    av_packet_rescale_ts(&pkt, TimeBase, pVideoStream->time_base);
                    pkt.stream_index = pVideoStream->index;

                    /* Write the compressed frame to the media file. */
                    {
                        int ret = av_interleaved_write_frame(formatContext, &pkt);
                        if (ret < 0) {
                            fprintf(stderr, "av_interleaved_write_frame failed:%s\n", av_error_to_string(ret).c_str());
                            return 1;
                        }
                    }


                    av_packet_unref(&pkt);
                }
            }

            //write 1 second of audio
            if (second % 2) { // every odd second write signal

            } else { // every even -- silence

            }
        }
    }

    {
       int ret = av_write_trailer(formatContext);
       if (ret < 0) {
           fprintf(stderr, "av_write_trailer failed: %s\n", av_error_to_string(ret).c_str());
           return 1;
       }
    }

    {
        int ret = avio_closep(&formatContext->pb);
        if (ret < 0) {
            fprintf(stderr, "avio_closep failed: %s\n", av_error_to_string(ret).c_str());
            return 1;
        }
    }


    return 0;
}
