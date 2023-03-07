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

    // Config: the `FILE*` for logging
    FILE *log_fp;

    // Config: the output `FILE*` (normally would be `stdout`)
    FILE *graphics_out_fp;

    // Config: How many frames should be pre-rendered
    double precache_seconds;

    // Config: is doing real time playing or just do rendering and output a bunch of frames?
    int real_time_play;

    // Config: should not to skip frames if the playback is lagging very much?
    int no_frameskip;

    // Config: should initialize audio device to play audio or not?
    int do_audio_output;

    // Config: should do colored output or just mono?
    int do_colored_output;

    // Config: should do color invert?
    int white_background;

    // Config: the weight of brightness in [0, 1], how important of the brightness
    float brightness_weight;

    // Config: should normalize the input frames brightness?
    int normalize_input;

    // Config: output size in characters
    uint32_t output_w, output_h;

    // Config: do avoid repetition or not?
    int no_avoid_repetition;

    // Config: Debug purpose: should write verbose info to log file or not
    int verbose;

    // Config: Debug purpose: should write verbose info about threading to log file or not
    int verbose_threading;

    // Assets: which font face will be used for rendering? will change the console window font to match the font face.
    char* font_face;

    // Assets: the font size in pixels per glyph. Assumed every glyph is the same size.
    uint32_t glyph_width, glyph_height;

    // Assets: the font source image for every glyph.
    UniformBitmap_p glyph_matrix;

    // Assets: the dimension of the glyph matrix in characters.
    uint32_t glyph_matrix_cols, glyph_matrix_rows;

    // Assets: glyph pixels, every glyph is stored in one vertical image, use as kernel array, for doing convolutional compute.
    float *glyph_vertical_array;

    // Assets: the number of the glyphs.
    size_t num_glyph_codes;

    // Assets: the UTF-32 codes for each glyphs.
    uint32_t *glyph_codes;

    // Assets: the calculated average luminance in [0, 1] for each glyph
    float* glyph_brightness;

    // Assets: is the glyphs all full-width?
    int font_is_wide;

    // Assets: is the source image black-white?
    int font_is_blackwhite;

    // Status: if the character assets contains non-ascii, this state will be set to true
    int need_chcp;

    // Status: if configured console to output UTF-8 successfully, this state will be set to true
    int output_utf8;

    // Status: the window size of the candidate glyphs, affected by `brightness_weight` and `num_glyph_codes`
    // The more weight for brightness, the smaller window size
    int candidate_glyph_window_size;

    // Status: is currently finished pre-render frames?
    int prepared;

    // Status: frames queue
    fontvideo_frame_p frames;
    fontvideo_frame_p frame_last;
    atomic_int frame_lock;

#ifndef FONTVIDEO_NO_SOUND
    // Status: audio queue
    fontvideo_audio_p audios;
    fontvideo_audio_p audio_last;
    atomic_int audio_lock;
#endif

    // Status: the current frame.
    uint32_t frame_counter;

    // Status: how many frames is extracted from the source media file and waiting for rendering
    uint32_t precached_frame_count;

    // Status: how many frames is rendering.
    uint32_t rendering_frame_count;

    // Status: how many frames is rendered and waiting for show
    uint32_t rendered_frame_count;

    // Status: avarage render time
    double avg_rendering_time_consuming;

    // Status: timestamp of last showed frame
    double last_output_time;

    // Status: the buffer for output UTF-8 encoded texts to console window.
    char *utf8buf;
    size_t utf8buf_size;

#if _WIN32
    // Config: do old output (using Win32 API) or not?
    int do_old_console_output;

    // Status: the buffer for doing old output
    void *old_console_buffer;
#endif

    // Config: if render to image files, this is the prefix of how to name the image sequence.
    char *output_frame_images_prefix;

    // Status: is doing media file decoding?
    atomic_int doing_decoding;

    // Status: is doing output?
    atomic_int doing_output;

    // Status: is all frames decoded from the media file?
    int tailed;

    // Status: media file decoder
    avdec_p av;

#ifndef FONTVIDEO_NO_SOUND
    // Status: sound driver
    siowrap_p sio;
#endif

    // Status: timer
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
    double start_timestamp,
    int white_background,
    int no_auto_aspect_adjust
);

int fv_show_prepare(fontvideo_p fv);
int fv_show(fontvideo_p fv);
int fv_render(fontvideo_p fv);
void fv_destroy(fontvideo_p fv);

#endif
