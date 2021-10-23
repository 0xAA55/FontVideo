#include"fontvideo.h"

#include<stdlib.h>
#include <locale.h>

#if defined(_WIN32) || defined(__MINGW32__)
#include <Windows.h>

static void init_console(fontvideo_p fv)
{
    fv->output_utf8 = SetConsoleOutputCP(CP_UTF8);
    if (!fv->output_utf8)
    {
        fv->output_utf8 = SetConsoleCP(CP_UTF8);
    }
}

#else

static void init_console(fontvideo_p fv)
{
    setlocale(LC_ALL, "");
    fv->output_utf8 = 1;
}

#endif

static void fv_on_get_video(void *bitmap, int width, int height, size_t pitch, double time_base, enum AVSampleFormat pixel_format)
{
    printf("Get video data: %p, %d, %d, %zu, %lf, %d\n", bitmap, width, height, pitch, time_base, pixel_format);
}

static void fv_on_get_audio(void **samples_of_channel, int channel_count, size_t num_samples_per_channel, double time_base, enum AVSampleFormat sample_fmt)
{
    printf("Get audio data: %p, %d, %zu, %lf, %d\n", samples_of_channel, channel_count, num_samples_per_channel, time_base, sample_fmt);
}

static void fv_on_write_sample(siowrap_p s, int sample_rate, int channel_count, void **pointers_per_channel, size_t *sample_pointer_steps, size_t samples_to_write_per_channel)
{
    size_t i;
    int c;

    for (i = 0; i < samples_to_write_per_channel; i++)
    {
        for (c = 0; c < channel_count; c++)
        {
            float *sample_ptr = pointers_per_channel[c];

            sample_ptr[0] = 0;

            sample_ptr = (float*)((uint8_t *)sample_ptr + sample_pointer_steps[c]);
        }
    }
}

fontvideo_p fv_create(char *input_file, FILE *log_fp)
{
    fontvideo_p fv = NULL;

    fv = calloc(1, sizeof fv[0]);
    if (!fv) return fv;
    fv->log_fp = log_fp;

    if (log_fp == stdout || log_fp == stderr)
    {
        init_console(fv);
    }

    fv->av = avdec_open(input_file, stderr);
    if (!fv->av) goto FailExit;

    fv->sio = siowrap_create(log_fp, SoundIoFormatFloat32NE, 48000, fv_on_write_sample);
    if (!fv->sio) goto FailExit;

    return fv;
FailExit:
    fv_destroy(fv);
    return NULL;
}
