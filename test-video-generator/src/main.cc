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

#define NOMINMAX
#include "VideoRenderer.h"
#include "AudioRenderer.h"

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

    AVStream* add_stream(AVFormatContext* pFormatContext) {
        AVStream* pStream = avformat_new_stream(pFormatContext, nullptr);
        if (not pStream) {
            fprintf(stderr, "Could not allocate stream\n");
            return nullptr;
        }
        pStream->id = pFormatContext->nb_streams - 1;

        return pStream;
    }

    constexpr AVPixelFormat STREAM_PIX_FMT = AV_PIX_FMT_YUV420P;
    constexpr int VIDEO_WIDTH = 320;
    constexpr int VIDEO_HEGIHT = 240;
    constexpr unsigned FramesPerSecond = 25;
    const AVRational VideoStreamTimeBase{ 1, FramesPerSecond };

    AVCodecContext* alloc_video_codec_context(AVFormatContext* pFormatContext, const AVDictionary* pAllOptions) {
        AVOutputFormat* fmt = pFormatContext->oformat;
        assert(fmt->video_codec != AV_CODEC_ID_NONE);
        AVCodec* videoCodec = avcodec_find_encoder(fmt->video_codec);

        if (!videoCodec) {
            fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(fmt->video_codec));
            return nullptr;
        }

        AVCodecContext* pVideoCodecContext = avcodec_alloc_context3(videoCodec);
        if (!pVideoCodecContext) {
            fprintf(stderr, "Could not alloc an encoding context\n");
            return nullptr;
        }
        pVideoCodecContext->codec_id = fmt->video_codec;
        pVideoCodecContext->bit_rate = 400000;
        pVideoCodecContext->width = VIDEO_WIDTH;
        pVideoCodecContext->height = VIDEO_HEGIHT;
        pVideoCodecContext->time_base = VideoStreamTimeBase;
        pVideoCodecContext->gop_size = 10; /* emit one intra frame every 10 frames at most */
        pVideoCodecContext->pix_fmt = STREAM_PIX_FMT;

        {
            ffraii::AVDictionaryR opt;
            av_dict_copy(opt.p_value(), pAllOptions, 0);

            /* open the codec */
            int ret = avcodec_open2(pVideoCodecContext, nullptr, opt.p_value());

            if (ret < 0) {
                fprintf(stderr, "Could not open video codec: %s\n", av_error_to_string(ret).c_str());
                return nullptr;
            }
        }

        return pVideoCodecContext;
    }

    AVCodecContext* alloc_audio_codec_context(AVFormatContext* pFormatContext, const AVDictionary* pAllOptions) {
        AVOutputFormat* fmt = pFormatContext->oformat;
        assert(fmt->audio_codec != AV_CODEC_ID_NONE);
        AVCodec* audioCodec = avcodec_find_encoder(fmt->audio_codec);

        if (!audioCodec) {
            fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(fmt->audio_codec));
            return nullptr;
        }

        AVCodecContext* pAudioCodecContext = avcodec_alloc_context3(audioCodec);
        if (!pAudioCodecContext) {
            fprintf(stderr, "Could not alloc an encoding context\n");
            return nullptr;
        }

        pAudioCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
        pAudioCodecContext->bit_rate = 64000;
        pAudioCodecContext->sample_rate = 44100;
        pAudioCodecContext->channels = 1;
        pAudioCodecContext->channel_layout = AV_CH_LAYOUT_MONO;


        {
            ffraii::AVDictionaryR opt;
            av_dict_copy(opt.p_value(), pAllOptions, 0);

            /* open the codec */
            int ret = avcodec_open2(pAudioCodecContext, nullptr, opt.p_value());

            if (ret < 0) {
                fprintf(stderr, "Could not open audio codec: %s\n", av_error_to_string(ret).c_str());
                return nullptr;
            }
        }

        return pAudioCodecContext;
    }

    AVFrame* alloc_audio_frame(AVCodecContext* pAudioCodecContext) {
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            fprintf(stderr, "Error allocating an audio frame\n");
            return nullptr;
        }

        frame->format = pAudioCodecContext->sample_fmt;
        frame->channel_layout = pAudioCodecContext->channel_layout;
        frame->sample_rate = pAudioCodecContext->sample_rate;
        frame->nb_samples = pAudioCodecContext->frame_size * pAudioCodecContext->channels;

        {
            int ret = av_frame_get_buffer(frame, 0);
            if (ret < 0) {
                fprintf(stderr, "Error allocating an audio buffer\n");
                return nullptr;
            }
        }

        return frame;
    }
}

using namespace test_video_generator;

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
    // ----------------- COMMON
    ffraii::AVDictionaryR allOptions;

    // ------------------- INIT VIDEO

    AVStream* pVideoStream = add_stream(formatContext);
    if (!pVideoStream) {
        return 1;
    }
    pVideoStream->time_base = VideoStreamTimeBase;

    ffraii::AVCodecContextR videoCodecContext(alloc_video_codec_context(formatContext, allOptions));
    if (!videoCodecContext) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        return 1;
    }

    // copy the stream parameters to the muxer
    {
        int ret = avcodec_parameters_from_context(pVideoStream->codecpar, videoCodecContext);
        if (ret < 0) {
            fprintf(stderr, "Could not copy the stream parameters\n");
            return 1;
        }
    }

    // ------------------ INIT AUDIO

    assert(formatContext->oformat->audio_codec != AV_CODEC_ID_NONE);
    AVStream* pAudioStream = avformat_new_stream(formatContext, nullptr);
    assert(pAudioStream);
    pAudioStream->id = formatContext->nb_streams - 1;

    ffraii::AVCodecContextR audioCodecContext(alloc_audio_codec_context(formatContext, allOptions));
    if (!audioCodecContext) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        return 1;
    }
    pAudioStream->time_base = { 1, audioCodecContext->sample_rate };

    // copy the stream parameters to the muxer
    {
        int ret = avcodec_parameters_from_context(pAudioStream->codecpar, audioCodecContext);
        if (ret < 0) {
            fprintf(stderr, "Could not copy the stream parameters\n");
            return 1;
        }
    }

    // ------------------- RENDER

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
        ffraii::AVFrameR audioFrame(alloc_audio_frame(audioCodecContext));
        if (!audioFrame) {
            fprintf(stderr, "Failed to audio frame");
            return 1;
        }
        audioFrame->pts = 0;
        AudioRenderer audioRenderer(
            audioCodecContext->channels,
            audioCodecContext->frame_size,
            audioCodecContext->sample_rate
        );


        VideoRenderer renderer(VIDEO_WIDTH, VIDEO_HEGIHT);

        // encoder input
        ffraii::AVFrameR videoFrame
            = allocVideoFrame(videoCodecContext->pix_fmt, videoCodecContext->width, videoCodecContext->height);
        if (!videoFrame) {
            return 1;
        }

        ffraii::SwsContextR scaler(
            sws_getContext(
                VIDEO_WIDTH, VIDEO_HEGIHT, AV_PIX_FMT_BGR0,
                videoFrame->width, videoFrame->height, static_cast<AVPixelFormat>(videoFrame->format),
                SWS_BICUBIC, nullptr, nullptr, nullptr)
        );

        if (!scaler) {
            fprintf(stderr, "Could not initialize the conversion context\n");
            return 1;
        }


        // now write a hour long video
        for (uint64_t second = 0; second < std::chrono::seconds(std::chrono::hours(1)).count(); ++second) {

            const bool beep = 1 == (second % 2);
            //write 1 second of video (that is, 25 frames)
            for (unsigned frameId = 0; frameId < FramesPerSecond; ++frameId) {
                const int64_t pts = second * FramesPerSecond + frameId;
                const uint64_t millisecond = second * 1000 + frameId * 1000 / FramesPerSecond;

                {
                    renderer.render(millisecond, beep);
                    {
                        uint8_t* array1[8] = {};
                        array1[0] = renderer.data();

                        int array2[8] = {};
                        array2[0] = 4 * VIDEO_WIDTH;

                        int ret = sws_scale(
                            scaler,
                            array1, array2, 0
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
                    fprintf(stderr, "Error sending a video frame for encoding:%s\n", av_error_to_string(ret).c_str());
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
                    av_packet_rescale_ts(&pkt, VideoStreamTimeBase, pVideoStream->time_base);
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

            {
                const uint64_t stopMillis = (second + 1) * 1000;
                while (audioRenderer.millisRendered() < stopMillis) {
                    audioRenderer.renderFrameInto(audioFrame->data[0]);

                    int ret = avcodec_send_frame(audioCodecContext, audioFrame);
                    if (ret < 0) {
                        fprintf(stderr, "Error sending an audio frame for encoding:%s\n", av_error_to_string(ret).c_str());
                        return 1;
                    }

                    AVPacket pkt = {};
                    av_init_packet(&pkt);

                    while (ret >= 0) {
                        ret = avcodec_receive_packet(audioCodecContext, &pkt);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            break;
                        else if (ret < 0) {
                            fprintf(stderr, "Error during audio encoding:%s\n", av_error_to_string(ret).c_str());
                            return 1;
                        }

                        pkt.stream_index = pAudioStream->index;
                        av_packet_rescale_ts(&pkt, pAudioStream->time_base, pAudioStream->time_base);

                        /* Write the compressed frame to the media file. */
                        {
                            int ret = av_interleaved_write_frame(formatContext, &pkt);
                            if (ret < 0) {
                                fprintf(stderr, "av_interleaved_write_frame failed:%s\n", av_error_to_string(ret).c_str());
                                return 1;
                            }
                        }

                        audioFrame->pts += audioCodecContext->frame_size;
                        av_packet_unref(&pkt);
                    }
                }
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
