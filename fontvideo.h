#ifndef _FONTVIDEO_H_
#define _FONTVIDEO_H_ 1

#include"avdec.h"
#include"siowrap.h"

#include<stdio.h>
#include<rttimer/rttimer.h>
#include<unibmp.h>

#include<stdatomic.h>

typedef struct fontvideo_frame_struct fontvideo_frame_t, *fontvideo_frame_p;
struct fontvideo_frame_struct
{
    double timestamp;
    uint32_t index;
    atomic_int rendering;
    atomic_int rendered;
    double rendering_start_time;
    double rendering_time_consuming;

    uint32_t w, h;
    uint32_t *data;
    uint32_t **row;
    uint8_t *c_data;
    uint8_t **c_row;

    uint32_t raw_w, raw_h;
    uint32_t *raw_data;
    uint32_t **raw_data_row;
    fontvideo_frame_p next;
};

typedef struct fontvideo_audio_struct fontvideo_audio_t, *fontvideo_audio_p;
struct fontvideo_audio_struct
{
    double timestamp;
    size_t frames;
    float *buffer;
    float *ptr_left;
    size_t step_left; // not in bytes, but in sizeof(float)
    float *ptr_right;
    size_t step_right;
    fontvideo_audio_p next;
};

typedef struct fontvideo_struct
{
    void *userdata;
    FILE *log_fp;
    FILE *graphics_out_fp;
    int output_utf8;
    int need_chcp;
    double precache_seconds;
    int verbose;
    int verbose_threading;
    int real_time_play;
    int do_audio_output;
    int do_colored_output;
    uint32_t output_w, output_h;
    int allow_opengl;
    int no_frameskip;
    int do_color_invert;
    void *opengl_renderer;

    char *font_face;
    uint32_t font_w, font_h;
    UniformBitmap_p font_matrix;
    float *font_luminance_image;
    uint32_t font_mat_w;
    uint32_t font_mat_h;
    size_t font_code_count;
    uint32_t *font_codes;
    int font_is_blackwhite;

    int prepared;

    fontvideo_frame_p frames;
    fontvideo_frame_p frame_last;
    atomic_int frame_lock;

#ifndef FONTVIDEO_NO_SOUND
    fontvideo_audio_p audios;
    fontvideo_audio_p audio_last;
    atomic_int audio_lock;
#endif

    uint32_t frame_count;
    uint32_t precached_frame_count;
    uint32_t rendering_frame_count;
    uint32_t rendered_frame_count;
    double avg_rendering_time_consuming;
    double last_output_time;

    char *utf8buf;
    size_t utf8buf_size;
#if _WIN32
    int do_old_console_output;
    void *old_console_buffer;
#endif

    char *output_frame_images_prefix;

    atomic_int doing_decoding;
    atomic_int doing_output;

    int tailed;
    avdec_p av;
#ifndef FONTVIDEO_NO_SOUND
    siowrap_p sio;
#endif
    rttimer_t tmr;
}fontvideo_t, *fontvideo_p;

fontvideo_p fv_create
(
    char *input_file,
    FILE *log_fp,
    int log_verbose,
    int log_verbose_threading,
    FILE *graphics_out_fp,
    char *assets_meta_file,
    uint32_t x_resolution,
    uint32_t y_resolution,
    double precache_seconds,
    int do_audio_output,
    double start_timestamp
);

int fv_allow_opengl(fontvideo_p fv);
int fv_show_prepare(fontvideo_p fv);
int fv_show(fontvideo_p fv);
int fv_render(fontvideo_p fv);
void fv_destroy(fontvideo_p fv);

#endif
