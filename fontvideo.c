#include"fontvideo.h"
#include"bunchalloc.h"

#ifdef _WIN32
#include<Windows.h>
#endif

#include<stdlib.h>
#include<locale.h>
#include<assert.h>
#include<C_dict/dictcfg.h>
#include<omp.h>
#include<threads.h>

#include<math.h>

#include"utf.h"

#ifdef _DEBUG
#   define DEBUG_OUTPUT_TO_SCREEN 0
#   define DEBUG_BGRX(r, g, b) (0xff000000 | (uint8_t)(b) | ((uint32_t)((uint8_t)(g)) << 8) | ((uint32_t)((uint8_t)(r)) << 16))
#endif

static const int discard_frame_num_threshold = 10;
static const double sqrt_3 = 1.7320508075688772935274463415059;

static uint32_t get_thread_id()
{
    return (uint32_t)thrd_current();
}

#ifdef _WIN32
#define SUBDIR "\\"

#if defined(_DEBUG) && DEBUG_OUTPUT_TO_SCREEN
void DebugRawBitmap(int dst_x, int dst_y, void *bitmap, int bmp_w, int bmp_h)
{
    HDC hDC = GetDC(NULL);
    BITMAPINFOHEADER BMIF =
    {
        40, bmp_w, -bmp_h, 1, 32, 0
    };
    StretchDIBits(hDC, dst_x, dst_y, bmp_w, bmp_h,
        0, 0, bmp_w, bmp_h,
        bitmap,
        (BITMAPINFO *)&BMIF,
        DIB_RGB_COLORS,
        SRCCOPY);
    ReleaseDC(NULL, hDC);
}
#endif

static void PrintLastError(FILE *fp)
{
    wchar_t *wbuf = NULL;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, (LPWSTR)&wbuf, 0, NULL);
    if (wbuf)
    {
        fputws(wbuf, fp);
        LocalFree(wbuf);
    }
}

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
        if (!(ConMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            if (!SetConsoleMode(hConsole, ConMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                fv->do_old_console_output = 1;
            }
        }

        if (fv->font_face && strlen(fv->font_face))
        {
            CONSOLE_FONT_INFOEX cfi = {0};
            char *src_face = fv->font_face;
            uint16_t *dst_face = &cfi.FaceName[0];
            uint32_t uchar = 0;

            cfi.cbSize = sizeof cfi;
            GetCurrentConsoleFontEx(hConsole, FALSE, &cfi);

            cfi.nFont = 0;
            cfi.FontFamily = FF_DONTCARE;
            cfi.FontWeight = FW_NORMAL;
            do
            {
                uchar = utf8tou32char(&src_face);
                u32toutf16(&dst_face, uchar);
            } while (uchar);
            if (!SetCurrentConsoleFontEx(hConsole, FALSE, &cfi))
            {
                fprintf(fv->log_fp, "SetCurrentConsoleFontEx() failed: ");
                PrintLastError(fv->log_fp);
                fprintf(fv->log_fp, "\n");
            }
        }
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
    case 000: strcat(fv->utf8buf, "\x1b[0;0m");  break;
    case 001: strcat(fv->utf8buf, "\x1b[0;31m"); break;
    case 002: strcat(fv->utf8buf, "\x1b[0;32m"); break;
    case 003: strcat(fv->utf8buf, "\x1b[0;33m"); break;
    case 004: strcat(fv->utf8buf, "\x1b[0;34m"); break;
    case 005: strcat(fv->utf8buf, "\x1b[0;35m"); break;
    case 006: strcat(fv->utf8buf, "\x1b[0;36m"); break;
    case 007: strcat(fv->utf8buf, "\x1b[0;37m"); break;
    case 010: strcat(fv->utf8buf, "\x1b[1;0m"); break;
    case 011: strcat(fv->utf8buf, "\x1b[1;31m"); break;
    case 012: strcat(fv->utf8buf, "\x1b[1;32m"); break;
    case 013: strcat(fv->utf8buf, "\x1b[1;33m"); break;
    case 014: strcat(fv->utf8buf, "\x1b[1;34m"); break;
    case 015: strcat(fv->utf8buf, "\x1b[1;35m"); break;
    case 016: strcat(fv->utf8buf, "\x1b[1;36m"); break;
    case 017: strcat(fv->utf8buf, "\x1b[1;37m"); break;
    }
}

static void cpu_relax()
{
    _mm_pause();
}

static int yield()
{
    return SwitchToThread();
}

static void relax_sleep(uint64_t milliseconds)
{
    Sleep((DWORD)milliseconds);
}

#else // For non-Win32 appliction, assume it's linux/unix and runs in terminal

#include <sched.h>

#ifndef __GNUC__
#define __asm__ asm
#endif

#define SUBDIR "/"

static void init_console(fontvideo_p fv)
{
    if (fv->need_chcp)
    {
        setlocale(LC_ALL, "");
        fv->output_utf8 = 1;
    }
}

static void set_cursor_xy(int x, int y)
{
    printf("\033[%d;%dH", y + 1, x + 1);
}

static void set_console_color(fontvideo_p fv, int clr)
{
    switch (clr)
    {
    case 000: strcat(fv->utf8buf, "\x1b[0;0m");  break;
    case 001: strcat(fv->utf8buf, "\x1b[0;31m"); break;
    case 002: strcat(fv->utf8buf, "\x1b[0;32m"); break;
    case 003: strcat(fv->utf8buf, "\x1b[0;33m"); break;
    case 004: strcat(fv->utf8buf, "\x1b[0;34m"); break;
    case 005: strcat(fv->utf8buf, "\x1b[0;35m"); break;
    case 006: strcat(fv->utf8buf, "\x1b[0;36m"); break;
    case 007: strcat(fv->utf8buf, "\x1b[0;37m"); break;
    case 010: strcat(fv->utf8buf, "\x1b[1;0m"); break;
    case 011: strcat(fv->utf8buf, "\x1b[1;31m"); break;
    case 012: strcat(fv->utf8buf, "\x1b[1;32m"); break;
    case 013: strcat(fv->utf8buf, "\x1b[1;33m"); break;
    case 014: strcat(fv->utf8buf, "\x1b[1;34m"); break;
    case 015: strcat(fv->utf8buf, "\x1b[1;35m"); break;
    case 016: strcat(fv->utf8buf, "\x1b[1;36m"); break;
    case 017: strcat(fv->utf8buf, "\x1b[1;37m"); break;
    }
}

static void cpu_relax()
{
    __asm__ volatile ("pause" ::: "memory");
}

static int yield()
{
    return sched_yield();
}

static void relax_sleep(uint64_t milliseconds)
{
    struct timespec req, rem;
    req.tv_sec = milliseconds / 1000;
    req.tv_nsec = (milliseconds % 1000) * 1000000;
    while(nanosleep(&req, &rem))
    {
        switch (errno)
        {
        default:
        case EFAULT: return;
        case EINTR: break;
        }
    }
}
#endif

static void backoff(int *iter_counter)
{
    const int max_iter = 16;
    const int max_yield = 1;
    const int max_sleep_ms = 20;
    if (iter_counter[0] < 0) iter_counter[0] = 0;
    if (iter_counter[0] < max_iter)
    {
        iter_counter[0] ++;
        cpu_relax();
        return;
    }
    else if (iter_counter[0] - max_iter < max_yield)
    {
        iter_counter[0] ++;
        if (!yield()) relax_sleep(1);
    }
    else
    {
        int random;
        int shift = iter_counter[0] - max_iter - max_yield;
        int sleep_ms;
        if (iter_counter[0] < 0x7fffffff) iter_counter[0] ++;
        sleep_ms = 1 << shift;
        if (sleep_ms > max_sleep_ms || sleep_ms < 0) sleep_ms = max_sleep_ms;
#pragma omp critical
        random = rand();
        relax_sleep(random % sleep_ms);
    }
}

static union colorBGR
{
    uint8_t f[4];
    struct desc
    {
        uint8_t b, g, r, x;
    }rgb;
    uint32_t packed;
}console_palette[16] =
{
    {{0, 0, 0}},
    {{31, 15, 197}},
    {{14, 161, 19}},
    {{0, 156, 193}},
    {{218, 55, 0}},
    {{152, 23, 136}},
    {{221, 150, 58}},
    {{204, 204, 204}},
    {{118, 118, 118}},
    {{86, 72, 231}},
    {{12, 198, 22}},
    {{165, 241, 249}},
    {{255, 120, 59}},
    {{158, 0, 180}},
    {{214, 214, 97}},
    {{255, 255, 255}}
};

#define _PADDED(value, unit) (((value) - 1) / (unit) + 1) * (unit)
static ptrdiff_t make_padded(ptrdiff_t value, int unit)
{
    return _PADDED(value, unit);
}

// For locking the frames link list of `fv->frames` and `fv->frame_last`, not for locking every individual frames
static void lock_frame_linklist(fontvideo_p fv)
{
    uint32_t readout = 0;
    uint32_t cur_id = get_thread_id();
    int bo = 0;
    while (
        // First, do a non-atomic test for a quick peek.
        ((readout = fv->frame_linklist_lock) != 0) ||

        // Then, perform the actual atomic operation for acquiring the lock.
        ((readout = atomic_exchange(&fv->frame_linklist_lock, cur_id)) != 0))
    {
        if (fv->verbose_threading)
        {
            fprintf(fv->log_fp, "Frame link list locked by %d. (%d)\n", readout, cur_id);
        }
        backoff(&bo);
    }
}

static void unlock_frame_linklist(fontvideo_p fv)
{
    atomic_exchange(&fv->frame_linklist_lock, 0);
}

#ifndef FONTVIDEO_NO_SOUND
// Same mechanism for locking audio link list.
static void lock_audio_linklist(fontvideo_p fv)
{
    uint32_t readout;
    uint32_t cur_id = get_thread_id();
    int bo = 0;
    while ((readout = atomic_exchange(&fv->audio_linklist_lock, cur_id)) != 0)
    {
        if (fv->verbose_threading)
        {
            fprintf(fv->log_fp, "Audio link list locked by %d. (%d)\n", readout, cur_id);
        }
        backoff(&bo);
    }
}

static void unlock_audio_linklist(fontvideo_p fv)
{
    atomic_exchange(&fv->audio_linklist_lock, 0);
}
#endif

static void audio_delete(fontvideo_audio_p* pa)
{
    fontvideo_audio_p a;
    if (!pa) return;
    a = *pa;
    if (!a) return;
    *pa = NULL;

    // free(a->buffer);
    bunchfree(a);
}

// Move a pointer by bytes
static void *move_ptr(void *ptr, ptrdiff_t step)
{
    return (uint8_t *)ptr + step;
}

static void fv_on_get_video(avdec_p av, void* bitmap, int width, int height, size_t pitch, double timestamp);
static void fv_on_get_audio(avdec_p av, void** samples_of_channel, int channel_count, size_t num_samples_per_channel, double timestamp);

// Create audio piece and copy the waveform
static fontvideo_audio_p audio_create(int sample_rate, void *samples, int channel_count, size_t num_samples_per_channel, double timestamp);

#ifndef FONTVIDEO_NO_SOUND
// Callback function for siowrap output sound to libsoundio
static size_t fv_on_write_sample(siowrap_p s, int sample_rate, int channel_count, void **pointers_per_channel, size_t *sample_pointer_steps, size_t samples_to_write_per_channel)
{
    ptrdiff_t i;
    fontvideo_p fv = s->userdata;

#ifdef _MSC_VER
    sample_rate;
#endif

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

    // Initialize stereo pointers
    if (channel_count == 2)
    {
        dptr_l = dptr;
        dptr_r = pointers_per_channel[1];
        dstep_l = dstep;
        dstep_r = sample_pointer_steps[1];
    }

    // Because the next operations will change the audio link list, so lock it up.
    lock_audio_linklist(fv);
    while (total < samples_to_write_per_channel)
    {
        if (fv->audios)
        {
            // Next audio piece in the link list
            fontvideo_audio_p next = fv->audios->next;

            // Current time
            double current_time = rttimer_gettime(&fv->tmr);

            // Source pointer
            float* sptr = fv->audios->buffer; // Mono
            float* sptr_l = fv->audios->ptr_left; // Stereo left
            float* sptr_r = fv->audios->ptr_right; // Stereo right

            // Source step length, by **SAMPLES**
            size_t sstep = 1; // Mono
            size_t sstep_l = fv->audios->step_left; // Stereo left
            size_t sstep_r = fv->audios->step_right; // Stereo right
            ptrdiff_t writable = fv->audios->frames; // Source frames
            size_t wrote = 0; // Total frames wrote

            // Do audio piece skip
            if (!fv->no_frameskip && next)
            {
                if (current_time > next->timestamp)
                {
                    audio_delete(&fv->audios);
                    fv->audios = next;
                    continue;
                }
            }

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
                audio_delete(&fv->audios);
                fv->audios = next;
            }
            else // If the current audio piece is not all written, reserve the rest of the frames for the next callback.
            {
                size_t rest = fv->audios->frames - wrote;
                fontvideo_audio_p replace;

                // Reserve the rest of the frames
                replace = audio_create(sample_rate, &fv->audios->buffer[wrote * channel_count], channel_count, rest, fv->audios->timestamp);

                // Insert into the link list
                replace->next = next;
                next = fv->audios;

                if (fv->audio_last == next) fv->audio_last = replace;
                fv->audios = replace;

                // Delete the current piece
                audio_delete(&next);

                // Done writing audio
                unlock_audio_linklist(fv);
                break;
            }
        }
        else
        {
            fv->audio_last = NULL;

            // No need to keep the lock, release ASAP
            unlock_audio_linklist(fv);

            // Check if the buffer isn't all filled, then fill if not.
            if (total < samples_to_write_per_channel)
            {
                if (fv->tailed)
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
                else
                {
                    int bo = 0;
                    while (atomic_exchange(&fv->doing_decoding, 1)) backoff(&bo);
                    fv->tailed = !avdec_decode(fv->av, fv_on_get_video, fv_on_get_audio, avt_for_audio);
                    atomic_store(&fv->doing_decoding, 0);
                }
            }
        }
    }

    return total;
}
#endif

static int sort_glyph_codes_by_brightness(fontvideo_p fv);

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
    ptrdiff_t si;
    char *font_raw_code = NULL;
    char *ch;
    uint32_t code = 0;
    size_t font_raw_code_size = 0;
    size_t font_count_max = 0;
    size_t expected_font_code_count = 0;
    size_t glyph_pixel_count;

    // snprintf(buf, sizeof buf, "%s"SUBDIR"%s", assets_dir, meta_file);
    d_meta = dictcfg_load(meta_file, log_fp); // Parse ini file
    if (!d_meta)
    {
        fprintf(log_fp, "Could not load meta-file: '%s'.\n", meta_file);
        goto FailExit;
    }

    d_font = dictcfg_section(d_meta, "[font]"); // Extract section
    if (!d_font)
    {
        fprintf(log_fp, "Meta-file '%s' don't have `[font]` section.\n", meta_file);
        goto FailExit;
    }

    font_bmp = dictcfg_getstr(d_font, "font_bmp", "font.bmp");
    font_code_txt = dictcfg_getstr(d_font, "font_code", "gb2312_code.txt");
    fv->glyph_width = dictcfg_getint(d_font, "font_width", 12);
    fv->glyph_height = dictcfg_getint(d_font, "font_height", 12);
    fv->font_face = dictcfg_getstr(d_font, "font_face", NULL);
    fv->num_glyph_codes = font_count_max = dictcfg_getint(d_font, "glyph_count", 127);
    fv->brightness_weight = (float)dictcfg_getfloat(d_font, "brightness_weight", 1.0 - (1.0 / 64.0));
    fv->glyph_codes = malloc(font_count_max * sizeof fv->glyph_codes[0]);
    if (!fv->glyph_codes)
    {
        fprintf(log_fp, "Could not allocate font codes: '%s'.\n", strerror(errno));
        goto FailExit;
    }
    glyph_pixel_count = (size_t)fv->glyph_width * fv->glyph_height;

    snprintf(buf, sizeof buf, "%s"SUBDIR"%s", assets_dir, font_bmp);
    fv->glyph_matrix = UB_CreateFromFile((const char*)buf, log_fp);
    if (!fv->glyph_matrix)
    {
        fprintf(log_fp, "Could not load glyph matrix bitmap file from '%s'.\n", buf);
        goto FailExit;
    }

    // Font matrix dimension
    fv->glyph_matrix_cols = fv->glyph_matrix->Width / fv->glyph_width;
    fv->glyph_matrix_rows = fv->glyph_matrix->Height / fv->glyph_height;
    expected_font_code_count = (size_t)fv->glyph_matrix_cols * fv->glyph_matrix_rows;

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
            uint32_t *new_ptr = realloc(fv->glyph_codes, new_size * sizeof new_ptr[0]);
            if (!new_ptr)
            {
                fprintf(log_fp, "Could not load font code file from '%s': '%s'\n", buf, strerror(errno));
                goto FailExit;
            }
            fv->glyph_codes = new_ptr;
            font_count_max = new_size;
        }
        fv->glyph_codes[i++] = code;
    } while (code);

    // Do trim excess to the buffer
    if (i != fv->num_glyph_codes)
    {
        uint32_t *new_ptr = realloc(fv->glyph_codes, i * sizeof fv->glyph_codes[0]);
        if (new_ptr) fv->glyph_codes = new_ptr;

        fprintf(log_fp, "Actual count of codes read out from code file '%s' is '%zu' rather than '%zu'\n", buf, i, fv->num_glyph_codes);
        fv->num_glyph_codes = i;
    }

    // Detect if the glyph matrix size doesn't match
    if (fv->num_glyph_codes > expected_font_code_count)
    {
        fprintf(log_fp, "Font meta-file issue: font bitmap file size of %ux%u"
            " for glyph size %ux%u"
            " should have a maximum of %zu"
            " glyphs, but the given code count %zu"
            " exceeded the glyph count. Please correct the meta-file and run this program again.\n",
            fv->glyph_matrix->Width, fv->glyph_matrix->Height, fv->glyph_width, fv->glyph_height, expected_font_code_count, fv->num_glyph_codes);
        goto FailExit;
    }

    if (fv->brightness_weight < 0) fv->brightness_weight = 0;
    else if (fv->brightness_weight > 1) fv->brightness_weight = 1;
    fv->candidate_glyph_window_size = (int)(fv->num_glyph_codes * fabs(1.0f - fv->brightness_weight));
    if (fv->candidate_glyph_window_size < 1) fv->candidate_glyph_window_size = 1;
    else if (fv->candidate_glyph_window_size > fv->num_glyph_codes) fv->candidate_glyph_window_size = (int)fv->num_glyph_codes;

    fv->glyph_vertical_array = calloc(fv->num_glyph_codes * glyph_pixel_count, sizeof fv->glyph_vertical_array[0]);
    fv->glyph_brightness = calloc(fv->num_glyph_codes, sizeof fv->glyph_brightness[0]);
    if (!fv->glyph_vertical_array || !fv->glyph_brightness)
    {
        fprintf(log_fp, "Could not calculate font glyph lum values: %s\n", strerror(errno));
        goto FailExit;
    }

    fv->font_is_blackwhite = 1;
#pragma omp parallel for
    for (si = 0; si < (ptrdiff_t)fv->num_glyph_codes; si++)
    {
        UniformBitmap_p gm = fv->glyph_matrix;
        uint32_t x, y;
        int srcx = (int)((si % fv->glyph_matrix_cols) * fv->glyph_width);
        int srcy = (int)((si / fv->glyph_matrix_cols) * fv->glyph_height);
        float *lum_of_glyph = &fv->glyph_vertical_array[si * glyph_pixel_count];
        float glyph_brightness = 0;
        for (y = 0; y < fv->glyph_height; y++)
        {
            uint32_t *row = gm->RowPointers[srcy + y];
            float *lum_row = &lum_of_glyph[y * fv->glyph_width];
            for (x = 0; x < fv->glyph_width; x++)
            {
                union pixel
                {
                    uint8_t u8[4];
                    uint32_t u32;
                    float f32;
                }   *gp = (void *)&(row[srcx + x]);
                float gp_r, gp_g, gp_b;
                float gp_lum;
                gp_b = (float)gp->u8[0] / 255.0f;
                gp_g = (float)gp->u8[1] / 255.0f;
                gp_r = (float)gp->u8[2] / 255.0f;
                gp_lum = (float)(sqrt(gp_r * gp_r + gp_g * gp_g + gp_b * gp_b) / sqrt_3);
                if (fv->font_is_blackwhite != 0 && gp->u32 != 0xff000000 && gp->u32 != 0xffffffff)
                {
                    fv->font_is_blackwhite = 0;
                }
                if (fv->white_background) gp_lum = 1.0f - gp_lum;
                lum_row[x] = gp_lum;
                glyph_brightness += gp_lum * gp_lum;
            }
        }
        glyph_brightness /= glyph_pixel_count;
        fv->glyph_brightness[si] = glyph_brightness;
    }

    free(font_raw_code);
    dict_delete(d_meta);
    return sort_glyph_codes_by_brightness(fv);
FailExit:
    if (fp_code) fclose(fp_code);
    free(font_raw_code);
    dict_delete(d_meta);
    return 0;
}

static void normalize_glyph(float *out, const float *in, size_t glyph_pixel_count);

typedef struct code_lum_struct
{
    size_t index;
    float lum;
    uint32_t code;
}code_lum_t, * code_lum_p;

static int cl_compare(const void *p1, const void *p2)
{
    const code_lum_t * cl1 = p1;
    const code_lum_t * cl2 = p2;
    if (cl1->lum < cl2->lum) return -1;
    if (cl1->lum > cl2->lum) return 1;
    if (cl1->code < cl2->code) return -1;
    if (cl1->code > cl2->code) return 1;
    // if (cl1->index > cl2->index) return -1;
    // if (cl1->index < cl2->index) return 1;
    return 0;
}

int sort_glyph_codes_by_brightness(fontvideo_p fv)
{
    ptrdiff_t i;
    code_lum_p cl_array = NULL;
    float* glyph_array = NULL;
    UniformBitmap_p sorted_gm = NULL;
    size_t num_glyph_codes;
    size_t glyph_pixel_count;
    float glyph_brightness_max, glyph_brightness_min, glyph_brightness_range;

    num_glyph_codes = fv->num_glyph_codes;
    glyph_pixel_count = (size_t)fv->glyph_width * fv->glyph_height;
    
    cl_array = calloc(num_glyph_codes, sizeof cl_array[0]);
    glyph_array = calloc(num_glyph_codes * glyph_pixel_count, sizeof glyph_array[0]);
    sorted_gm = UB_CreateNew(fv->glyph_matrix->Width, fv->glyph_matrix->Height);
    if (!cl_array || !glyph_array || !sorted_gm)
    {
        fprintf(fv->log_fp, "Could not sort glyphs: %s\n", strerror(errno));
        goto FailExit;
    }

#pragma omp parallel for
    for (i = 0; i < (ptrdiff_t)num_glyph_codes; i++)
    {
        code_lum_p cl = &cl_array[i];
        cl->code = fv->glyph_codes[i];
        cl->index = i;
        cl->lum = fv->glyph_brightness[i];
    }

    qsort(cl_array, num_glyph_codes, sizeof cl_array[0], cl_compare);

#pragma omp parallel for
    for (i = 0; i < (ptrdiff_t)num_glyph_codes; i++)
    {
        ptrdiff_t j;
        code_lum_p cl = &cl_array[i];
        ptrdiff_t gm_sx, gm_sy, gm_dx, gm_dy, y;

        j = cl->index;
        fv->glyph_codes[i] = cl->code;
        fv->glyph_brightness[i] = cl->lum;

        // normalize_glyph(&glyph_array[glyph_pixel_count * i], &fv->glyph_vertical_array[glyph_pixel_count * j], glyph_pixel_count);
        memcpy(&glyph_array[glyph_pixel_count * i], &fv->glyph_vertical_array[glyph_pixel_count * j], glyph_pixel_count * sizeof glyph_array[0]);

        // Sort the glyph matrix as well
        gm_sx = j % fv->glyph_matrix_cols;
        gm_sy = j / fv->glyph_matrix_cols;
        gm_dx = i % fv->glyph_matrix_cols;
        gm_dy = i / fv->glyph_matrix_cols;

        gm_sx *= fv->glyph_width;
        gm_sy *= fv->glyph_height;
        gm_dx *= fv->glyph_width;
        gm_dy *= fv->glyph_height;

        if (!fv->white_background)
        {
            for (y = 0; y < fv->glyph_height; y++)
            {
                memcpy(&sorted_gm->RowPointers[gm_dy + y][gm_dx], &fv->glyph_matrix->RowPointers[gm_sy + y][gm_sx], fv->glyph_width * sizeof(uint32_t));
            }
        }
        else
        {
            for (y = 0; y < fv->glyph_height; y++)
            {
                uint32_t x;
                for (x = 0; x < fv->glyph_width; x++)
                {
                    sorted_gm->RowPointers[gm_dy + y][gm_dx + x] = 0x00ffffff ^ fv->glyph_matrix->RowPointers[gm_sy + y][gm_sx + x];
                }
            }
        }
    }

    glyph_brightness_min = fv->glyph_brightness[0];
    glyph_brightness_max = fv->glyph_brightness[num_glyph_codes - 1];
    glyph_brightness_range = glyph_brightness_max - glyph_brightness_min;

#pragma omp parallel for
    for (i = 0; i < (ptrdiff_t)num_glyph_codes; i++)
    {
        fv->glyph_brightness[i] = (fv->glyph_brightness[i] - glyph_brightness_min) / glyph_brightness_range;
    }

    UB_Free(&fv->glyph_matrix);
    fv->glyph_matrix = sorted_gm;
    free(fv->glyph_vertical_array);
    fv->glyph_vertical_array = glyph_array;
    free(cl_array);

#if DEBUG_OUTPUT_TO_SCREEN
    DebugRawBitmap(0, 0, fv->glyph_matrix->BitmapData, fv->glyph_matrix->Width, fv->glyph_matrix->Height);
#endif
    return 1;
FailExit:
    free(cl_array);
    free(glyph_array);
    UB_Free(&sorted_gm);
    return 0;
}

void normalize_glyph(float* out, const float* in, size_t glyph_pixel_count)
{
    double lengthsq = 0;
    double length;
    size_t i;

    for (i = 0; i < glyph_pixel_count; i++)
    {
        double in_val = in[i];
        lengthsq += in_val * in_val;
    }

    length = sqrt(lengthsq);
    if (length < 0.000001) length = 1.0;
    
    for (i = 0; i < glyph_pixel_count; i++)
    {
        double in_val = in[i];
        out[i] = (float)(in_val / length);
    }
}

static size_t get_glyph_bitmap_length(size_t num_glyph_codes)
{
    return ((num_glyph_codes - 1) / (8 * sizeof(size_t)) + 1);
}

static size_t* create_glyph_usage_bitmap(size_t num_glyph_codes)
{
    return calloc(get_glyph_bitmap_length(num_glyph_codes), sizeof(size_t));
}

static int get_glyph_usage(size_t* glyph_usage_bitmap, uint32_t code)
{
    uint32_t nbit = code % (8 * sizeof glyph_usage_bitmap[0]);
    size_t unit = code / (8 * sizeof glyph_usage_bitmap[0]);
    size_t bm = (size_t)1 << nbit;
    if (!glyph_usage_bitmap) return 0;
    return (bm & glyph_usage_bitmap[unit]) ? 1 : 0;
}

static void set_glyph_usage(size_t* glyph_usage_bitmap, uint32_t code, int value)
{
    uint32_t nbit = code % (8 * sizeof glyph_usage_bitmap[0]);
    size_t unit = code / (8 * sizeof glyph_usage_bitmap[0]);
    size_t bm = (size_t)1 << nbit;
    if (!glyph_usage_bitmap) return;
    if (value) glyph_usage_bitmap[unit] |= bm;
    else glyph_usage_bitmap[unit] &= ~bm;
}

static void clear_all_glyph_usage(size_t* glyph_usage_bitmap, size_t num_glyph_codes)
{
    size_t i;
    size_t length;
    if (!glyph_usage_bitmap) return;
    length = get_glyph_bitmap_length(num_glyph_codes);
    for (i = 0; i < length; i++) glyph_usage_bitmap[i] = 0;
}

static void frame_delete(fontvideo_frame_p* pf)
{
    fontvideo_frame_p f;

    if (!pf) return;
    f = *pf;
    if (!f) return;
    *pf = NULL;

    // free(f->raw_data); // RGBA pixel data
    // free(f->raw_data_row);
    // free(f->mono_data); // RGBA pixel data
    // free(f->mono_data_row);
    // free(f->data); // Font CharCode data
    // free(f->row);
    // free(f->c_data); // Color data
    // free(f->c_row);
    free(f->glyph_usage_bitmap);

    bunchfree(f);
}

// Create a frame, create buffers, copy the source bitmap, preparing for the rendering
static fontvideo_frame_p frame_create(fontvideo_p fv, double timestamp, void *bitmap, int bmp_width, int bmp_height, size_t bmp_pitch)
{
    ptrdiff_t i;
    uint32_t w, h;
    fontvideo_frame_p f;
    w = fv->output_w;
    h = fv->output_h;

    f = bunchalloc(16, sizeof *f,
        offsetof(fontvideo_frame_t, data), (size_t)w * h * sizeof f->data[0],
        offsetof(fontvideo_frame_t, c_data), (size_t)w * h * sizeof f->c_data[0],
        offsetof(fontvideo_frame_t, raw_data), (size_t)bmp_pitch * bmp_height,
        offsetof(fontvideo_frame_t, mono_data), (size_t)bmp_width * bmp_height * sizeof f->mono_data[0],
        offsetof(fontvideo_frame_t, row), (size_t)h * sizeof f->row[0],
        offsetof(fontvideo_frame_t, c_row), (size_t)h * sizeof f->c_row[0],
        offsetof(fontvideo_frame_t, raw_data_row), (size_t)bmp_height * sizeof f->raw_data_row[0],
        offsetof(fontvideo_frame_t, mono_data_row), (size_t)bmp_height * sizeof f->mono_data_row[0],
        0, 0);

    // f = calloc(1, sizeof f[0]);
    if (!f) return f;

    f->timestamp = timestamp;
    f->w = w;
    f->h = h;

    // Char code buffer
    // f->data = calloc((size_t)w * h, sizeof f->data[0]); if (!f->data) goto FailExit;
    // f->row = calloc(h, sizeof f->row[0]); if (!f->row) goto FailExit;

    // Char color buffer
    // f->c_data = calloc((size_t)w * h, sizeof f->c_data[0]); if (!f->c_data) goto FailExit;
    // f->c_row = calloc(h, sizeof f->c_row[0]); if (!f->c_row) goto FailExit;

    // Create row pointers for faster access to the matrix slots
    for (i = 0; i < (ptrdiff_t)h; i++)
    {
        f->row[i] = &f->data[i * w];
        f->c_row[i] = &f->c_data[i * w];
    }

    // Source bitmap buffer
    f->raw_w = bmp_width;
    f->raw_h = bmp_height;
    // f->raw_data = calloc(bmp_height, bmp_pitch); if (!f->raw_data) goto FailExit;
    // f->raw_data_row = calloc(bmp_height, sizeof f->raw_data_row[0]); if (!f->raw_data_row) goto FailExit;
    // f->mono_data = calloc(bmp_height, bmp_width * sizeof f->mono_data[0]); if (!f->mono_data) goto FailExit;
    // f->mono_data_row = calloc(bmp_height, sizeof f->mono_data_row[0]); if (!f->mono_data_row) goto FailExit;

    if (fv->avoid_repetition)
    {
        f->glyph_usage_bitmap = create_glyph_usage_bitmap(fv->num_glyph_codes);
    }

    // Copy the source bitmap whilst creating row pointers.
#pragma omp parallel for
    for (i = 0; i < (ptrdiff_t)bmp_height; i++)
    {
        f->raw_data_row[i] = (void*)&(((uint8_t*)f->raw_data)[i * bmp_pitch]);
        f->mono_data_row[i] = &f->mono_data[i * bmp_width];
        memcpy(f->raw_data_row[i], &((uint8_t*)bitmap)[i * bmp_pitch], bmp_pitch);
    }
    
    return f;
// FailExit:
//     frame_delete(f);
//     return NULL;
}

static void frame_normalize_input(fontvideo_frame_p f)
{
    int y;
    float max_lum = -999999.9f;
    float min_lum = 999999.9f;
    float lum_dif = 0.0f;
    float lum_scale = 1.7320508075688772935274463415059f;
// #pragma omp parallel for
    for (y = 0; y < (int)f->raw_h; y++)
    {
        int x;
        float row_max = max_lum;
        float row_min = min_lum;
        uint32_t *raw_row = f->raw_data_row[y];
        for (x = 0; x < (int)f->raw_w; x++)
        {
            union pixel
            {
                uint8_t u8[4];
                uint32_t u32;
            }   *src_pixel = (void *)&(raw_row[x]);
            float src_r = (float)src_pixel->u8[0] / 255.0f;
            float src_g = (float)src_pixel->u8[1] / 255.0f;
            float src_b = (float)src_pixel->u8[2] / 255.0f;
            float src_lum = (float)(sqrt(src_r * src_r + src_g * src_g + src_b * src_b) / sqrt_3);
            if (src_lum > row_max) row_max = src_lum;
            else if (src_lum < row_min) row_min = src_lum;
        }
// #pragma omp critical
        if (1)
        {
            if (row_max > max_lum) max_lum = row_max;
            if (row_min < min_lum) min_lum = row_min;
        }
    }
    max_lum /= lum_scale;
    min_lum /= lum_scale;
    lum_dif = max_lum - min_lum;

#pragma omp parallel for
    for (y = 0; y < (int)f->raw_h; y++)
    {
        int x;
        uint32_t *raw_row = f->raw_data_row[y];
        for (x = 0; x < (int)f->raw_w; x++)
        {
            union pixel
            {
                uint8_t u8[4];
                uint32_t u32;
            }   *src_pixel = (void *)&(raw_row[x]);
            float r = (float)src_pixel->u8[0] / 255.0f;
            float g = (float)src_pixel->u8[1] / 255.0f;
            float b = (float)src_pixel->u8[2] / 255.0f;
            r = (r - min_lum) / lum_dif; if (r < 0) r = 0; else if (r > 1) r = 1;
            g = (g - min_lum) / lum_dif; if (g < 0) g = 0; else if (g > 1) g = 1;
            b = (b - min_lum) / lum_dif; if (b < 0) b = 0; else if (b > 1) b = 1;
            src_pixel->u8[0] = (uint8_t)(r * 255.0f);
            src_pixel->u8[1] = (uint8_t)(g * 255.0f);
            src_pixel->u8[2] = (uint8_t)(b * 255.0f);
        }
    }
}

struct brightness_index
{
    int index;
    float brightness;
};

static int brightness_index_compare(const void* bi1, const void* bi2)
{
    const struct brightness_index* b1 = bi1;
    const struct brightness_index* b2 = bi2;
    if (b1->brightness < b2->brightness) return -1;
    if (b1->brightness > b2->brightness) return 1;
    return 0;
}

struct matching_status
{
    uint32_t src_x;
    uint32_t src_y;

    float src_min;
    float src_max;
    float src_range;

    uint32_t start_index;
    uint32_t end_index;
};

static int locked_get_glyph_usage(fontvideo_frame_p f, uint32_t code)
{
    int ic = 0;
    int ret = 0;
    atomic_int xp = 0;
    if (!f->glyph_usage_bitmap) return 0;
    while (!atomic_compare_exchange_strong(&f->sl_gub, &xp, 1))
    {
        xp = 0;
        backoff(&ic);
    }
    ret = get_glyph_usage(f->glyph_usage_bitmap, code);
    atomic_store(&f->sl_gub, 0);
    return ret;
}

static void locked_set_glyph_usage(fontvideo_frame_p f, uint32_t code, int value)
{
    int ic = 0;
    atomic_int xp = 0;
    if (!f->glyph_usage_bitmap) return;
    while (!atomic_compare_exchange_strong(&f->sl_gub, &xp, 1))
    {
        xp = 0;
        backoff(&ic);
    }
    set_glyph_usage(f->glyph_usage_bitmap, code, value);
    atomic_store(&f->sl_gub, 0);
}

static void locked_clear_all_glyph_usage(fontvideo_frame_p f, size_t num_glyph_codes)
{
    int ic = 0;
    atomic_int xp = 0;
    if (!f->glyph_usage_bitmap) return;
    while (!atomic_compare_exchange_strong(&f->sl_gub, &xp, 1))
    {
        xp = 0;
        backoff(&ic);
    }
    clear_all_glyph_usage(f->glyph_usage_bitmap, num_glyph_codes);
    atomic_store(&f->sl_gub, 0);
}

typedef uint32_t(*matching_algorithm)(fontvideo_p fv, fontvideo_frame_p f, struct matching_status* s);

static uint32_t match_by_differency(fontvideo_p fv, fontvideo_frame_p f, struct matching_status* s)
{
    size_t num_glyph_codes = fv->num_glyph_codes;
    uint32_t glyph_pixel_count = fv->glyph_width * fv->glyph_height;
    uint32_t src_x = s->src_x;
    uint32_t src_y = s->src_y;
    float src_min = s->src_min;
    float src_range = s->src_range;
    uint32_t cur_code_index;
    uint32_t best_code_index = s->start_index;
    int findmatch = 0;
    for (;;)
    {
        double best_score = 9999999999.9f;
        for (cur_code_index = s->start_index; cur_code_index <= s->end_index; cur_code_index++)
        {
            double score = 0;
            int x, y;
            float* font_lum_img = &fv->glyph_vertical_array[cur_code_index * glyph_pixel_count];

            // Compare each pixels and collect the scores.
            for (y = 0; y < (int)fv->glyph_height; y++)
            {
                size_t row_start = (size_t)y * fv->glyph_width;
                float* buf_row = &f->mono_data_row[src_y + y][src_x];
                float* font_row = &font_lum_img[row_start];
                for (x = 0; x < (int)fv->glyph_width; x++)
                {
                    float src_lum = buf_row[x];
                    float font_lum = font_row[x];
                    double nsrc = (double)(src_lum - src_min) / src_range;
                    double diff = font_lum - nsrc;
                    score += diff * diff;
                }
            }

            if (score <= best_score)
            {
                int glyph_used = locked_get_glyph_usage(f, cur_code_index);
                if (!glyph_used)
                {
                    best_score = score;
                    best_code_index = cur_code_index;
                    findmatch = 1;
                }
            }
        }
        if (findmatch) break;
        locked_clear_all_glyph_usage(f, num_glyph_codes);
    }
    return best_code_index;
}

static uint32_t match_by_dotproduct(fontvideo_p fv, fontvideo_frame_p f, struct matching_status* s)
{
    size_t num_glyph_codes = fv->num_glyph_codes;
    uint32_t glyph_pixel_count = fv->glyph_width * fv->glyph_height;
    uint32_t src_x = s->src_x;
    uint32_t src_y = s->src_y;
    float src_min = s->src_min;
    float src_range = s->src_range;
    uint32_t cur_code_index;
    uint32_t best_code_index = s->start_index;
    int findmatch = 0;
    for (;;)
    {
        double best_score = -9999999999.9f;
        for (cur_code_index = s->start_index; cur_code_index <= s->end_index; cur_code_index++)
        {
            double score = 0;
            int x, y;
            float* font_lum_img = &fv->glyph_vertical_array[cur_code_index * glyph_pixel_count];

            // Compare each pixels and collect the scores.
            for (y = 0; y < (int)fv->glyph_height; y++)
            {
                size_t row_start = (size_t)y * fv->glyph_width;
                float* buf_row = &f->mono_data_row[src_y + y][src_x];
                float* font_row = &font_lum_img[row_start];
                for (x = 0; x < (int)fv->glyph_width; x++)
                {
                    float src_lum = buf_row[x];
                    float font_lum = font_row[x];
                    double nsrc = (double)(src_lum - src_min) / src_range;
                    score += (font_lum - 0.5) * (nsrc - 0.5);
                }
            }
            score /= glyph_pixel_count;

            if (score >= best_score)
            {
                int glyph_used = locked_get_glyph_usage(f, cur_code_index);
                if (!glyph_used)
                {
                    best_score = score;
                    best_code_index = cur_code_index;
                    findmatch = 1;
                }
            }
        }
        if (findmatch) break;
        locked_clear_all_glyph_usage(f, num_glyph_codes);
    }
    return best_code_index;
}

static void do_cpu_render(fontvideo_p fv, fontvideo_frame_p f)
{
    int fy, fw, fh;
    size_t num_glyph_codes = fv->num_glyph_codes;
    uint32_t glyph_pixel_count = fv->glyph_width * fv->glyph_height;
    int i;
    static union color
    {
        float f[3];
        struct desc
        {
            float b, g, r;
        }rgb;
    }palette[16] = { 0 };
    static struct brightness_index palette_brightness_index[16] = {0};
    static atomic_int palette_initializing = 0;
    static int palette_initialized = 0;
    matching_algorithm algorithm;

    switch (fv->algorithm)
    {
    default:
    case alg_differency:
        algorithm = match_by_differency;
        break;
    case alg_dotproduct:
        algorithm = match_by_dotproduct;
        break;
    }

    fw = f->w;
    fh = f->h;

    if (!palette_initialized && !atomic_exchange(&palette_initializing, 1))
    {
        // Get the palette from the table and convert to float [0, 1]
        for (i = 0; i < 16; i++)
        {
            palette[i].rgb.r = (float)console_palette[i].rgb.r / 255.0f;
            palette[i].rgb.g = (float)console_palette[i].rgb.g / 255.0f;
            palette[i].rgb.b = (float)console_palette[i].rgb.b / 255.0f;
        }

        // Process palettes to make vectors for matching
        // To prevent all-grey (color-picking failed)
        for (i = 0; i < 16; i++)
        {
            float length;
            palette[i].rgb.r -= 0.5f;
            palette[i].rgb.g -= 0.5f;
            palette[i].rgb.b -= 0.5f;
            length = sqrtf(
                palette[i].rgb.r * palette[i].rgb.r +
                palette[i].rgb.g * palette[i].rgb.g +
                palette[i].rgb.b * palette[i].rgb.b);

            // Do normalize
            if (length < 0.000001f) length = 1;
            palette[i].rgb.r /= length;
            palette[i].rgb.g /= length;
            palette[i].rgb.b /= length;

            palette_brightness_index[i].index = i;
            palette_brightness_index[i].brightness = (float)(length / sqrt_3);
        }
        qsort(palette_brightness_index, 16, sizeof palette_brightness_index[0], brightness_index_compare);
        palette_initialized = 1;
        palette_initializing = 0;
    }

    if (fv->normalize_input) frame_normalize_input(f);

#pragma omp parallel for
    for (fy = 0; fy < fh; fy++)
    {
        int fx, sx, sy;
        uint32_t *row = f->row[fy];
        uint8_t *c_row = f->c_row[fy];
        sy = fy * fv->glyph_height;
        struct matching_status ms = {0};
        ms.src_y = sy;

        for (fx = 0; fx < fw; fx++)
        {
            uint32_t x, y;
            uint32_t best_code_index = 0;
            uint32_t start_code_position = 0;
            uint32_t end_code_position = 0;
            double src_brightness = 0;
            sx = fx * fv->glyph_width;
            ms.src_min = 999;
            ms.src_max = 0;
            ms.src_x = sx;

            // Compute source brightness for further use
            for (y = 0; y < fv->glyph_height; y++)
            {
                uint32_t *raw_row = &f->raw_data_row[sy + y][sx];
                float *buf_row = &f->mono_data_row[sy + y][sx];
                for (x = 0; x < fv->glyph_width; x++)
                {
                    union pixel
                    {
                        uint8_t u8[4];
                        uint32_t u32;
                    }   *src_pixel = (void *)&(raw_row[x]);
                    float src_r, src_g, src_b;
                    float src_lum;
                    src_r = (float)src_pixel->u8[0] / 255.0f;
                    src_g = (float)src_pixel->u8[1] / 255.0f;
                    src_b = (float)src_pixel->u8[2] / 255.0f;
                    src_lum = (float)(sqrt(src_r * src_r + src_g * src_g + src_b * src_b) / sqrt_3);
                    src_brightness += (double)src_lum; // Get brightness
                    ms.src_min = min(ms.src_min, src_lum);
                    ms.src_max = max(ms.src_max, src_lum);
                    buf_row[x] = src_lum; // Store for normalize
                }
            }
            src_brightness /= glyph_pixel_count;
            ms.src_range = ms.src_max - ms.src_min;
            if (ms.src_range <= 0.000001)
            {
                ms.src_max = 1;
                ms.src_min = 0;
                ms.src_range = 1;
            }

            if (1)
            {
                int
                    left = 0,
                    right = (int)(num_glyph_codes - 1),
                    mid = (int)(num_glyph_codes / 2);
                float left_brightness = fv->glyph_brightness[left];
                float right_brightness = fv->glyph_brightness[right];
                // bisection approximation
                for (;;)
                {
                    float mid_brightness = fv->glyph_brightness[mid];
                    if (src_brightness < mid_brightness)
                    {
                        right = mid;
                        right_brightness = fv->glyph_brightness[right];
                    }
                    else if (src_brightness > mid_brightness)
                    {
                        left = mid;
                        left_brightness = fv->glyph_brightness[left];
                    }
                    else
                    {
                        start_code_position = (left / fv->candidate_glyph_window_size) * fv->candidate_glyph_window_size;
                        end_code_position = start_code_position + fv->candidate_glyph_window_size - 1;
                        break;
                    }
                    mid = (right + left) / 2;
                    if (mid == left)
                    {
                        start_code_position = (left / fv->candidate_glyph_window_size) * fv->candidate_glyph_window_size;
                        end_code_position = start_code_position + fv->candidate_glyph_window_size - 1;
                        break;
                    }
                }

                if (end_code_position > num_glyph_codes - 1) end_code_position = (int32_t)num_glyph_codes - 1;
            }

            ms.start_index = start_code_position;
            ms.end_index = end_code_position;
            best_code_index = algorithm(fv, f, &ms);

            // The best matching char code
            row[fx] = best_code_index;
            locked_set_glyph_usage(f, best_code_index, 1);

            // Start picking color
            if (fv->do_colored_output)
            {
                float avr_r = 0, avr_g = 0, avr_b = 0;
                double avr_brightness = 0;
                uint8_t col = 0;
                int scp = 0;
                int ecp = 0;
                const int palette_brightness_window = 12;
                int hcbw = palette_brightness_window / 2;
                float sbr, ebr;
                double rgb_normalize;
                float best_score;

                // Get the average color of the char from the source frame
                for (y = 0; y < (int)fv->glyph_height; y++)
                {
                    uint32_t* raw_row = &f->raw_data_row[sy + y][sx];
                    for (x = 0; x < (int)fv->glyph_width; x++)
                    {
                        union pixel
                        {
                            uint8_t u8[4];
                            uint32_t u32;
                        }   *src_pixel = (void*)&(raw_row[x]);

                        avr_b += (float)src_pixel->u8[0] / 255.0f;
                        avr_g += (float)src_pixel->u8[1] / 255.0f;
                        avr_r += (float)src_pixel->u8[2] / 255.0f;
                    }
                }

                avr_b /= glyph_pixel_count;
                avr_g /= glyph_pixel_count;
                avr_r /= glyph_pixel_count;
                // **The following code uses vector matching method to pickup best color**
                avr_b -= 0.5f;
                avr_g -= 0.5f;
                avr_r -= 0.5f;
                rgb_normalize = sqrt(avr_r * avr_r + avr_g * avr_g + avr_b * avr_b);
                if (rgb_normalize < 0.000001) rgb_normalize = 1;
                avr_b = (float)(avr_b / rgb_normalize);
                avr_g = (float)(avr_g / rgb_normalize);
                avr_r = (float)(avr_r / rgb_normalize);
                avr_brightness = rgb_normalize / sqrt_3;

                scp = 0;
                ecp = 15;
                sbr = palette_brightness_index[scp].brightness;
                ebr = palette_brightness_index[ecp].brightness;
                for (; scp != ecp;)
                {
                    int mid = (scp + ecp) / 2;
                    float mb;
                    if (mid == scp) break;
                    mb = palette_brightness_index[mid].brightness;
                    if (avr_brightness < mb)
                    {
                        ecp = mid;
                        ebr = palette_brightness_index[ecp].brightness;
                    }
                    else if (src_brightness > mb)
                    {
                        scp = mid;
                        sbr = palette_brightness_index[scp].brightness;
                    }
                    else
                    {
                        break;
                    }
                }

                scp -= hcbw;
                ecp = scp + hcbw + hcbw;
                if (scp < 0) scp = 0;
                else if (scp > 15) scp = 15;
                if (ecp < 0) ecp = 0;
                else if (ecp > 15) ecp = 15;

                best_score = -9999999.9f;
                col = 0;
                for (i = scp; i <= ecp; i++)
                {
                    int j = palette_brightness_index[i].index;
                    // **The following code uses vector matching method to pickup best color**
                    float score =
                        avr_r * palette[j].rgb.r +
                        avr_g * palette[j].rgb.g +
                        avr_b * palette[j].rgb.b;
                    if (score >= best_score)
                    {
                        best_score = score;
                        col = (uint8_t)j;
                    }
                }

                // Avoid generating pure black characters.
                if (!col) col = 0x08;

                c_row[fx] = col;
            }
            else
            {
                c_row[fx] = 15;
            }
#if DEBUG_OUTPUT_TO_SCREEN
            for (y = 0; y < (int)fv->glyph_height; y++)
            {
                uint32_t* raw_row = &f->raw_data_row[sy + y][sx];
                float* mono_row = &f->mono_data_row[sy + y][sx];
                float* font_row = &fv->glyph_vertical_array[(size_t)best_code_index * glyph_pixel_count + (size_t)y * fv->glyph_width];
                for (x = 0; x < (int)fv->glyph_width; x++)
                {
                    union pixel
                    {
                        uint8_t u8[4];
                        uint32_t u32;
                    }   *src_pixel = (void*)&(raw_row[x]);
                    if (1)
                    {
                        uint8_t uv = (uint8_t)(font_row[x] * 255.0f);
                        src_pixel->u8[0] = uv;
                        src_pixel->u8[1] = uv;
                        src_pixel->u8[2] = uv;
                    }
                    if (1)
                    {
                        uint8_t uv = (uint8_t)((mono_row[x] - ms.src_min) * 255.0 / ms.src_range);
                        src_pixel = (void*)&(mono_row[x]);
                        src_pixel->u8[0] = uv;
                        src_pixel->u8[1] = uv;
                        src_pixel->u8[2] = uv;
                    }
                }
            }
#endif
        }
    }
    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Finished CPU rendering a frame. (%u)\n", get_thread_id());
    }
}

// Render the frame from bitmap into font char codes. One of the magic happens here.
static void render_frame_from_rgbabitmap(fontvideo_p fv, fontvideo_frame_p f)
{
    if (f->rendered) return;

    atomic_fetch_add(&fv->rendering_frame_count, 1);
    f->rendering_start_time = rttimer_gettime(&fv->tmr);
    do_cpu_render(fv, f);

    f->rendering_time_consuming = rttimer_gettime(&fv->tmr) - f->rendering_start_time;

#if DEBUG_OUTPUT_TO_SCREEN
    DebugRawBitmap(0, 0, f->raw_data, f->raw_w, f->raw_h);
    DebugRawBitmap(0, f->raw_h, f->mono_data, f->raw_w, f->raw_h);
#endif

    // Finished rendering, update statistics
    atomic_exchange(&f->rendered, get_thread_id());
    atomic_fetch_sub(&fv->rendering_frame_count, 1);
    atomic_fetch_add(&fv->rendered_frame_count, 1);
}

// Lock the link list of the frames, insert the frame into the link list, preserve the ordering.
// The link list of the frames is the cached frames to be rendered.
static void locked_add_frame_to_fv(fontvideo_p fv, fontvideo_frame_p f)
{
    double timestamp;
    fontvideo_frame_p iter;

    lock_frame_linklist(fv);

    timestamp = f->timestamp;
    f->index = atomic_fetch_add(&fv->frame_counter, 1);
    atomic_fetch_add(&fv->precached_frame_count, 1);

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
    unlock_frame_linklist(fv);
}

#ifndef FONTVIDEO_NO_SOUND
// Create the audio 
static fontvideo_audio_p audio_create(int sample_rate, void *samples, int channel_count, size_t num_samples_per_channel, double timestamp)
{
    fontvideo_audio_p a = NULL;
    ptrdiff_t i;
    float *src = samples;
    
    if (channel_count < 1 || channel_count > 2) return a;

    a = bunchalloc(16, sizeof *a, 
        offsetof(fontvideo_audio_t, buffer), num_samples_per_channel * channel_count * sizeof a->buffer[0],
        0, 0);
    
    // a = calloc(1, sizeof a[0]);
    if (!a) return a;

    a->timestamp = timestamp;
    a->duration = (double)num_samples_per_channel / sample_rate;
    a->frames = num_samples_per_channel;
    // a->buffer = calloc(num_samples_per_channel * channel_count, sizeof a->buffer[0]);
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
    audio_delete(&a);
    return NULL;
}

static void locked_add_audio_to_fv(fontvideo_p fv, fontvideo_audio_p a)
{
    double timestamp;
    fontvideo_audio_p iter;
    
    timestamp = a->timestamp;

    lock_audio_linklist(fv);

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
    unlock_audio_linklist(fv);
}
#endif

static void fv_on_get_video(avdec_p av, void *bitmap, int width, int height, size_t pitch, double timestamp)
{
    fontvideo_p fv = av->userdata;
    fontvideo_frame_p f;

    f = frame_create(fv, timestamp, bitmap, width, height, pitch);
    if (!f)
    {
        return;
    }
    locked_add_frame_to_fv(fv, f);
}

static void ensure_avi_writer(fontvideo_p fv)
{
    if (!fv->avi_writer)
    {
        int i;
        WaveFormatEx_t wf = { 0 };
        BitmapInfoHeader_t BMIF = { 0 };
        uint32_t Palette[16] = { 0 };
        uint32_t bw, bh;
        size_t pitch;

        bw = fv->glyph_width * fv->output_w;
        bh = fv->glyph_height * fv->output_h;

        if (fv->font_is_blackwhite)
        {
            pitch = (((size_t)bw * 8 - 1) / 32 + 1) * 4;

            BMIF.biSize = 40;
            BMIF.biWidth = bw;
            BMIF.biHeight = bh;
            BMIF.biPlanes = 1;
            BMIF.biBitCount = 8;
            BMIF.biCompression = 0;
            BMIF.biSizeImage = (uint32_t)(pitch * bh);
            BMIF.biXPelsPerMeter = 0;
            BMIF.biYPelsPerMeter = 0;
            BMIF.biClrUsed = 16;
            BMIF.biClrImportant = 0;
            for (i = 0; i < 16; i++)
            {
                Palette[i] = console_palette[i].packed;
            }
        }
        else
        {
            pitch = (((size_t)bw * 24 - 1) / 32 + 1) * 4;

            BMIF.biSize = 40;
            BMIF.biWidth = bw;
            BMIF.biHeight = bh;
            BMIF.biPlanes = 1;
            BMIF.biBitCount = 24;
            BMIF.biCompression = 0;
            BMIF.biSizeImage = (uint32_t)(pitch * bh);
            BMIF.biXPelsPerMeter = 0;
            BMIF.biYPelsPerMeter = 0;
            BMIF.biClrUsed = 0;
            BMIF.biClrImportant = 0;
        }

        wf.FormatTag = 3;
        wf.Channels = (uint16_t)fv->av->decoded_af.num_channels;
        wf.SamplesPerSec = fv->av->decoded_af.sample_rate;
        wf.BlockAlign = wf.Channels * sizeof(float);
        wf.BitsPerSample = 32;
        wf.AvgBytesPerSec = wf.SamplesPerSec * wf.BlockAlign;

        fv->avi_writer = AVIWriterStart(fv->output_avi_file, (uint32_t)fv->av->decoded_vf.framerate, &BMIF, Palette, NULL, &wf);
    }
}

static int check_nan_inf(float *audio, int channel_count, size_t num_samples_per_channel)
{
    size_t i, num;
    int ret = 0;
    float prevs[2] = { 0 };
    num = num_samples_per_channel * channel_count;
    for (i = 0; i < num; i++)
    {
        int ch = i % channel_count;
        switch (fpclassify(audio[i]))
        {
        case FP_NAN:
        case FP_INFINITE:
            audio[i] = prevs[ch];
            ret = 1;
            break;
        default:
            break;
        }
        prevs[ch] = audio[i]; 
    }
    return ret;
}

static void fv_on_get_audio(avdec_p av, void **samples_of_channel, int channel_count, size_t num_samples_per_channel, double timestamp)
{
#ifndef FONTVIDEO_NO_SOUND
    fontvideo_p fv = av->userdata;
    fontvideo_audio_p a = NULL;

    if (fv->output_avi_file) ensure_avi_writer(fv);
    if (fv->avi_writer)
    {
        // check_nan_inf(samples_of_channel[0], channel_count, num_samples_per_channel);
        AVIWriterWriteAudio(fv->avi_writer, samples_of_channel[0], (uint32_t)(num_samples_per_channel * channel_count * sizeof(float)));
    }

    if (!fv->do_audio_output) return;

    a = audio_create(av->decoded_af.sample_rate, samples_of_channel[0], channel_count, num_samples_per_channel, timestamp);
    if (!a)
    {
        return;
    }
    locked_add_audio_to_fv(fv, a);
#endif
}

static int get_frame_and_render(fontvideo_p fv)
{
    fontvideo_frame_p f = NULL, prev = NULL;
    if (!fv->frames) return 0;
    if (fv->precached_frame_count <= fv->rendered_frame_count + fv->rendering_frame_count)
    {
        return 0;
    }
    lock_frame_linklist(fv);
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
            double cur_time = rttimer_gettime(&fv->tmr);
            double lagged_time = cur_time - f->timestamp;
            double last_output_to_now = cur_time - fv->last_output_time;
            // If the frame isn't being rendered, first detect if it's too late to render it, then do frame skipping.
            if (!fv->no_frameskip && fv->real_time_play && fv->prepared && lagged_time >= 1.0 && last_output_to_now >= 0.5)
            {
                fprintf(fv->log_fp, "Discarding frame %u to render due to lag (%lf seconds). (%d)\n", f->index, lagged_time, get_thread_id());
                
                // Too late to render the frame, skip it.
                fontvideo_frame_p next = f->next;
                if (!prev)
                {
                    assert(f == fv->frames);
                    fv->frames = next;
                    if (!next)
                    {
                        fv->frame_last = NULL;
                        atomic_fetch_sub(&fv->precached_frame_count, 1);
                        unlock_frame_linklist(fv);
                        return 0;
                    }
                    else
                    {
                        frame_delete(&f);
                        f = fv->frames = next;
                        atomic_fetch_sub(&fv->precached_frame_count, 1);
                        continue;
                    }
                }
                else
                {
                    prev->next = next;
                    frame_delete(&f);
                    f = next;
                    atomic_fetch_sub(&fv->precached_frame_count, 1);
                    continue;
                }
            }
            else
            {
                if (fv->verbose)
                {
                    fprintf(fv->log_fp, "Got frame to render. (%u)\n", get_thread_id());
                }
                unlock_frame_linklist(fv);
                render_frame_from_rgbabitmap(fv, f);
                return 1;
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
    unlock_frame_linklist(fv);
    return f ? 1 : 0;
}

static int create_1bit_image(fontvideo_p fv, fontvideo_frame_p rendered_frame, void** out_file_content_buffer, size_t* out_file_content_size, size_t* out_offset_of_file_body)
{
    void* buffer = NULL;
    size_t buf_size = 0;
    size_t pitch;
    uint32_t cx, cy, bw, bh;
    uint8_t* bmp;
    UniformBitmap_p fm = fv->glyph_matrix;

#pragma pack(push, 1)
    struct Bmp1BitPerPixelHeader
    {
        uint16_t    bfType;
        uint32_t    bfSize;
        uint16_t    bfReserved1;
        uint16_t    bfReserved2;
        uint32_t    bfOffBits;
        uint32_t    biSize;
        int32_t     biWidth;
        int32_t     biHeight;
        uint16_t    biPlanes;
        uint16_t    biBitCount;
        uint32_t    biCompression;
        uint32_t    biSizeImage;
        int32_t     biXPelsPerMeter;
        int32_t     biYPelsPerMeter;
        uint32_t    biClrUsed;
        uint32_t    biClrImportant;
        uint32_t    Palette[2];
    } *header;
#pragma pack(pop)

    bw = fv->glyph_width * rendered_frame->w;
    bh = fv->glyph_height * rendered_frame->h;

    pitch = (((size_t)bw - 1) / 32 + 1) * 4;
    buf_size = sizeof header[0] + pitch * bh;
    buffer = malloc(buf_size);
    if (!buffer)
    {
        fprintf(fv->log_fp, "Could not allocate memory (%zu bytes) for images to be output: %s\n", buf_size, strerror(errno));
        goto FailExit;
    }
    memset(buffer, 0, buf_size);
    if (out_offset_of_file_body) *out_offset_of_file_body = sizeof header[0];

    header = buffer;
    header->bfType = 0x4d42;
    header->bfSize = (uint32_t)buf_size;
    header->bfReserved1 = 0;
    header->bfReserved2 = 0;
    header->bfOffBits = sizeof header[0];
    header->biSize = 40;
    header->biWidth = bw;
    header->biHeight = bh;
    header->biPlanes = 1;
    header->biBitCount = 1;
    header->biCompression = 0;
    header->biSizeImage = (uint32_t)(pitch * bh);
    header->biXPelsPerMeter = 0;
    header->biYPelsPerMeter = 0;
    header->biClrUsed = 2;
    header->biClrImportant = 0;
    header->Palette[0] = console_palette[0].packed;
    header->Palette[1] = console_palette[15].packed;

    bmp = (void*)&header[1];
    for (cy = 0; cy < rendered_frame->h; cy++)
    {
        uint32_t* con_row = rendered_frame->row[cy];
        uint32_t dy = cy * fv->glyph_height;
        for (cx = 0; cx < rendered_frame->w; cx++)
        {
            uint32_t code = con_row[cx];
            int font_x = (int)((code % fv->glyph_matrix_cols) * fv->glyph_width);
            int font_y = (int)((code / fv->glyph_matrix_cols) * fv->glyph_height);
            uint32_t dx = cx * fv->glyph_width;
            uint32_t x, y;
            for (y = 0; y < fv->glyph_height; y++)
            {
                uint32_t* src_row = fm->RowPointers[font_y + y];
                uint8_t* dst_row = &bmp[(size_t)(bh - 1 - (dy + y)) * pitch];
                for (x = 0; x < fv->glyph_width; x++)
                {
                    size_t slot = (size_t)(dx + x) / 8;
                    int bit = 7 - x % 8;
                    if (src_row[font_x + x] > 0xff7f7f7f) dst_row[slot] |= 1 << bit;
                }
            }
        }
    }

    *out_file_content_buffer = buffer;
    *out_file_content_size = buf_size;
    return 1;
FailExit:
    free(buffer);
    if (out_file_content_buffer) *out_file_content_buffer = NULL;
    if (out_file_content_size) *out_file_content_size = 0;
    return 0;
}

static int create_4bit_image(fontvideo_p fv, fontvideo_frame_p rendered_frame, void** out_file_content_buffer, size_t* out_file_content_size, size_t* out_offset_of_file_body)
{
    void* buffer = NULL;
    size_t i;
    size_t buf_size = 0;
    size_t pitch;
    uint32_t cx, cy, bw, bh;
    uint8_t* bmp;
    UniformBitmap_p fm = fv->glyph_matrix;

#pragma pack(push, 1)
    struct Bmp4BitPerPixelHeader
    {
        uint16_t    bfType;
        uint32_t    bfSize;
        uint16_t    bfReserved1;
        uint16_t    bfReserved2;
        uint32_t    bfOffBits;
        uint32_t    biSize;
        int32_t     biWidth;
        int32_t     biHeight;
        uint16_t    biPlanes;
        uint16_t    biBitCount;
        uint32_t    biCompression;
        uint32_t    biSizeImage;
        int32_t     biXPelsPerMeter;
        int32_t     biYPelsPerMeter;
        uint32_t    biClrUsed;
        uint32_t    biClrImportant;
        uint32_t    Palette[16];
    } *header;
#pragma pack(pop)

    bw = fv->glyph_width * rendered_frame->w;
    bh = fv->glyph_height * rendered_frame->h;

    pitch = (((size_t)bw * 4 - 1) / 32 + 1) * 4;
    buf_size = sizeof header[0] + pitch * bh;
    buffer = malloc(buf_size);
    if (!buffer)
    {
        fprintf(fv->log_fp, "Could not allocate memory (%zu bytes) for images to be output: %s\n", buf_size, strerror(errno));
        goto FailExit;
    }
    memset(buffer, 0, buf_size);
    if (out_offset_of_file_body) *out_offset_of_file_body = sizeof header[0];

    header = buffer;
    header->bfType = 0x4d42;
    header->bfSize = (uint32_t)buf_size;
    header->bfReserved1 = 0;
    header->bfReserved2 = 0;
    header->bfOffBits = sizeof header[0];
    header->biSize = 40;
    header->biWidth = bw;
    header->biHeight = bh;
    header->biPlanes = 1;
    header->biBitCount = 4;
    header->biCompression = 0;
    header->biSizeImage = (uint32_t)(pitch * bh);
    header->biXPelsPerMeter = 0;
    header->biYPelsPerMeter = 0;
    header->biClrUsed = 16;
    header->biClrImportant = 0;
    for (i = 0; i < 16; i++)
    {
        header->Palette[i] = console_palette[i].packed;
    }

    bmp = (void*)&header[1];
    for (cy = 0; cy < rendered_frame->h; cy++)
    {
        uint32_t* con_row = rendered_frame->row[cy];
        uint8_t* col_row = rendered_frame->c_row[cy];
        uint32_t dy = cy * fv->glyph_height;
        for (cx = 0; cx < rendered_frame->w; cx++)
        {
            uint32_t code = con_row[cx];
            uint8_t color = col_row[cx];
            int font_x = (int)((code % fv->glyph_matrix_cols) * fv->glyph_width);
            int font_y = (int)((code / fv->glyph_matrix_cols) * fv->glyph_height);
            uint32_t dx = cx * fv->glyph_width;
            uint32_t x, y;
            for (y = 0; y < fv->glyph_height; y++)
            {
                uint32_t* src_row = fm->RowPointers[font_y + y];
                uint8_t* dst_row = &bmp[(size_t)(bh - 1 - (dy + y)) * pitch];
                for (x = 0; x < fv->glyph_width; x += 2)
                {
                    size_t index = (size_t)(dx + x) >> 1;
                    dst_row[index] = 0;
                    if (src_row[font_x + x + 0] > 0xff7f7f7f) dst_row[index] |= color << 4;
                    if (src_row[font_x + x + 1] > 0xff7f7f7f) dst_row[index] |= color;
                }
            }
        }
    }

    *out_file_content_buffer = buffer;
    *out_file_content_size = buf_size;
    return 1;
FailExit:
    free(buffer);
    if (out_file_content_buffer) *out_file_content_buffer = NULL;
    if (out_file_content_size) *out_file_content_size = 0;
    return 0;
}

static int create_8bit_image(fontvideo_p fv, fontvideo_frame_p rendered_frame, void** out_file_content_buffer, size_t* out_file_content_size, size_t* out_offset_of_file_body)
{
    void* buffer = NULL;
    size_t i;
    size_t buf_size = 0;
    size_t pitch;
    uint32_t cx, cy, bw, bh;
    uint8_t* bmp;
    UniformBitmap_p fm = fv->glyph_matrix;

#pragma pack(push, 1)
    struct Bmp4BitPerPixelHeader
    {
        uint16_t    bfType;
        uint32_t    bfSize;
        uint16_t    bfReserved1;
        uint16_t    bfReserved2;
        uint32_t    bfOffBits;
        uint32_t    biSize;
        int32_t     biWidth;
        int32_t     biHeight;
        uint16_t    biPlanes;
        uint16_t    biBitCount;
        uint32_t    biCompression;
        uint32_t    biSizeImage;
        int32_t     biXPelsPerMeter;
        int32_t     biYPelsPerMeter;
        uint32_t    biClrUsed;
        uint32_t    biClrImportant;
        uint32_t    Palette[256];
    } *header;
#pragma pack(pop)

    bw = fv->glyph_width * rendered_frame->w;
    bh = fv->glyph_height * rendered_frame->h;

    pitch = (((size_t)bw * 8 - 1) / 32 + 1) * 4;
    buf_size = sizeof header[0] + pitch * bh;
    buffer = malloc(buf_size);
    if (!buffer)
    {
        fprintf(fv->log_fp, "Could not allocate memory (%zu bytes) for images to be output: %s\n", buf_size, strerror(errno));
        goto FailExit;
    }
    memset(buffer, 0, buf_size);
    if (out_offset_of_file_body) *out_offset_of_file_body = sizeof header[0];

    header = buffer;
    header->bfType = 0x4d42;
    header->bfSize = (uint32_t)buf_size;
    header->bfReserved1 = 0;
    header->bfReserved2 = 0;
    header->bfOffBits = sizeof header[0];
    header->biSize = 40;
    header->biWidth = bw;
    header->biHeight = bh;
    header->biPlanes = 1;
    header->biBitCount = 8;
    header->biCompression = 0;
    header->biSizeImage = (uint32_t)(pitch * bh);
    header->biXPelsPerMeter = 0;
    header->biYPelsPerMeter = 0;
    header->biClrUsed = 16;
    header->biClrImportant = 0;
    for (i = 0; i < 16; i++)
    {
        header->Palette[i] = console_palette[i].packed;
    }

    bmp = (void*)&header[1];
    for (cy = 0; cy < rendered_frame->h; cy++)
    {
        uint32_t* con_row = rendered_frame->row[cy];
        uint8_t* col_row = rendered_frame->c_row[cy];
        uint32_t dy = cy * fv->glyph_height;
        for (cx = 0; cx < rendered_frame->w; cx++)
        {
            uint32_t code = con_row[cx];
            uint8_t color = col_row[cx];
            int font_x = (int)((code % fv->glyph_matrix_cols) * fv->glyph_width);
            int font_y = (int)((code / fv->glyph_matrix_cols) * fv->glyph_height);
            uint32_t dx = cx * fv->glyph_width;
            uint32_t x, y;
            for (y = 0; y < fv->glyph_height; y++)
            {
                uint32_t* src_row = fm->RowPointers[font_y + y];
                uint8_t* dst_row = &bmp[(size_t)(bh - 1 - (dy + y)) * pitch];
                for (x = 0; x < fv->glyph_width; x ++)
                {
                    size_t index = (size_t)(dx + x);
                    if (src_row[font_x + x] > 0xff7f7f7f) dst_row[index] = color;
                }
            }
        }
    }

    *out_file_content_buffer = buffer;
    *out_file_content_size = buf_size;
    return 1;
FailExit:
    free(buffer);
    if (out_file_content_buffer) *out_file_content_buffer = NULL;
    if (out_file_content_size) *out_file_content_size = 0;
    return 0;
}

static int create_24bit_image(fontvideo_p fv, fontvideo_frame_p rendered_frame, void** out_file_content_buffer, size_t* out_file_content_size, size_t* out_offset_of_file_body)
{
    void* buffer = NULL;
    size_t i;
    size_t buf_size = 0;
    size_t pitch;
    uint32_t cx, cy, bw, bh;
    uint8_t* bmp;
    UniformBitmap_p fm = fv->glyph_matrix;

#pragma pack(push, 1)
    struct Bmp24BitPerPixelHeader
    {
        uint16_t    bfType;
        uint32_t    bfSize;
        uint16_t    bfReserved1;
        uint16_t    bfReserved2;
        uint32_t    bfOffBits;
        uint32_t    biSize;
        int32_t     biWidth;
        int32_t     biHeight;
        uint16_t    biPlanes;
        uint16_t    biBitCount;
        uint32_t    biCompression;
        uint32_t    biSizeImage;
        int32_t     biXPelsPerMeter;
        int32_t     biYPelsPerMeter;
        uint32_t    biClrUsed;
        uint32_t    biClrImportant;
    } *header;
    union pixel
    {
        uint32_t u32;
        uint8_t u8[4];
    } Palette[16] = { 0 };
#pragma pack(pop)

    bw = fv->glyph_width * rendered_frame->w;
    bh = fv->glyph_height * rendered_frame->h;

    for (i = 0; i < 16; i++)
    {
        Palette[i].u32 = console_palette[i].packed;
    }

    pitch = (((size_t)bw * 24 - 1) / 32 + 1) * 4;
    buf_size = sizeof header[0] + pitch * bh;
    buffer = malloc(buf_size);
    if (!buffer)
    {
        fprintf(fv->log_fp, "Could not allocate memory (%zu bytes) for images to be output: %s\n", buf_size, strerror(errno));
        goto FailExit;
    }
    memset(buffer, 0, buf_size);
    if (out_offset_of_file_body) *out_offset_of_file_body = sizeof header[0];

    header = buffer;
    header->bfType = 0x4d42;
    header->bfSize = (uint32_t)buf_size;
    header->bfReserved1 = 0;
    header->bfReserved2 = 0;
    header->bfOffBits = sizeof header[0];
    header->biSize = 40;
    header->biWidth = bw;
    header->biHeight = bh;
    header->biPlanes = 1;
    header->biBitCount = 24;
    header->biCompression = 0;
    header->biSizeImage = (uint32_t)(pitch * bh);
    header->biXPelsPerMeter = 0;
    header->biYPelsPerMeter = 0;
    header->biClrUsed = 0;
    header->biClrImportant = 0;

    bmp = (void*)&header[1];
    for (cy = 0; cy < rendered_frame->h; cy++)
    {
        uint32_t* con_row = rendered_frame->row[cy];
        uint8_t* col_row = rendered_frame->c_row[cy];
        uint32_t dy = cy * fv->glyph_height;
        for (cx = 0; cx < rendered_frame->w; cx++)
        {
            uint32_t code = con_row[cx];
            uint8_t color = col_row[cx];
            int font_x = (int)((code % fv->glyph_matrix_cols) * fv->glyph_width);
            int font_y = (int)((code / fv->glyph_matrix_cols) * fv->glyph_height);
            uint32_t dx = cx * fv->glyph_width;
            uint32_t x, y;
            for (y = 0; y < fv->glyph_height; y++)
            {
                uint32_t* src_row = fm->RowPointers[font_y + y];
                uint8_t* dst_row = &bmp[(size_t)(bh - 1 - (dy + y)) * pitch];
                for (x = 0; x < fv->glyph_width; x++)
                {
                    size_t index = (size_t)(dx + x) * 3;
                    union pixel
                    {
                        uint8_t u8[4];
                        uint32_t u32;
                    }*src_pixel = (void*)&src_row[font_x + x];
                    dst_row[index + 0] = (uint8_t)((uint32_t)src_pixel->u8[0] * Palette[color].u8[2] / 255);
                    dst_row[index + 1] = (uint8_t)((uint32_t)src_pixel->u8[1] * Palette[color].u8[1] / 255);
                    dst_row[index + 2] = (uint8_t)((uint32_t)src_pixel->u8[2] * Palette[color].u8[0] / 255);
                }
            }
        }
    }

    *out_file_content_buffer = buffer;
    *out_file_content_size = buf_size;
    return 1;
FailExit:
    free(buffer);
    if (out_file_content_buffer) *out_file_content_buffer = NULL;
    if (out_file_content_size) *out_file_content_size = 0;
    return 0;
}

static int create_rendered_image(fontvideo_p fv, fontvideo_frame_p rendered_frame, void** out_file_content_buffer, size_t* out_file_content_size, size_t* out_offset_of_file_body)
{
    if (fv->font_is_blackwhite)
    {
        if (!fv->do_colored_output)
        {
            return create_1bit_image(fv, rendered_frame, out_file_content_buffer, out_file_content_size, out_offset_of_file_body);
        }
        else
        {
            return create_4bit_image(fv, rendered_frame, out_file_content_buffer, out_file_content_size, out_offset_of_file_body);
        }
    }
    else
    {
        return create_24bit_image(fv, rendered_frame, out_file_content_buffer, out_file_content_size, out_offset_of_file_body);
    }
}


static void output_frame(fontvideo_p fv, fontvideo_frame_p f)
{
#pragma omp parallel sections
    {
#pragma omp section
        if (1)
        {
            uint32_t x, y;
            int done_output = 0;
            if (fv->do_colored_output)
            {
#ifdef _WIN32
                if (fv->do_old_console_output)
                {
                    if (!fv->old_console_buffer)
                    {
                        fv->old_console_buffer = calloc((size_t)fv->output_w * fv->output_h, sizeof(CHAR_INFO));
                        if (!fv->old_console_buffer)
                        {
                            fv->do_old_console_output = 0;
                            done_output = 0;
                        }
                    }
                    if (fv->old_console_buffer)
                    {
                        CHAR_INFO* ci = fv->old_console_buffer;
                        COORD BufferSize = { (SHORT)fv->output_w, (SHORT)fv->output_h };
                        COORD BufferCoord = { 0, 0 };
                        SMALL_RECT sr = { 0, 0, (SHORT)fv->output_w - 1, (SHORT)fv->output_h - 1 };
                        for (y = 0; y < f->h && y < fv->output_h; y++)
                        {
                            CHAR_INFO* dst_row = &ci[y * fv->output_w];
                            uint32_t* row = f->row[y];
                            uint8_t* c_row = f->c_row[y];
                            for (x = 0; x < f->w && x < fv->output_w; x++)
                            {
                                uint32_t Char = fv->glyph_codes[row[x]];
                                uint8_t Color = c_row[x];
                                WORD Attr =
                                    (Color & 0x01 ? FOREGROUND_RED : 0) |
                                    (Color & 0x02 ? FOREGROUND_GREEN : 0) |
                                    (Color & 0x04 ? FOREGROUND_BLUE : 0) |
                                    (Color & 0x08 ? FOREGROUND_INTENSITY : 0);
                                if (!Attr) Attr = FOREGROUND_INTENSITY;
                                dst_row[x].Attributes = Attr;
                                dst_row[x].Char.UnicodeChar = (WCHAR)Char;
                            }
                        }
                        done_output = WriteConsoleOutputW(GetStdHandle(STD_OUTPUT_HANDLE), ci, BufferSize, BufferCoord, &sr);
                    }
                }
#endif
                if (!done_output)
                {
                    char* u8chr = fv->utf8buf;
                    int cur_color = f->c_row[0][0];
                    memset(fv->utf8buf, 0, fv->utf8buf_size);
                    set_console_color(fv, cur_color);
                    u8chr = strchr(u8chr, 0);
                    for (y = 0; y < f->h; y++)
                    {
                        uint32_t* row = f->row[y];
                        uint8_t* c_row = f->c_row[y];
                        for (x = 0; x < f->w; x++)
                        {
                            int new_color = c_row[x];
                            uint32_t char_code = fv->glyph_codes[row[x]];
                            if (x == f->w - 1) char_code = '\n';
                            if (new_color != cur_color && char_code != ' ')
                            {
                                cur_color = new_color;
                                *u8chr = '\0';
                                set_console_color(fv, new_color);
                                u8chr = strchr(u8chr, 0);
                            }
                            u32toutf8(&u8chr, char_code);
                        }
                    }
                    set_console_color(fv, 7);// *u8chr = '\0';
                    fputs(fv->utf8buf, fv->graphics_out_fp);
                    done_output = 1;
                }
            }
            else
            {
                char* u8chr = fv->utf8buf;
                for (y = 0; y < (int)f->h; y++)
                {
                    uint32_t* row = f->row[y];
                    for (x = 0; x < (int)f->w; x++)
                    {
                        u32toutf8(&u8chr, fv->glyph_codes[row[x]]);
                    }
                    *u8chr++ = '\n';
                }
                *u8chr = '\0';
                fputs(fv->utf8buf, fv->graphics_out_fp);
            }
        }

#pragma omp section
        while (fv->output_frame_images_prefix || fv->output_avi_file)
        {
            char buf[4096];
            void* bmp_buf = NULL;
            size_t bmp_len = 0;
            size_t bmp_body_offset;
            BitmapInfoHeader_p BMIF = NULL;

            if (fv->output_frame_images_prefix)
            {
                snprintf(buf, sizeof buf, "%s%08u.bmp", fv->output_frame_images_prefix, f->index);
                FILE* fp = fopen(buf, "wb");
                if (!fp)
                {
                    fprintf(fv->log_fp, "Could not open '%s' for writting the current frame %u: %s.\n", buf, f->index, strerror(errno));
                    break;
                }
                if (create_rendered_image(fv, f, &bmp_buf, &bmp_len, &bmp_body_offset))
                {
                    BMIF = (BitmapInfoHeader_p)((size_t)bmp_buf + sizeof(BitmapFileHeader_t));
                    if (!fwrite(bmp_buf, bmp_len, 1, fp))
                    {
                        fprintf(fv->log_fp, "Could not write current frame %u to '%s': %s.\n", f->index, buf, strerror(errno));
                    }
                    if (BMIF->biBitCount < 8)
                    {
                        free(bmp_buf);
                        bmp_buf = NULL;
                        BMIF = NULL;
                    }
                }
                fclose(fp);
                fp = NULL;
            }

            if (fv->output_avi_file)
            {
                ensure_avi_writer(fv);
                if (fv->avi_writer)
                {
                    bmp_body_offset = 0;
                    if (!bmp_buf)
                    {
                        switch (fv->avi_writer->VideoFormat.biBitCount)
                        {
                        case 8: create_8bit_image(fv, f, &bmp_buf, &bmp_len, &bmp_body_offset); break;
                        case 24: create_24bit_image(fv, f, &bmp_buf, &bmp_len, &bmp_body_offset); break;
                        default: assert(0);
                        }
                    }
                    if (bmp_buf)
                    {
                        AVIWriterWriteVideo(fv->avi_writer, (void*)((size_t)bmp_buf + bmp_body_offset));
                    }
                }
            }
            free(bmp_buf);
            bmp_buf = NULL;
            break;
        }
    }
}

static int output_rendered_video(fontvideo_p fv, double timestamp)
{
    fontvideo_frame_p cur = NULL, next = NULL, prev = NULL;
    int discard_threshold = 0;

    if (!fv->frames) return 0;
    if (!fv->rendered_frame_count) return 0;

    if (atomic_exchange(&fv->doing_output, 1))
    {
        return 0;
    }

    lock_frame_linklist(fv);
    cur = fv->frames;
    if (!cur)
    {
        unlock_frame_linklist(fv);
        goto FailExit;
    }
    // Do real-time play with frameskip check
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

                // If output is too slow, skip frames.
                // If the next frame also need to output right now, skip the current frame
                if (!fv->no_frameskip)
                {
                    discard_threshold++;
                    if (discard_threshold >= discard_frame_num_threshold)
                    {
                        if (cur->rendered)
                        {
                            if (fv->verbose)
                            {
                                fprintf(fv->log_fp, "Discarding rendered frame %u due to lag. (%u)\n", cur->index, get_thread_id());
                            }

                            if (fv->frames == cur)
                            {
                                fv->frames = next;
                            }
                            else
                            {
                                if (!prev) prev = fv->frames;
                                prev->next = next;
                            }
                            frame_delete(&cur);
                            cur = next;
                            atomic_fetch_sub(&fv->precached_frame_count, 1);
                            atomic_fetch_sub(&fv->rendered_frame_count, 1);
                        }
                        else
                        {
                            atomic_int expected = 0;

                            // Acquire it to prevent it from being rendered
                            if (atomic_compare_exchange_strong(&cur->rendering, &expected, get_thread_id()))
                            {
                                fprintf(fv->log_fp, "Discarding frame %u to render due to lag. (%u)\n", cur->index, get_thread_id());

                                if (fv->frames == cur)
                                {
                                    fv->frames = next;
                                }
                                else
                                {
                                    if (!prev) prev = fv->frames;
                                    prev->next = next;
                                }
                                frame_delete(&cur);
                                cur = next;
                                atomic_fetch_sub(&fv->precached_frame_count, 1);
                            }
                            else
                            {
                                // It's acquired by a rendering thread, skip it.
                                prev = cur;
                                cur = next;
                            }
                        }
                    }
                }
                else
                {
                    break;
                }
            }
            else
            {
                // Timestamp not arrived to this frame
                break;
            }
        }
    }
    if (timestamp < 0 || timestamp >= cur->timestamp || !fv->real_time_play)
    {
        next = cur->next;
        unlock_frame_linklist(fv);
#if !defined(_DEBUG)
        if (fv->real_time_play)
        {
            set_cursor_xy(0, 0);
        }
#endif
        if (!cur->rendered)
        {
            unlock_frame_linklist(fv);
            goto FailExit;
        }

        output_frame(fv, cur);

        fprintf(fv->graphics_out_fp, "%u\n", cur->index);
        if (!fv->real_time_play) fprintf(stderr, "%u\r", cur->index);
        lock_frame_linklist(fv);
        fv->frames = next;
        if (!next)
        {
            fv->frame_last = NULL;
        }
        frame_delete(&cur);
        atomic_fetch_sub(&fv->precached_frame_count, 1);
        atomic_fetch_sub(&fv->rendered_frame_count, 1);
        fv->last_output_time = timestamp;
    }
    unlock_frame_linklist(fv);
    atomic_exchange(&fv->doing_output, 0);
    return 1;
FailExit:
    atomic_exchange(&fv->doing_output, 0);
    return 0;
}

static int decode_frames(fontvideo_p fv, int max_precache_frame_count)
{
    int ret = 0;
    if (fv->tailed) return 0;
    
    if (atomic_exchange(&fv->doing_decoding, 1)) return 0;

    for (;!fv->tailed;)
    {
        double target_timestamp = rttimer_gettime(&fv->tmr) + fv->precache_seconds;
        if (fv->verbose)
        {
            fprintf(fv->log_fp, "Decoding frames. (%u)\n", get_thread_id());
        }
        if (fv->real_time_play)
        {
            int should_break_loop = 0;
            lock_frame_linklist(fv);
            if (fv->frame_last && fv->frame_last->timestamp > target_timestamp)
                should_break_loop = 1;
            unlock_frame_linklist(fv);
#ifndef FONTVIDEO_NO_SOUND
            if (fv->do_audio_output)
            {
                lock_audio_linklist(fv);
                if (!fv->audio_last || fv->audio_last->timestamp - fv->audio_last->duration - fv->precache_seconds <= target_timestamp) should_break_loop = 0;
                unlock_audio_linklist(fv);
            }
#endif
            if (should_break_loop)
                break;
        }
        if (fv->precached_frame_count >= (uint32_t)max_precache_frame_count) break;

        ret = 1;
        fv->tailed = !avdec_decode(fv->av, fv_on_get_video, fv_on_get_audio, avt_for_both);
    }
    atomic_store(&fv->doing_decoding, 0);
    if (fv->tailed && fv->verbose)
    {
        fprintf(fv->log_fp, "All frames decoded. (%u)\n", get_thread_id());
    }
    return ret;
}

static int fv_set_output_resolution(fontvideo_p fv, uint32_t x_resolution, uint32_t y_resolution);

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
)
{
    fontvideo_p fv = NULL;

    fv = calloc(1, sizeof fv[0]);
    if (!fv) return fv;
    fv->log_fp = log_fp;
#ifdef _DEBUG
    // log_verbose = 1;
#endif
    fv->verbose = log_verbose;
    fv->verbose_threading = log_verbose_threading;
    fv->graphics_out_fp = graphics_out_fp;
    fv->do_audio_output = do_audio_output;
    fv->white_background = white_background;

    if (graphics_out_fp == stdout || graphics_out_fp == stderr)
    {
        fv->do_colored_output = 1;
        fv->real_time_play = 1;
    }

    fv->av = avdec_open(input_file, stderr);
    if (!fv->av) goto FailExit;
    fv->av->userdata = fv;
    if (!fv->av->audio_codec_context)
    {
        fv->do_audio_output = 0;
    }

    fv->precache_seconds = precache_seconds;

    if (!load_font(fv, "assets", assets_meta_file)) goto FailExit;
    if (fv->need_chcp || fv->real_time_play || log_fp == stdout || log_fp == stderr)
    {
        init_console(fv);
    }

    if (!y_resolution) y_resolution = 24;
    if (!x_resolution || !no_auto_aspect_adjust)
    {
        uint32_t desired_x;
        int src_w, src_h;
        src_w = fv->av->decoded_vf.width;
        src_h = fv->av->decoded_vf.height;
        desired_x = y_resolution * 2 * src_w / src_h;
        desired_x += desired_x & 1;
        if (!desired_x) desired_x = 2;
        if (!x_resolution || desired_x < x_resolution) x_resolution = desired_x;
        else if (desired_x > x_resolution)
        {
            uint32_t desired_y;
            desired_x = x_resolution;
            desired_y = (x_resolution * src_h / src_w) / 2;
            if (!desired_y) desired_y = 1;
            y_resolution = desired_y;
        }
    }
    if (!fv_set_output_resolution(fv, x_resolution, y_resolution)) goto FailExit;

    if (fv->do_audio_output || fv->output_avi_file)
    {
        avdec_audio_format_t af = { 0 };
        af.num_channels = fv->av->decoded_af.num_channels;
        af.sample_rate = fv->av->decoded_af.sample_rate;
        af.sample_fmt = AV_SAMPLE_FMT_FLT;
        af.bit_rate = (int64_t)af.sample_rate * af.num_channels * 32;
        avdec_set_decoded_audio_format(fv->av, &af);
    }

    if (start_timestamp < 0) start_timestamp = 0;
    else avdec_seek(fv->av, start_timestamp);

    rttimer_init(&fv->tmr, 1);
    rttimer_settime(&fv->tmr, start_timestamp);

    return fv;
FailExit:
    fv_destroy(fv);
    return NULL;
}

int fv_set_output_resolution(fontvideo_p fv, uint32_t x_resolution, uint32_t y_resolution)
{
    avdec_video_format_t vf = { 0 };

    fv->output_w = x_resolution;
    fv->output_h = y_resolution;

    // Wide-glyph detect
    if (fv->glyph_width > fv->glyph_height / 2)
    {
        fv->font_is_wide = 1;
        fv->output_w /= 2;
    }

    free(fv->utf8buf);
    fv->utf8buf_size = (size_t)fv->output_w * fv->output_h * 32 + 1;
    fv->utf8buf = malloc(fv->utf8buf_size);
    if (!fv->utf8buf) goto FailExit;

    vf.width = fv->output_w * fv->glyph_width;
    vf.height = fv->output_h * fv->glyph_height;
    vf.pixel_format = AV_PIX_FMT_BGRA;
    avdec_set_decoded_video_format(fv->av, &vf);
    return 1;
FailExit:
    return 0;
}

int fv_show_prepare(fontvideo_p fv)
{
    int mt = 1;
    rttimer_t tmr;
    char *bar_buf = NULL;
    int bar_len;

    if (!fv) return 0;

    if (fv->prepared) return 1;

    fprintf(fv->log_fp, "Pre-rendering frames.\n");

    if (fv->real_time_play)
    {
        rttimer_init(&tmr, 0);
        bar_len = fv->output_w * (1 + fv->font_is_wide) - 1;
        bar_buf = calloc((size_t)bar_len + 1, 1);
        decode_frames(fv, -1);
        while (fv->rendered_frame_count + fv->rendering_frame_count < fv->precached_frame_count)
        {
            if (mt)
            {
#pragma omp parallel
                for (;;)
                {
                    if (!get_frame_and_render(fv)) break;
                }
            }
            else
            {
                get_frame_and_render(fv);
            }
            if (bar_buf && rttimer_gettime(&tmr) >= 1.0)
            {
                double progress = (double)fv->rendered_frame_count / fv->precached_frame_count;
                int pos = (int)(progress * bar_len);
                int i;
                if (pos < 0) pos = 0; else if (pos > bar_len) pos = bar_len;
                for (i = 0; i < pos; i++) bar_buf[i] = '>';
                for (; i < bar_len; i++) bar_buf[i] = '=';
                fprintf(fv->log_fp, "%s\r", bar_buf);
                rttimer_settime(&tmr, 0.0);
            }
        }
        while (fv->rendering_frame_count);
        if (bar_buf)
        {
            int i;
            for (i = 0; i < bar_len; i++) bar_buf[i] = '>';
            fprintf(fv->log_fp, "%s\r", bar_buf);
            free(bar_buf); bar_buf = NULL;
        }

        rttimer_start(&fv->tmr);

        if (fv->do_audio_output)
        {
#ifndef FONTVIDEO_NO_SOUND
            fv->sio = siowrap_create(fv->log_fp, SoundIoFormatFloat32NE, fv->av->decoded_af.sample_rate, fv_on_write_sample);
            if (!fv->sio) return 0;
            fv->sio->userdata = fv;
#else
            fprintf(fv->log_fp, "`FONTVIDEO_NO_SOUND` is set so no sound API is called.\n");
#endif
        }
    }

    fv->prepared = 1;
    return 1;
}

int fv_show(fontvideo_p fv)
{
    if (!fv) return 0;
    if (!fv->prepared) fv_show_prepare(fv);
    fv->real_time_play = 1;
    return fv_render(fv);
}

int fv_render(fontvideo_p fv)
{
#ifdef _OPENMP
    int run_mt = 1;
    int thread_count = omp_get_max_threads();
#else
    int run_mt = 0;
    int thread_count = 1;
#endif

    if (!fv) return 0;

    if (run_mt)
    {
        int bo = 0;
#pragma omp parallel
        for (;;)
        {
            if (0 ||
                output_rendered_video(fv, rttimer_gettime(&fv->tmr)) ||
                get_frame_and_render(fv) ? 1 : 0 ||
                decode_frames(fv, thread_count * 2) ||
                0)
            {
                bo = 0;
                continue;
            }
            else
            {
                backoff(&bo);
            }
            if (fv->tailed && !fv->frames) break;
        }
    }
    else
    {
        for (;;)
        {
            while (decode_frames(fv, 4));
            if (get_frame_and_render(fv)) output_rendered_video(fv, rttimer_gettime(&fv->tmr));
            if (fv->tailed && !fv->frames) break;
        }
    }
#ifndef FONTVIDEO_NO_SOUND
    if (fv->do_audio_output && fv->sio)
    {
        int bo = 0;
        while (fv->audios || fv->audio_last)
        {
            backoff(&bo);
        }
    }
#endif
    return 1;
}

void fv_destroy(fontvideo_p fv)
{
    if (!fv) return;

#ifndef FONTVIDEO_NO_SOUND
    siowrap_destroy(fv->sio);
#endif

    free(fv->glyph_vertical_array);
    free(fv->glyph_codes);
    free(fv->glyph_brightness);
    UB_Free(&fv->glyph_matrix);
    avdec_close(&fv->av);

    AVIWriterFinish(&fv->avi_writer);

    while (fv->frames)
    {
        fontvideo_frame_p next = fv->frames->next;
        frame_delete(&fv->frames);
        fv->frames = next;
    }

#ifndef FONTVIDEO_NO_SOUND
    while (fv->audios)
    {
        fontvideo_audio_p next = fv->audios->next;
        audio_delete(&fv->audios);
        fv->audios = next;
    }
#endif

#ifdef _WIN32
    free(fv->old_console_buffer);
#endif

    free(fv->utf8buf);
    free(fv);
}
