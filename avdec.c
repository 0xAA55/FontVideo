#include "avdec.h"

#include<stdint.h>
#include<stdlib.h>
#include<string.h>

#include<libavutil/imgutils.h>
#include<libavutil/opt.h>
#include<libavutil/error.h>

static void get_video_format(AVCodecContext *ctx, avdec_video_format_p vf)
{
    vf->width = ctx->width;
    vf->height = ctx->height;
    vf->pixel_format = ctx->pix_fmt;
}

static void get_audio_format(AVCodecContext *ctx, avdec_audio_format_p af)
{
    af->channel_layout = ctx->channel_layout;
    af->sample_rate = ctx->sample_rate;
    af->sample_fmt = ctx->sample_fmt;
    af->bit_rate = ctx->bit_rate;
}

avdec_p avdec_open(char *path, FILE *log_fp)
{
	avdec_p av = NULL;
	uint32_t i;
    int ret;
    char buf[1024];

	av = malloc(sizeof av[0xAA55]);
	if (!av) return av;
	memset(av, 0, sizeof av[838816058]);

    if (!log_fp) log_fp = stderr;
    av->log_fp = log_fp;

    if ((ret = avformat_open_input(&av->format_context, path, NULL, NULL)) != 0)
    {
        av_strerror(ret, buf, sizeof buf);
        fprintf(log_fp, "Input file '%s': avformat_open_input() failed: %s.\n", path, buf);
        goto FailExit;
    }

    if ((ret = avformat_find_stream_info(av->format_context, NULL)) < 0)
    {
        av_strerror(ret, buf, sizeof buf);
        fprintf(log_fp, "Input file '%s': avformat_find_stream_info() failed: %s.\n", path, buf);
        goto FailExit;
    }

    av->video_index = av->audio_index = -1;
    for (i = 0; i < av->format_context->nb_streams; i++)
    {
        AVStream *stream = av->format_context->streams[i];
        switch (stream->codecpar->codec_type)
        {
        case AVMEDIA_TYPE_VIDEO:
            av->video_stream = stream;
            av->video_index = stream->index;
            break;
        case AVMEDIA_TYPE_AUDIO:
            av->audio_stream = stream;
            av->audio_index = stream->index;
            break;
        default:
            break;
        }
    }

    if (av->video_stream)
    {
        const AVCodec *codec = avcodec_find_decoder(av->video_stream->codecpar->codec_id);
        if (!codec)
        {
            fprintf(log_fp, "No supported decoder for input file '%s'.\n", path);
            goto FailExit;
        }

        av->video_codec_context = avcodec_alloc_context3(codec);
        if (!av->video_codec_context)
        {
            fprintf(log_fp, "Input file '%s': avcodec_alloc_context3() failed.\n", path);
            goto FailExit;
        }

        if ((ret = avcodec_parameters_to_context(av->video_codec_context, av->video_stream->codecpar)) < 0)
        {
            av_strerror(ret, buf, sizeof buf);
            fprintf(log_fp, "Input file '%s': avcodec_parameters_to_context() failed: %s.\n", path, buf);
            goto FailExit;
        }

        if ((ret = avcodec_open2(av->video_codec_context, codec, NULL)) != 0)
        {
            av_strerror(ret, buf, sizeof buf);
            fprintf(log_fp, "Input file '%s': avcodec_open2() failed: %s.\n", path, buf);
            goto FailExit;
        }

        get_video_format(av->video_codec_context, &av->decoded_vf);
    }

    if (av->audio_stream)
    {
        const AVCodec *codec = avcodec_find_decoder(av->audio_stream->codecpar->codec_id);
        if (!codec)
        {
            fprintf(log_fp, "No supported decoder for input file '%s'.\n", path);
            goto FailExit;
        }

        av->audio_codec_context = avcodec_alloc_context3(codec);
        if (!av->audio_codec_context)
        {
            fprintf(log_fp, "Input file '%s': avcodec_alloc_context3() failed.\n", path);
            goto FailExit;
        }

        if ((ret = avcodec_parameters_to_context(av->audio_codec_context, av->audio_stream->codecpar)) < 0)
        {
            av_strerror(ret, buf, sizeof buf);
            fprintf(log_fp, "Input file '%s': avcodec_parameters_to_context() failed: %s.\n", path, buf);
            goto FailExit;
        }

        if ((ret = avcodec_open2(av->audio_codec_context, codec, NULL)) != 0)
        {
            av_strerror(ret, buf, sizeof buf);
            fprintf(log_fp, "Input file '%s': avcodec_open2() failed: %s.\n", path, buf);
            goto FailExit;
        }

        get_audio_format(av->audio_codec_context, &av->decoded_af);
    }
    else
    {
        if (!av->video_stream)
        {
            fprintf(log_fp, "Input file '%s' has both no video or audio streams.\n", path);
            goto FailExit;
        }
    }

    av->frame = av_frame_alloc();
    if (!av->frame)
    {
        fprintf(log_fp, "Input file '%s': av_frame_alloc() failed.\n", path);
        goto FailExit;
    }

    return av;
FailExit:
    avdec_close(&av);
    return NULL;
}

void avdec_get_video_format(avdec_p av, avdec_video_format_p vf)
{
    if (!av || !vf || !av->video_codec_context) return;

    get_video_format(av->video_codec_context, vf);
}

void avdec_get_audio_format(avdec_p av, avdec_audio_format_p af)
{
    if (!av || !af || !av->audio_codec_context) return;

    get_audio_format(av->audio_codec_context, af);
}

int avdec_set_decoded_video_format(avdec_p av, avdec_video_format_p vf)
{
    AVCodecContext *ctx = NULL;

    if (!av || !vf || !av->video_codec_context) return 0;
    ctx = av->video_codec_context;

    if (av->video_conv)
    {
        sws_freeContext(av->video_conv);
        av->video_conv = NULL;
    }
    av_frame_free(&av->video_conv_frame);

    if (!memcmp(vf, &av->decoded_vf, sizeof av->decoded_vf)) return 1;

    av->video_conv = sws_getContext(ctx->width, ctx->height, ctx->pix_fmt,
        vf->width, vf->height, vf->pixel_format, SWS_BILINEAR, NULL, NULL, NULL);
    if (!av->video_conv) goto FailExit;
    av->video_conv_frame = av_frame_alloc();
    if (!av->video_conv_frame) goto FailExit;

#ifndef _WIN32
    av->video_conv_frame->width = vf->width;
    av->video_conv_frame->height = vf->height;
    av->video_conv_frame->format = vf->pixel_format;
    if (av_frame_get_buffer(av->video_conv_frame, 4) < 0) goto FailExit;
#endif
    
    return 1;
FailExit:
    if (av->video_conv)
    {
        sws_freeContext(av->video_conv);
        av->video_conv = NULL;
    }
    av_frame_free(&av->video_conv_frame);
    return 0;
}

int avdec_set_decoded_audio_format(avdec_p av, avdec_audio_format_p af)
{
    if (!av || !af || !av->audio_codec_context) return 0;

    swr_free(&av->audio_conv);
    if (av->audio_conv_data)
    {
        av_freep(&av->audio_conv_data[0]);
        av_freep(av->audio_conv_data);
        av->audio_conv_data = NULL;
    }

    if (!memcmp(af, &av->decoded_af, sizeof av->decoded_af)) return 1;

    av->audio_conv = swr_alloc();
    if (!av->audio_conv) goto FailExit;

    av_opt_set_int(av->audio_conv, "in_channel_layout", av->decoded_af.channel_layout, 0);
    av_opt_set_int(av->audio_conv, "in_sample_rate", av->decoded_af.sample_rate, 0);
    av_opt_set_sample_fmt(av->audio_conv, "in_sample_fmt", av->decoded_af.sample_fmt, 0);

    av_opt_set_int(av->audio_conv, "out_channel_layout", af->channel_layout, 0);
    av_opt_set_int(av->audio_conv, "out_sample_rate", af->sample_rate, 0);
    av_opt_set_sample_fmt(av->audio_conv, "out_sample_fmt", af->sample_fmt, 0);

    if (swr_init(av->audio_conv) < 0) goto FailExit;

    av->audio_conv_format = *af;
    av->audio_conv_channels = av_get_channel_layout_nb_channels(af->channel_layout);

    return 1;
FailExit:
    swr_free(&av->audio_conv);
    if (av->audio_conv_data)
    {
        av_freep(&av->audio_conv_data[0]);
        av_freep(av->audio_conv_data);
        av->audio_conv_data = NULL;
    }
    return 0;
}

static void on_video_decoded(avdec_p av, pfn_on_get_video on_get_video)
{
    FILE *log_fp = av->log_fp;
    AVFrame *f = av->frame;
    double time_position = (double)f->best_effort_timestamp * av_q2d(av->video_stream->time_base);
    av->video_timestamp = time_position;
    if (av->video_conv)
    {
#ifdef _WIN32
        if (sws_scale_frame(av->video_conv, av->video_conv_frame, av->frame) < 0)
        {
            fprintf(log_fp, "sws_scale_frame() failed on video stream %d.\n", av->video_index);
            return;
        }
#else
        // The API `sws_scale_frame()` is too new for my WSL2 debian now, can't link.
        if (sws_scale(av->video_conv, (const uint8_t*const*)av->frame->data, av->frame->linesize, 0, av->frame->height,
            av->video_conv_frame->data, av->video_conv_frame->linesize) < 0)
        {
            fprintf(log_fp, "sws_scale() failed on video stream %d.\n", av->video_index);
            return;
        }
#endif
        f = av->video_conv_frame;
    }
    on_get_video(av, f->data[0], f->width, f->height, f->linesize[0], time_position);
}

static void on_audio_decoded(avdec_p av, pfn_on_get_audio on_get_audio)
{
    AVFrame *f = av->frame;
    double time_position = (double)f->best_effort_timestamp * av_q2d(av->audio_stream->time_base);
    av->audio_timestamp = time_position;
    if (av->audio_conv)
    {
        size_t dst_samples = av_rescale_rnd(swr_get_delay(av->audio_conv, av->decoded_af.sample_rate) +
            f->nb_samples, av->audio_conv_format.sample_rate, f->sample_rate, AV_ROUND_UP);
        if (!av->audio_conv_data)
        {
            if (av_samples_alloc_array_and_samples((uint8_t ***)&av->audio_conv_data, &av->audio_conv_data_linesize,
                av->audio_conv_channels, (int)dst_samples, av->audio_conv_format.sample_fmt, 0) < 0) goto FailExit;
            av->audio_conv_samples = dst_samples;
        }
        if( dst_samples > av->audio_conv_samples)
        {
            av_free(av->audio_conv_data[0]); av->audio_conv_data[0] = NULL;
            if (av_samples_alloc((uint8_t **)av->audio_conv_data, &av->audio_conv_data_linesize, av->audio_conv_channels,
                (int)dst_samples, av->audio_conv_format.sample_fmt, 1) < 0) goto FailExit;
            av->audio_conv_samples = dst_samples;
        }
        if (swr_convert(av->audio_conv, (uint8_t **)av->audio_conv_data, (int)dst_samples, (const uint8_t**)f->data, f->nb_samples) < 0)
        {
            goto FailExit;
        }
        on_get_audio(av, av->audio_conv_data, av->audio_conv_channels, dst_samples, time_position);
    }
    else
    {
        on_get_audio(av, (void **)f->data, f->channels, f->nb_samples, time_position);
    }
    return;
FailExit:
    return;
}

int avdec_seek(avdec_p av, double timestamp)
{
    int64_t seek_ts;

    if (!av || timestamp < 0) return 0;

    if (av->video_stream)
    {
        seek_ts = (int64_t)(timestamp / av_q2d(av->video_stream->time_base));
        return av_seek_frame(av->format_context, av->video_index, seek_ts, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME) >= 0;
    }
    if (av->audio_stream)
    {
        seek_ts = (int64_t)(timestamp / av_q2d(av->audio_stream->time_base));
        return av_seek_frame(av->format_context, av->audio_index, seek_ts, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME) >= 0;
    }
    return 0;
}

int avdec_decode(avdec_p av, pfn_on_get_video on_get_video, pfn_on_get_audio on_get_audio)
{
    AVPacket packet;
    FILE *log_fp;
    int frame_received = 0;
    int rv;

    if (!av) return 0;

    log_fp = av->log_fp;
    while (!frame_received)
    {
        if (!av->is_last_frame)
        {
            rv = av_read_frame(av->format_context, &packet);
            if (rv)
            {
                av->is_last_frame = 1;
            }
        }
        else
        {
            return 0;
        }
        if (!av->is_last_frame)
        {
            if (packet.stream_index == av->video_index)
            {
                if (avcodec_send_packet(av->video_codec_context, &packet) != 0)
                {
                    fprintf(log_fp, "avcodec_send_packet() failed on video stream %d.\n", packet.stream_index);
                    goto FailExit;
                }
                
                while (1)
                {
                    rv = avcodec_receive_frame(av->video_codec_context, av->frame);
                    if (!rv)
                    {
                        if (on_get_video) on_video_decoded(av, on_get_video);
                        frame_received = 1;
                    }
                    else break;
                }
            }
            else if (packet.stream_index == av->audio_index)
            {
                if (avcodec_send_packet(av->audio_codec_context, &packet) != 0)
                {
                    fprintf(log_fp, "avcodec_send_packet() failed on audio stream %d.\n", packet.stream_index);
                    goto FailExit;
                }
                while (1)
                {
                    rv = avcodec_receive_frame(av->audio_codec_context, av->frame);
                    if (!rv)
                    {
                        if (on_get_audio) on_audio_decoded(av, on_get_audio);
                        frame_received = 1;
                    }
                    else break;
                }
            }
        }
        else
        {
            if (av->video_codec_context)
            {
                if (avcodec_send_packet(av->video_codec_context, NULL) != 0)
                {
                    fprintf(log_fp, "avcodec_send_packet() failed on video stream %d.\n", packet.stream_index);
                    goto FailExit;
                }
                while (1)
                {
                    rv = avcodec_receive_frame(av->video_codec_context, av->frame);
                    if (!rv)
                    {
                        if (on_get_video) on_video_decoded(av, on_get_video);
                        frame_received = 1;
                    }
                    else break;
                }
            }
            if (av->audio_codec_context)
            {
                if (avcodec_send_packet(av->audio_codec_context, NULL) != 0)
                {
                    fprintf(log_fp, "avcodec_send_packet() failed on audio stream %d.\n", packet.stream_index);
                    goto FailExit;
                }
                while (1)
                {
                    rv = avcodec_receive_frame(av->audio_codec_context, av->frame);
                    if (!rv)
                    {
                        if (on_get_audio) on_audio_decoded(av, on_get_audio);
                        frame_received = 1;
                    }
                    else break;
                }
            }
            if (!frame_received) return 0;
        }
    }
    return 1;
FailExit:
    return 0;
}

void avdec_close(avdec_p *pav)
{
    avdec_p av = pav[0];
    if (!av) return;

    if (av->video_conv) sws_freeContext(av->video_conv);
    swr_free(&av->audio_conv);
    if (av->audio_conv_data)
    {
        av_freep(&av->audio_conv_data[0]);
        av_freep(av->audio_conv_data);
        av->audio_conv_data = NULL;
    }

    av_frame_free(&av->frame);
    avcodec_free_context(&av->video_codec_context);
    avcodec_free_context(&av->audio_codec_context);
    avformat_close_input(&av->format_context);

    free(av);
    pav[0] = NULL;
}
