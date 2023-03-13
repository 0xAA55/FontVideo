#ifndef _AVDEC_H_
#define _AVDEC_H_ 1

#include<stdio.h>
#include<stdint.h>

#include<libavcodec/avcodec.h>
#include<libavutil/avutil.h>
#include<libavformat/avformat.h>
#include<libswscale/swscale.h>
#include<libswresample/swresample.h>

// Pixel format descriptor for video frames
typedef struct avdec_video_format_struct
{
    int width;
    int height;
    enum AVPixelFormat pixel_format;
}avdec_video_format_t, *avdec_video_format_p;

// Audio format descriptor for audio waveform
typedef struct avdec_audio_format_struct
{
    uint64_t channel_layout;
    int sample_rate;
    enum AVSampleFormat sample_fmt;
    int64_t bit_rate;
}avdec_audio_format_t, *avdec_audio_format_p;

typedef enum avdec_target_enum
{
    avt_for_none = 0,
    avt_for_video = 1,
    avt_for_audio = 2,
    avt_for_both = avt_for_video | avt_for_audio
}avdec_target_t, * avdec_target_p;

typedef struct avdec_struct
{
    // the opened multimedia file context dedicated for video
    AVFormatContext* format_context_video;
    
    // the opened multimedia file context dedicated for audio
    AVFormatContext* format_context_audio;
    
    // streams of the file
    AVStream *video_stream;
    AVStream *audio_stream;
    int video_index; // and it's index
    int audio_index;

    // the codecs for each stream to decode it
    AVCodecContext *video_codec_context;
    AVCodecContext *audio_codec_context;

    // frame read from raw
    AVFrame *frame;

    // Current timestamp
    double video_timestamp;
    double audio_timestamp;

    // decoded format for each stream
    avdec_video_format_t decoded_vf;
    avdec_audio_format_t decoded_af;

    // if the decoded plain data should be converted, there's the converters
    struct SwsContext *video_conv;
    struct SwrContext *audio_conv;

    // if is finished decoding
    int video_eof;
    int audio_eof;

    // converted data from video converter
    AVFrame *video_conv_frame;

    // converted data from audio converter
    void **audio_conv_data;
    int audio_conv_data_linesize;
    size_t audio_conv_samples;
    avdec_audio_format_t audio_conv_format;
    int audio_conv_channels;

    // put whatever data you want
    void *userdata;

    // if you pass NULL at avdec_open() it will be changed to stderr, and you can change it again to NULL.
    FILE *log_fp;
}avdec_t, *avdec_p;

typedef void(*pfn_on_get_video)(avdec_p av, void *bitmap, int width, int height, size_t pitch, double timestamp);
typedef void(*pfn_on_get_audio)(avdec_p av, void **samples_of_channel, int channel_count, size_t num_samples_per_channel, double timestamp);

avdec_p avdec_open(char *path, FILE *log_fp);
void avdec_get_video_format(avdec_p av, avdec_video_format_p vf);
void avdec_get_audio_format(avdec_p av, avdec_audio_format_p af);
int avdec_set_decoded_video_format(avdec_p av, avdec_video_format_p vf); // Conversion will be performed if the format doesn't match the original decoded format.
int avdec_set_decoded_audio_format(avdec_p av, avdec_audio_format_p af);
int avdec_seek(avdec_p av, double timestamp);
int avdec_decode(avdec_p av, pfn_on_get_video on_get_video, pfn_on_get_audio on_get_audio, avdec_target_t target);
void avdec_close(avdec_p *pav);




#endif // !_AVDEC_H_
