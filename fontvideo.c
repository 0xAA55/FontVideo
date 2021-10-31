#include"fontvideo.h"

#include<stdlib.h>
#include<locale.h>
#include<assert.h>
#include<C_dict/dictcfg.h>
#include<omp.h>
#include<threads.h>

#if defined(_DEBUG)
#   define DEBUG_OUTPUT_TO_SCREEN 0
#endif

#if defined(_WIN32) || defined(__MINGW32__)
#include <Windows.h>

#define SUBDIR "\\"

static void init_console(fontvideo_p fv)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD ConMode;

    if (fv->need_chcp)
    {
        fv->output_utf8 = SetConsoleOutputCP(CP_UTF8);
        if (!fv->output_utf8)
        {
            fv->output_utf8 = SetConsoleCP(CP_UTF8);
        }
    }

    if (fv->real_time_play)
    {
        GetConsoleMode(hConsole, &ConMode);
        SetConsoleMode(hConsole, ConMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

static void set_cursor_xy(int x, int y)
{
    COORD pos = {(SHORT)x, (SHORT)y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

static void set_console_color(fontvideo_p fv, int clr)
{
    switch (clr)
    {
    case 0: strcat(fv->utf8buf, "\x1b[1;0m");  break;
    case 1: strcat(fv->utf8buf, "\x1b[1;31m"); break;
    case 2: strcat(fv->utf8buf, "\x1b[1;32m"); break;
    case 3: strcat(fv->utf8buf, "\x1b[1;33m"); break;
    case 4: strcat(fv->utf8buf, "\x1b[1;34m"); break;
    case 5: strcat(fv->utf8buf, "\x1b[1;35m"); break;
    case 6: strcat(fv->utf8buf, "\x1b[1;36m"); break;
    case 7: strcat(fv->utf8buf, "\x1b[1;37m"); break;
    }
}

static void yield()
{
    if (!SwitchToThread()) Sleep(1);
}

#else // For non-Win32 appliction, assume it's linux/unix and runs in terminal

#define SUBDIR "/"

static void init_console(fontvideo_p fv)
{
    setlocale(LC_ALL, "");
    fv->output_utf8 = 1;
}

static void set_cursor_xy(int x, int y)
{
    printf("\033[%d;%dH", y + 1, x + 1);
}

static void set_console_color(fontvideo_p fv, int clr)
{
    switch (clr)
    {
    case 0: strcat(fv->utf8buf, "\x1B[1;0m" );  break;
    case 1: strcat(fv->utf8buf, "\x1B[1;31m"); break;
    case 2: strcat(fv->utf8buf, "\x1B[1;32m"); break;
    case 3: strcat(fv->utf8buf, "\x1B[1;33m"); break;
    case 4: strcat(fv->utf8buf, "\x1B[1;34m"); break;
    case 5: strcat(fv->utf8buf, "\x1B[1;35m"); break;
    case 6: strcat(fv->utf8buf, "\x1B[1;36m"); break;
    case 7: strcat(fv->utf8buf, "\x1B[1;37m"); break;
    }
}

static void yield()
{
    sched_yield();
}


#endif

static int get_thread_id()
{
    return thrd_current();
}

// For locking the frames link list of `fv->frames` and `fv->frame_last`, not for locking every individual frames
static void lock_frame(fontvideo_p fv)
{
    atomic_int readout = 0;
    atomic_int cur_id = get_thread_id();
    while (
        // First, do a non-atomic test for a quick peek.
        ((readout = fv->frame_lock) != 0) ||

        // Then, perform the actual atomic operation for acquiring the lock.
        ((readout = atomic_exchange(&fv->frame_lock, cur_id)) != 0))
    {
        if (fv->verbose_threading)
        {
            ; // fprintf(stderr, "Frame link list locked by %d. (%d)\n", readout, cur_id);
        }
        yield();
    }
}

// Try lock, if already locked by others, immediately returns 0. If acquired the lock, returns non-zero.
static int try_lock_frame(fontvideo_p fv)
{
    atomic_int expected = 0;
    atomic_int cur_id = get_thread_id();

    // First, do a non-atomic test for a quick peek.
    if (fv->frame_lock) goto NotAcquired;

    // Then, perform the actual atomic operation for acquiring the lock.
    if (atomic_compare_exchange_strong(&fv->frame_lock, &expected, cur_id))
    {
        return 1;
    }
NotAcquired:
    if (fv->verbose_threading)
    {
        fprintf(stderr, "Frame link list locked by %d, try lock failed. (%d)\n", expected, cur_id);
    }
    return 0;
}

static void unlock_frame(fontvideo_p fv)
{
    atomic_exchange(&fv->frame_lock, 0);
}

// Same mechanism for locking audio link list.
static void lock_audio(fontvideo_p fv)
{
    atomic_int readout;
    atomic_int cur_id = get_thread_id();
    while ((readout = atomic_exchange(&fv->audio_lock, cur_id)) != 0)
    {
        if (fv->verbose_threading)
        {
            fprintf(stderr, "Audio link list locked by %d. (%d)\n", readout, cur_id);
        }
        yield();
    }
}

static void unlock_audio(fontvideo_p fv)
{
    atomic_exchange(&fv->audio_lock, 0);
}

static void frame_delete(fontvideo_frame_p f)
{
    if (!f) return;

    free(f->raw_data); // RGBA pixel data
    free(f->raw_data_row);
    free(f->data); // Font CharCode data
    free(f->row);
    free(f->c_data); // Color data
    free(f->c_row);
}

static void audio_delete(fontvideo_audio_p a)
{
    if (!a) return;

    free(a->buffer);
}

// Move a pointer by bytes
static void *move_ptr(void *ptr, ptrdiff_t step)
{
    return (uint8_t *)ptr + step;
}

// Create audio piece and copy the waveform
static fontvideo_audio_p audio_create(void *samples, int channel_count, size_t num_samples_per_channel, double timestamp);

// Callback function for siowrap output sound to libsoundio
static size_t fv_on_write_sample(siowrap_p s, int sample_rate, int channel_count, void **pointers_per_channel, size_t *sample_pointer_steps, size_t samples_to_write_per_channel)
{
    ptrdiff_t i;
    fontvideo_p fv = s->userdata;

    // Destination pointer
    float *dptr = pointers_per_channel[0]; // Mono
    float *dptr_l = dptr; // Stereo left
    float *dptr_r = dptr; // Stereo right

    // Destination step length, by **BYTES**
    size_t dstep = sample_pointer_steps[0]; // Mono
    size_t dstep_l = dstep; // Stereo left
    size_t dstep_r = dstep; // Stereo right

    // Total frames written. One sample for every channel is a frame.
    size_t total = 0;

    // Only support for mono and stereo
    if (channel_count < 1 || channel_count > 2) return 0;

    // Supress warning
    sample_rate;

    // Initialize stereo pointers
    if (channel_count == 2)
    {
        dptr_l = dptr;
        dptr_r = pointers_per_channel[1];
        dstep_l = dstep;
        dstep_r = sample_pointer_steps[1];
    }

    // Because the next operations will change the audio link list, so lock it up.
    lock_audio(fv);
    while (fv->audios)
    {
        // Next audio piece in the link list
        fontvideo_audio_p next = fv->audios->next;

        // Source pointer
        float *sptr = fv->audios->buffer; // Mono
        float *sptr_l = fv->audios->ptr_left; // Stereo left
        float *sptr_r = fv->audios->ptr_right; // Stereo right

        // Source step length, by **SAMPLES**
        size_t sstep = 1; // Mono
        size_t sstep_l = fv->audios->step_left; // Stereo left
        size_t sstep_r = fv->audios->step_right; // Stereo right
        ptrdiff_t writable = fv->audios->frames; // Source frames
        size_t wrote = 0; // Total frames wrote
        if ((size_t)writable > samples_to_write_per_channel - total) writable = (ptrdiff_t)(samples_to_write_per_channel - total);
        if (!writable) break;
        switch (channel_count)
        {
        case 1:
            for (i = 0; i < writable; i++)
            {
                // Copy samples
                dptr[0] = sptr[0];

                // Move pointers
                dptr = move_ptr(dptr, dstep);
                sptr += sstep;
            }
            wrote = i;
            total += wrote;
            break;
        case 2:
            for (i = 0; i < writable; i++)
            {
                // Copy samples
                dptr_l[0] = sptr_l[0];
                dptr_r[0] = sptr_r[0];

                // Move pointers
                dptr_l = move_ptr(dptr_l, dstep_l);
                dptr_r = move_ptr(dptr_r, dstep_r);
                sptr_l += sstep_l;
                sptr_r += sstep_r;
            }
            wrote = i;
            total += wrote;
            break;
        }
        if (wrote >= fv->audios->frames)
        {
            audio_delete(fv->audios);
            fv->audios = next;
        }
        else // If the current audio piece is not all written, reserve the rest of the frames for the next callback.
        {
            size_t rest = fv->audios->frames - wrote;
            fontvideo_audio_p replace;

            // Reserve the rest of the frames
            replace = audio_create(&fv->audios->buffer[wrote * channel_count], channel_count, rest, fv->audios->timestamp);

            // Insert into the link list
            replace->next = next;
            next = fv->audios;
            fv->audios = replace;

            // Delete the current piece
            audio_delete(next);

            // Done writing audio
            break;
        }
    }

    // If all audio pieces is written, fill the rest of the buffer to silence
    if (!fv->audios)
    {
        fv->audio_last = NULL;

        // No need to keep the lock, release ASAP
        unlock_audio(fv);

        // Check if the buffer isn't all filled, then fill if not.
        if (total < samples_to_write_per_channel)
        {
            ptrdiff_t writable = samples_to_write_per_channel - total;
            switch (channel_count)
            {
            case 1:
                for (i = 0; i < writable; i++)
                {
                    dptr[0] = 0;
                    dptr = move_ptr(dptr, dstep);
                }
                total += writable;
                break;
            case 2:
                for (i = 0; i < writable; i++)
                {
                    dptr_l[0] = 0;
                    dptr_r[0] = 0;
                    dptr_l = move_ptr(dptr_l, dstep_l);
                    dptr_r = move_ptr(dptr_r, dstep_r);
                }
                total += writable;
                break;
            }
        }
    }
    else
    {
        // Make sure the link list is unlocked properly.
        unlock_audio(fv);
    }
    return total;
}

// Convert 32-bit unicode into UTF-8 form one by one.
// The UTF-8 output string pointer will be moved to the next position.
static void u32toutf8
(
    char **ppUTF8,
    const uint32_t CharCode
)
{
    char *pUTF8 = ppUTF8[0];

    if (CharCode >= 0x4000000)
    {
        *pUTF8++ = 0xFC | ((CharCode >> 30) & 0x01);
        *pUTF8++ = 0x80 | ((CharCode >> 24) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 18) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 12) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 6) & 0x3F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else if (CharCode >= 0x200000)
    {
        *pUTF8++ = 0xF8 | ((CharCode >> 24) & 0x03);
        *pUTF8++ = 0x80 | ((CharCode >> 18) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 12) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 6) & 0x3F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else if (CharCode >= 0x10000)
    {
        *pUTF8++ = 0xF0 | ((CharCode >> 18) & 0x07);
        *pUTF8++ = 0x80 | ((CharCode >> 12) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 6) & 0x3F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else if (CharCode >= 0x0800)
    {
        *pUTF8++ = 0xE0 | ((CharCode >> 12) & 0x0F);
        *pUTF8++ = 0x80 | ((CharCode >> 6) & 0x3F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else if (CharCode >= 0x0080)
    {
        *pUTF8++ = 0xC0 | ((CharCode >> 6) & 0x1F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else
    {
        *pUTF8++ = (char)CharCode;
    }

    // Move the pointer
    ppUTF8[0] = pUTF8;
}

// Convert UTF-8 character into 32-bit unicode.
// The UTF-8 string pointer will be moved to the next position.
static uint32_t utf8tou32char
(
    char **ppUTF8
)
{
    size_t cb = 0;
    uint32_t ret = 0;
    char *pUTF8 = ppUTF8[0];

    // Detect the available room size of the UTF-8 buffer
    while (cb < 6 && pUTF8[cb]) cb++;

    if ((pUTF8[0] & 0xFE) == 0xFC) // 1111110x
    {
        if (6 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x01) << 30) |
                (((uint32_t)pUTF8[1] & 0x3F) << 24) |
                (((uint32_t)pUTF8[2] & 0x3F) << 18) |
                (((uint32_t)pUTF8[3] & 0x3F) << 12) |
                (((uint32_t)pUTF8[4] & 0x3F) << 6) |
                (((uint32_t)pUTF8[5] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[6];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xFC) == 0xF8) // 111110xx
    {
        if (5 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x03) << 24) |
                (((uint32_t)pUTF8[1] & 0x3F) << 18) |
                (((uint32_t)pUTF8[2] & 0x3F) << 12) |
                (((uint32_t)pUTF8[3] & 0x3F) << 6) |
                (((uint32_t)pUTF8[4] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[5];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xF8) == 0xF0) // 11110xxx
    {
        if (4 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x07) << 18) |
                (((uint32_t)pUTF8[1] & 0x3F) << 12) |
                (((uint32_t)pUTF8[2] & 0x3F) << 6) |
                (((uint32_t)pUTF8[3] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[4];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xF0) == 0xE0) // 1110xxxx
    {
        if (3 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x0F) << 12) |
                (((uint32_t)pUTF8[1] & 0x3F) << 6) |
                (((uint32_t)pUTF8[2] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[3];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xE0) == 0xC0) // 110xxxxx
    {
        if (2 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x1F) << 6) |
                (((uint32_t)pUTF8[1] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[2];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xC0) == 0x80) // 10xxxxxx
    {
        // Wrong encode
        goto FailExit;
    }
    else if ((pUTF8[0] & 0x80) == 0x00) // 0xxxxxxx
    {
        ret = pUTF8[0] & 0x7F;
        ppUTF8[0] = &pUTF8[1];
        return ret;
    }
    return ret;
FailExit:
    // If convert failed, null-char will be returned.
    return ret;
}

// Load font metadata and config
static int load_font(fontvideo_p fv, char *assets_dir, char *meta_file)
{
    char buf[4096];
    FILE *log_fp = fv->log_fp;
    dict_p d_meta = NULL;
    dict_p d_font = NULL;
    char *font_bmp = NULL;
    char *font_code_txt = NULL;
    FILE *fp_code = NULL;
    size_t i = 0;
    char *font_raw_code = NULL;
    char *ch;
    size_t font_raw_code_size = 0;
    size_t font_count_max = 0;
    uint32_t code;

    snprintf(buf, sizeof buf, "%s"SUBDIR"%s", assets_dir, meta_file);
    d_meta = dictcfg_load(buf, log_fp); // Parse ini file
    if (!d_meta) goto FailExit;

    d_font = dictcfg_section(d_meta, "[font]"); // Extract section
    if (!d_font) goto FailExit;

    font_bmp = dictcfg_getstr(d_font, "font_bmp", "font.bmp");
    font_code_txt = dictcfg_getstr(d_font, "font_code", "gb2312_code.txt");
    fv->font_w = dictcfg_getint(d_font, "font_width", 12);
    fv->font_h = dictcfg_getint(d_font, "font_height", 12);
    fv->font_code_count = font_count_max = dictcfg_getint(d_font, "font_count", 127);
    fv->font_codes = malloc(font_count_max * sizeof fv->font_codes[0]);
    if (!fv->font_codes)
    {
        fprintf(log_fp, "Could not load font code file from '%s': '%s'\n", buf, strerror(errno));
        goto FailExit;
    }

    snprintf(buf, sizeof buf, "%s"SUBDIR"%s", assets_dir, font_bmp);
    fv->font_matrix = UB_CreateFromFile(buf, log_fp);
    if (!fv->font_matrix)
    {
        fprintf(log_fp, "Could not load font matrix bitmap file from '%s'\n", buf);
        goto FailExit;
    }

    // Font matrix dimension
    fv->font_mat_w = fv->font_matrix->Width / fv->font_w;
    fv->font_mat_h = fv->font_matrix->Height / fv->font_h;

    snprintf(buf, sizeof buf, "%s"SUBDIR"%s", assets_dir, font_code_txt);
    fp_code = fopen(buf, "r");
    if (!fp_code)
    {
        fprintf(log_fp, "Could not load font code file from '%s': '%s'\n", buf, strerror(errno));
        goto FailExit;
    }
    fseek(fp_code, 0, SEEK_END);
    font_raw_code_size = ftell(fp_code); // Not expect the code file to be too large, no need for fgetpos()
    fseek(fp_code, 0, SEEK_SET);
    font_raw_code = malloc(font_raw_code_size + 1);
    if (!font_raw_code)
    {
        fprintf(log_fp, "Could not load font code file from '%s': '%s'\n", buf, strerror(errno));
        goto FailExit;
    }
    font_raw_code[font_raw_code_size] = '\0';
    if (!fread(font_raw_code, font_raw_code_size, 1, fp_code))
    {
        fprintf(log_fp, "Could not load font code file from '%s': '%s'\n", buf, strerror(errno));
        goto FailExit;
    }
    fclose(fp_code); fp_code = NULL;

    // Try parse the UTF-8 code file into 32-bit unicode string
    ch = font_raw_code;
    do
    {
        code = utf8tou32char(&ch);
        if (!code) break;

        // If none of the char code is above 127, it's not needed to tell the terminal to use UTF-8 encoding.
        if (code > 127) fv->need_chcp = 1;

        // If the count of the codes is not as described in the ini file, then do buffer expansion and continue to read all codes.
        if (i >= font_count_max)
        {
            size_t new_size = i * 3 / 2 + 1;
            uint32_t *new_ptr = realloc(fv->font_codes, new_size * sizeof new_ptr[0]);
            if (!new_ptr)
            {
                fprintf(log_fp, "Could not load font code file from '%s': '%s'\n", buf, strerror(errno));
                goto FailExit;
            }
            fv->font_codes = new_ptr;
            font_count_max = new_size;
        }
        fv->font_codes[i++] = code;
    } while (code);

    // Do trim excess to the buffer
    if (i != fv->font_code_count)
    {
        uint32_t *new_ptr = realloc(fv->font_codes, i * sizeof fv->font_codes[0]);
        if (new_ptr) fv->font_codes = new_ptr;

        fprintf(log_fp, "Actual count of codes read out from code file '%s' is '%zu' rather than '%zu'\n", buf, i, fv->font_code_count);
        fv->font_code_count = i;
    }

    free(font_raw_code);
    dict_delete(d_meta);
    return 1;
FailExit:
    if (fp_code) fclose(fp_code);
    free(font_raw_code);
    dict_delete(d_meta);
    return 0;
}

// Create a frame, create buffers, copy the source bitmap, preparing for the rendering
static fontvideo_frame_p frame_create(fontvideo_p fv, uint32_t w, uint32_t h, double timestamp, void *bitmap, int bmp_width, int bmp_height, size_t bmp_pitch)
{
    ptrdiff_t i;
    fontvideo_frame_p f = calloc(1, sizeof f[0]);
    if (!f) return f;
    fv;

    f->timestamp = timestamp;
    f->w = w;
    f->h = h;

    // Char code buffer
    f->data = calloc(w * h, sizeof f->data[0]); if (!f->data) goto FailExit;
    f->row = malloc(h * sizeof f->row[0]); if (!f->row) goto FailExit;

    // Char color buffer
    f->c_data = malloc((size_t)w * h); if (!f->c_data) goto FailExit;
    f->c_row = malloc(h * sizeof f->c_row[0]); if (!f->c_row) goto FailExit;

    // Create row pointers for faster access to the matrix slots
    for (i = 0; i < (ptrdiff_t)h; i++)
    {
        f->row[i] = &f->data[i * w];
        f->c_row[i] = &f->c_data[i * w];
    }

    // Source bitmap buffer
    f->raw_w = bmp_width;
    f->raw_h = bmp_height;
    f->raw_data = malloc(bmp_pitch * bmp_height); if (!f->raw_data) goto FailExit;
    f->raw_data_row = malloc(bmp_height * sizeof f->raw_data_row[0]); if (!f->raw_data_row) goto FailExit;

    // Copy the source bitmap whilst creating row pointers.
#pragma omp parallel for
    for (i = 0; i < (ptrdiff_t)bmp_height; i++)
    {
        f->raw_data_row[i] = (void *)&(((uint8_t *)f->raw_data)[i * bmp_pitch]);
        memcpy(f->raw_data_row[i], &((uint8_t*)bitmap)[i * bmp_pitch], bmp_pitch);
    }
    
    return f;
FailExit:
    frame_delete(f);
    return NULL;
}

// Render the frame from bitmap into font char codes. One of the magic happens here.
static void render_frame_from_rgbabitmap(fontvideo_p fv, fontvideo_frame_p f)
{
    int fy, fw, fh;
    UniformBitmap_p fm = fv->font_matrix;
    uint32_t font_pixel_count = fv->font_w * fv->font_h;

    if (f->rendered) return;

    fw = f->w;
    fh = f->h;

    f->rendering_start_time = rttimer_gettime(&fv->tmr);

    // OpenMP will automatically disable recursive multi-threading
#pragma omp parallel for
    for (fy = 0; fy < fh; fy++)
    {
        int fx, sx, sy;
        sy = fy * fv->font_h;
        for (fx = 0; fx < fw; fx++)
        {
            int fmx, fmy;
            int x, y;
            uint32_t avr_r = 0, avr_g = 0, avr_b = 0;
            size_t cur_code_index = 0;
            size_t best_code_index = 0;
            int best_fmx = 0, best_fmy = 0;
            float best_score = -9999999.0f;
            sx = fx * fv->font_w;

            // Replace the rightmost char to newline
            if (fx == fw - 1)
            {
                f->row[fy][fx] = '\n';
                continue;
            }

            // Iterate the font matrix
            for (fmy = 0; fmy < (int)fv->font_mat_h; fmy++)
            {
                int srcy = fmy * fv->font_h;
                for (fmx = 0; fmx < (int)fv->font_mat_w; fmx++)
                {
                    int srcx = fmx * fv->font_w;
                    float score = 0;
                    float font_normalize = 0;

                    // Compare each pixels and collect the scores.
                    for (y = 0; y < (int)fv->font_h; y++)
                    {
                        for (x = 0; x < (int)fv->font_w; x++)
                        {
                            union pixel
                            {
                                uint8_t u8[4];
                                uint32_t u32;
                            }   *src_pixel = (void *)&(f->raw_data_row[sy + y][sx + x]),
                                *font_pixel = (void *)&(fm->RowPointers[fm->Height - 1 - (srcy + y)][srcx + x]);
                            float src_lum = sqrtf((float)(
                                (uint32_t)src_pixel->u8[0] * src_pixel->u8[0] +
                                (uint32_t)src_pixel->u8[1] * src_pixel->u8[1] +
                                (uint32_t)src_pixel->u8[2] * src_pixel->u8[2])) / 441.672955930063709849498817084f;
                            float font_lum = font_pixel->u8[0] ? 1.0f : 0.0f;

                            // font_lum -= 0.5;
                            // src_lum = sqrtf(src_lum);
                            src_lum -= 0.5;
                            score += src_lum * font_lum;
                            font_normalize += font_lum * font_lum;
                        }
                    }

                    // Do vector normalize
                    if (font_normalize >= 0.000001f) score /= sqrtf(font_normalize);
                    if (score > best_score)
                    {
                        best_score = score;
                        best_code_index = cur_code_index;
                        best_fmx = fmx * fv->font_w;
                        best_fmy = fmy * fv->font_h;
                    }

                    cur_code_index++;
                    if (cur_code_index >= fv->font_code_count) break;
                }
            }

            // The best matching char code
            f->row[fy][fx] = fv->font_codes[best_code_index];

            // Get the average color of the char
            for (y = 0; y < (int)fv->font_h; y++)
            {
                for (x = 0; x < (int)fv->font_w; x++)
                {
                    union pixel
                    {
                        uint8_t u8[4];
                        uint32_t u32;
                    }   *src_pixel = (void *)&(f->raw_data_row[sy + y][sx + x]);

                    avr_b += src_pixel->u8[0];
                    avr_g += src_pixel->u8[1];
                    avr_r += src_pixel->u8[2];
                }
            }

            avr_b /= font_pixel_count;
            avr_g /= font_pixel_count;
            avr_r /= font_pixel_count;

            // Encode the color into 3-bit BGR
            f->c_row[fy][fx] =
                ((avr_b & 0x80) >> 5) |
                ((avr_g & 0x80) >> 6) |
                ((avr_r & 0x80) >> 7);
        }
    }

    f->rendering_time_consuming = rttimer_gettime(&fv->tmr) - f->rendering_start_time;

    // Finished rendering, update statistics
    atomic_exchange(&f->rendered, get_thread_id());
    lock_frame(fv);
    fv->rendered_frame_count++;
    fv->rendering_frame_count--;
    unlock_frame(fv);

#ifdef DEBUG_OUTPUT_SCREEN
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "gdi32.lib")
    if (1)
    {
        HDC hDC = GetDC(NULL);
        BITMAPINFOHEADER BMIF =
        {
            40, f->raw_w, -f->raw_h, 1, 32, 0
        };
        StretchDIBits(hDC, 0, 0, f->raw_w, f->raw_h,
            0, 0, f->raw_w, f->raw_h,
            f->raw_data,
            (BITMAPINFO *)&BMIF,
            DIB_RGB_COLORS,
            SRCCOPY);
        ReleaseDC(NULL, hDC);
    }
#endif

    // free(f->raw_data); f->raw_data = NULL;
    // free(f->raw_data_row); f->raw_data_row = NULL;
}

// Lock the link list of the frames, insert the frame into the link list, preserve the ordering.
// The link list of the frames is the cached frames to be rendered.
static void locked_add_frame_to_fv(fontvideo_p fv, fontvideo_frame_p f)
{
    double timestamp;
    fontvideo_frame_p iter;

    lock_frame(fv);

    timestamp = f->timestamp;
    f->index = fv->frame_count++;
    fv->precached_frame_count ++;

    if (!fv->frames)
    {
        fv->frames = f;
        fv->frame_last = f;
        goto Unlock;
    }

    if (timestamp >= fv->frame_last->timestamp)
    {
        fv->frame_last->next = f;
        fv->frame_last = f;
        goto Unlock;
    }

    if (timestamp < fv->frames->timestamp)
    {
        f->next = fv->frames;
        fv->frames = f;
        goto Unlock;
    }

    iter = fv->frames;
    while (timestamp >= iter->timestamp)
    {
        if (timestamp < iter->next->timestamp)
        {
            f->next = iter->next;
            iter->next = f;
            goto Unlock;
        }
        iter = iter->next;
    }
Unlock:
    unlock_frame(fv);
}

// Create the audio 
static fontvideo_audio_p audio_create(void *samples, int channel_count, size_t num_samples_per_channel, double timestamp)
{
    fontvideo_audio_p a = NULL;
    ptrdiff_t i;
    float *src = samples;
    
    if (channel_count < 1 || channel_count > 2) return a;
    
    a = calloc(1, sizeof a[0]);
    if (!a) return a;

    a->timestamp = timestamp;
    a->frames = num_samples_per_channel;
    a->buffer = malloc(num_samples_per_channel * channel_count * sizeof a->buffer[0]);
    if (!a->buffer) goto FailExit;
    switch (channel_count)
    {
    case 1:
        a->ptr_left = &a->buffer[0];
        a->ptr_right = &a->buffer[0];
        a->step_left = 1;
        a->step_right = 1;
#pragma omp parallel for
        for (i = 0; i < (ptrdiff_t)num_samples_per_channel; i++)
        {
            a->buffer[i] = src[i];
        }
        break;
    case 2:
        a->ptr_left = &a->buffer[0];
        a->ptr_right = &a->buffer[1];
        a->step_left = 2;
        a->step_right = 2;
#pragma omp parallel for
        for (i = 0; i < (ptrdiff_t)num_samples_per_channel; i++)
        {
            a->ptr_left[i * a->step_left] = src[i * 2 + 0];
            a->ptr_right[i * a->step_right] = src[i * 2 + 1];
        }
        break;
    }

    return a;
FailExit:
    audio_delete(a);
    return NULL;
}

static void locked_add_audio_to_fv(fontvideo_p fv, fontvideo_audio_p a)
{
    double timestamp;
    fontvideo_audio_p iter;
    
    timestamp = a->timestamp;

    lock_audio(fv);

    if (!fv->audios)
    {
        fv->audios = a;
        fv->audio_last = a;
        goto Unlock;
    }

    if (timestamp >= fv->audio_last->timestamp)
    {
        fv->audio_last->next = a;
        fv->audio_last = a;
        goto Unlock;
    }

    if (timestamp < fv->audios->timestamp)
    {
        a->next = fv->audios;
        fv->audios = a;
        goto Unlock;
    }

    iter = fv->audios;
    while (timestamp >= iter->timestamp)
    {
        if (timestamp < iter->next->timestamp)
        {
            a->next = iter->next;
            iter->next = a;
            goto Unlock;
        }
        iter = iter->next;
    }
Unlock:
    unlock_audio(fv);
}

static void fv_on_get_video(avdec_p av, void *bitmap, int width, int height, size_t pitch, double timestamp, enum AVPixelFormat pixel_format)
{
    fontvideo_p fv = av->userdata;
    fontvideo_frame_p f = frame_create(fv, fv->output_w, fv->output_h, timestamp, bitmap, width, height, pitch);
    if (!f)
    {
        return;
    }
    locked_add_frame_to_fv(fv, f);

    pixel_format;
    // printf("Get video data: %p, %d, %d, %zu, %lf, %d\n", bitmap, width, height, pitch, timestamp, pixel_format);
}

static void fv_on_get_audio(avdec_p av, void **samples_of_channel, int channel_count, size_t num_samples_per_channel, double timestamp, enum AVSampleFormat sample_fmt)
{
    fontvideo_p fv = av->userdata;
    fontvideo_audio_p a = NULL;

    if (!fv->do_audio_output) return;
    
    a = audio_create(samples_of_channel[0], channel_count, num_samples_per_channel, timestamp);
    if (!a)
    {
        return;
    }
    locked_add_audio_to_fv(fv, a);

    sample_fmt;
    // printf("Get audio data: %p, %d, %zu, %lf, %d\n", samples_of_channel, channel_count, num_samples_per_channel, timestamp, sample_fmt);
}

static fontvideo_frame_p get_frame_and_render(fontvideo_p fv)
{
    fontvideo_frame_p f = NULL, prev = NULL;
    if (!fv->frames) return NULL;
    lock_frame(fv);
    if (fv->precached_frame_count <= fv->rendered_frame_count + fv->rendering_frame_count)
    {
        unlock_frame(fv);
        return NULL;
    }
    f = fv->frames;
    while (f)
    {
        atomic_int expected = 0;
        if (f->rendered)
        {
            prev = f;
            f = f->next;
            continue;
        }
        if (atomic_compare_exchange_strong(&f->rendering, &expected, get_thread_id()))
        {
            // If the frame isn't being rendered, first detect if it's too late to render it, then do frame skipping.
            if (fv->real_time_play && rttimer_gettime(&fv->tmr) > f->timestamp + fv->precache_seconds - 1.0f)
            {
                // Too late to render the frame, skip it.
                fontvideo_frame_p next = f->next;
                if (!prev)
                {
                    assert(f == fv->frames);
                    fv->frames = next;
                    if (!next)
                    {
                        fv->frame_last = NULL;
                        unlock_frame(fv);
                        return NULL;
                    }
                    else
                    {
                        frame_delete(f);
                        f = fv->frames = next;
                        continue;
                    }
                }
                else
                {
                    prev->next = next;
                    frame_delete(f);
                    f = next;
                    continue;
                }
            }
            else
            {
                fv->rendering_frame_count++;

                if (fv->verbose_threading)
                {
                    fprintf(stderr, "Got frame to render. (%d)\n", get_thread_id());
                }
                unlock_frame(fv);
                render_frame_from_rgbabitmap(fv, f);
                return f;
            }
        }
        else
        {
            // This one is occupied and perform rendering.
            prev = f;
            f = f->next;
            continue;
        }
    }
    unlock_frame(fv);
    return f;
}

static int output_rendered_video(fontvideo_p fv, double timestamp)
{
    char* utf8buf = fv->utf8buf;
    fontvideo_frame_p cur = NULL, next = NULL, prev = NULL;
    int cur_color = 0;

    if (!fv->frames) return 0;
    if (!fv->rendered_frame_count) return 0;

    if (atomic_exchange(&fv->doing_output, 1)) return 0;

    lock_frame(fv);
    cur = fv->frames;
    if (!cur)
    {
        unlock_frame(fv);
        goto FailExit;
    }
    // If output is too slow, skip frames.
    if (fv->real_time_play && timestamp >= 0)
    {
        while(1)
        {
            next = cur->next;
            if (!next) break;
            if (timestamp >= cur->timestamp)
            {
                // Timestamp arrived
                if (timestamp < next->timestamp) break;

                // If the next frame also need to output right now, skip the current frame
                if (cur->rendered)
                {
                    if (fv->frames == cur)
                    {
                        fv->frames = next;
                    }
                    else
                    {
                        if (!prev) prev = fv->frames;
                        prev->next = next;
                    }
                    frame_delete(cur);
                    cur = next;
                    fv->precached_frame_count--;
                    fv->rendered_frame_count--;
                }
                else
                {
                    atomic_int expected = 0;

                    // Acquire it to prevent it from being rendered
                    if (atomic_compare_exchange_strong(&cur->rendering, &expected, get_thread_id()))
                    {
                        if (fv->frames == cur)
                        {
                            fv->frames = next;
                        }
                        else
                        {
                            if (!prev) prev = fv->frames;
                            prev->next = next;
                        }
                        frame_delete(cur);
                        cur = next;
                        fv->precached_frame_count--;
                        fv->rendered_frame_count--;
                    }
                    else
                    {
                        // Sadly it's acquired by a rendering thread, skip it.
                        prev = cur;
                        cur = next;
                    }
                }
            }
            else
            {
                // Timestamp not arrived to this frame
                break;
            }
        }
    }
    unlock_frame(fv);
    if (timestamp < 0 || timestamp >= cur->timestamp || !fv->real_time_play)
    {
        next = cur->next;
        int x, y;
#if !defined(_DEBUG)
        if (1)
        {
            set_cursor_xy(0, 0);
        }
#endif
        if (!cur->rendered) goto FailExit;
        if (fv->do_colored_output)
        {
            char *u8chr = utf8buf;
            cur_color = cur->c_row[0][0];
            memset(utf8buf, 0, fv->utf8buf_size);
            set_console_color(fv, cur_color);
            u8chr = strchr(u8chr, 0);
            for (y = 0; y < (int)cur->h; y++)
            {
                for (x = 0; x < (int)cur->w; x++)
                {
                    int new_color = cur->c_row[y][x];
                    if (new_color != cur_color && cur->row[y][x] != ' ')
                    {
                        cur_color = new_color;
                        *u8chr = '\0';
                        set_console_color(fv, new_color);
                        u8chr = strchr(u8chr, 0);
                    }
                    u32toutf8(&u8chr, cur->row[y][x]);
                }
            }
            *u8chr = '\0';
            fputs(utf8buf, fv->graphics_out_fp);
        }
        else
        {
            for (y = 0; y < (int)cur->h; y++)
            {
                char *u8chr = utf8buf;
                for (x = 0; x < (int)cur->w; x++)
                {
                    u32toutf8(&u8chr, cur->row[y][x]);
                }
                *u8chr = '\0';
                fputs(utf8buf, fv->graphics_out_fp);
            }
        }

        fprintf(fv->graphics_out_fp, "%u\n", cur->index);
        lock_frame(fv);
        fv->frames = next;
        if (!next)
        {
            fv->frame_last = NULL;
        }
        frame_delete(cur);
        fv->precached_frame_count--;
        fv->rendered_frame_count--;
        unlock_frame(fv);
    }
    atomic_exchange(&fv->doing_output, 0);
    return 1;
FailExit:
    atomic_exchange(&fv->doing_output, 0);
    return 0;
}

static int do_decode(fontvideo_p fv, int keeprun)
{
    double target_timestamp = rttimer_gettime(&fv->tmr) + fv->precache_seconds;
    int ret = 0;
    if (!fv->tailed)
    {
        if (atomic_exchange(&fv->doing_decoding, 1)) return 0;

        lock_frame(fv);
        while (!fv->tailed && (
            !fv->frame_last || (fv->frame_last && fv->frame_last->timestamp < target_timestamp) ||
            (fv->do_audio_output &&
            !fv->audio_last || (fv->audio_last && fv->audio_last->timestamp < target_timestamp))))
        {
            unlock_frame(fv);
            if (fv->verbose_threading)
            {
                fprintf(stderr, "Decoding frames. (%d)\n", get_thread_id());
            }
            ret = 1;
            fv->tailed = !avdec_decode(fv->av, fv_on_get_video, fv->do_audio_output ? fv_on_get_audio : NULL);
            if (keeprun)
            {
                target_timestamp = rttimer_gettime(&fv->tmr) + fv->precache_seconds;
            }
            lock_frame(fv);
        }
        unlock_frame(fv);
        if (fv->tailed)
        {
            if (fv->verbose_threading)
            {
                fprintf(stderr, "All frames decoded. (%d)\n", get_thread_id());
            }
        }
        atomic_exchange(&fv->doing_decoding, 0);
        return ret;
    }
    return ret;
}

fontvideo_p fv_create(char *input_file, FILE *log_fp, int do_verbose_log, FILE *graphics_out_fp, uint32_t x_resolution, uint32_t y_resolution, double precache_seconds, int do_audio_output, double start_timestamp)
{
    fontvideo_p fv = NULL;
    avdec_video_format_t vf = {0};
    avdec_audio_format_t af = {0};
    int i;
    int tc = omp_get_max_threads();

    fv = calloc(1, sizeof fv[0]);
    if (!fv) return fv;
    fv->log_fp = log_fp;
    fv->verbose = do_verbose_log;
#if defined(_DEBUG)
    fv->verbose_threading = 1;
#else
    fv->verbose_threading = 0;
#endif
    fv->graphics_out_fp = graphics_out_fp;
    fv->do_audio_output = do_audio_output;

    if (log_fp == stdout || log_fp == stderr || graphics_out_fp == stdout || graphics_out_fp == stderr)
    {
        fv->do_colored_output = 1;
        fv->real_time_play = 1;
    }

    fv->av = avdec_open(input_file, stderr);
    if (!fv->av) goto FailExit;
    fv->av->userdata = fv;

    fv->output_w = x_resolution;
    fv->output_h = y_resolution;
    fv->precache_seconds = precache_seconds;

    fv->utf8buf_size = (size_t)fv->output_w * fv->output_h * 32 + 1;
    fv->utf8buf = malloc(fv->utf8buf_size);
    if (!fv->utf8buf) goto FailExit;
    if (!load_font(fv, "assets", "meta.ini")) goto FailExit;
    if (fv->need_chcp || fv->real_time_play)
    {
        init_console(fv);
    }

    vf.width = x_resolution * fv->font_w;
    vf.height = y_resolution * fv->font_h;
    vf.pixel_format = AV_PIX_FMT_BGRA;
    avdec_set_decoded_video_format(fv->av, &vf);

    if (do_audio_output)
    {
        af.channel_layout = av_get_default_channel_layout(2);
        af.sample_rate = fv->av->decoded_af.sample_rate;
        af.sample_fmt = AV_SAMPLE_FMT_FLT;
        af.bit_rate = (int64_t)af.sample_rate * 2 * sizeof(float);
        avdec_set_decoded_audio_format(fv->av, &af);
    }

    if (start_timestamp < 0) start_timestamp = 0;
    else avdec_seek(fv->av, start_timestamp);

    rttimer_init(&fv->tmr, 1);
    rttimer_settime(&fv->tmr, start_timestamp);

    if (fv->real_time_play)
    {
        do_decode(fv, 0);
        while (fv->rendered_frame_count + fv->rendering_frame_count < fv->precached_frame_count)
        {
#pragma omp parallel for
            for (i = 0; i < tc; i++)
            {
                get_frame_and_render(fv);
            }
        }
        while (fv->rendering_frame_count);

        rttimer_start(&fv->tmr);

        if (do_audio_output)
        {
            fv->sio = siowrap_create(log_fp, SoundIoFormatFloat32NE, af.sample_rate, fv_on_write_sample);
            if (!fv->sio) goto FailExit;
            fv->sio->userdata = fv;
        }
    }

    return fv;
FailExit:
    fv_destroy(fv);
    return NULL;
}

int fv_show(fontvideo_p fv)
{
    int i;
    int tc = omp_get_max_threads();

    if (!fv) return 0;

    fv->real_time_play = 1;

#pragma omp parallel for // ordered
    for(i = 0; i < tc; i++)
    {
        if (i == 0) for (;;)
        {
            if (!do_decode(fv, 1))
                yield();

            if (fv->tailed) break;
        }
        else if (i == 1) for (;;)
        {
            if (fv->rendered_frame_count)
            {
                if (!output_rendered_video(fv, rttimer_gettime(&fv->tmr)));
                yield();
            }

            if (fv->tailed && !fv->precached_frame_count) break;
        }
        else for (;;)
        {
            fontvideo_frame_p f = get_frame_and_render(fv);
            if (!f) yield();

            if (fv->tailed && fv->rendered_frame_count >= fv->precached_frame_count) break;
        }

    }
    while (fv->audios || fv->audio_last) yield();
    return 1;
}

int fv_render(fontvideo_p fv)
{
    if (!fv) return 0;
    for (;;)
    {
        if (!fv->tailed)
        {
            avdec_decode(fv->av, fv_on_get_video, fv->do_audio_output ? fv_on_get_audio : NULL);
        }
        get_frame_and_render(fv);
        if (fv->tailed && !output_rendered_video(fv, -1))
            break;
    }
    return 1;
}

void fv_destroy(fontvideo_p fv)
{
    if (!fv) return;

    siowrap_destroy(fv->sio);
    free(fv->font_codes);
    UB_Free(&fv->font_matrix);
    avdec_close(&fv->av);

    while (fv->frames)
    {
        fontvideo_frame_p next = fv->frames->next;
        frame_delete(fv->frames);
        fv->frames = next;
    }

    while (fv->audios)
    {
        fontvideo_audio_p next = fv->audios->next;
        audio_delete(fv->audios);
        fv->audios = next;
    }

    free(fv->utf8buf);
    free(fv);
}
