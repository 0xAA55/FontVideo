#include"fontvideo.h"

#ifdef FONTVIDEO_ALLOW_OPENGL
#include<GL/glew.h>
#endif

#ifdef _WIN32
#ifdef FONTVIDEO_ALLOW_OPENGL
#include<GL/wglew.h>
#endif
#include<Windows.h>
#endif

#ifdef FONTVIDEO_ALLOW_OPENGL
#include<GLFW/glfw3.h>
#endif

#include<stdlib.h>
#include<locale.h>
#include<assert.h>
#include<C_dict/dictcfg.h>
#include<omp.h>
#include<threads.h>

#include"utf.h"

#ifdef _DEBUG
#   define DEBUG_OUTPUT_TO_SCREEN 1
#   define DEBUG_BGRX(r, g, b) (0xff000000 | (uint8_t)(b) | ((uint32_t)((uint8_t)(g)) << 8) | ((uint32_t)((uint8_t)(r)) << 16))
#endif

#define DISCARD_FRAME_NUM_THRESHOLD 10

static const double sqrt_3 = 1.7320508075688772935274463415059;

static uint32_t get_thread_id()
{
    return (uint32_t)thrd_current();
}

#ifdef _WIN32
#define SUBDIR "\\"

#if defined(_DEBUG) && defined(DEBUG_OUTPUT_TO_SCREEN)
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

#ifdef FONTVIDEO_ALLOW_OPENGL
#include <GL/glxew.h>
#include <GL/glx.h>
#include <unistd.h>
#endif

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
    const int max_yield = 64;
    const int max_sleep_ms = 1000;
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
        if (!yield()) iter_counter[0] = max_iter + max_yield;
    }
    else
    {
        int random;
        int sleep_ms = 1 << (iter_counter[0] - max_iter - max_yield);
        if (iter_counter[0] < 0x7fffffff) iter_counter[0] ++;
#pragma omp critical
        random = rand();
        if (sleep_ms > max_sleep_ms) sleep_ms = max_sleep_ms;
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

#ifdef FONTVIDEO_ALLOW_OPENGL

static atomic_int GLFWInitialized = 0;

typedef struct opengl_data_struct
{
    GLuint PBO_src;
    GLuint src_texture;
    int src_width;
    int src_height;

    GLuint FBO_match;
    GLuint match_texture;
    int match_width;
    int match_height;
    GLuint match_shader;

    GLuint FBO_reduction;
    GLuint reduction_texture1;
    GLuint reduction_texture2;
    int reduction_width;
    int reduction_height;
    GLuint reduction_shader;
    GLuint PBO_reduction;
    size_t PBO_reduction_size;

    GLuint FBO_final;
    GLuint PBO_final;
    GLuint PBO_final_c;
    GLuint final_texture;
    GLuint final_texture_c;
    int final_width;
    int final_height;
    GLuint final_shader;

    GLuint font_matrix_texture;

    GLuint Quad_VAO;
    int output_info;
}opengl_data_t, *opengl_data_p;

static int output_shader_infolog(GLuint shader, const char *shader_type, FILE *compiler_output)
{
    GLint log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length)
    {
        char *log_info = calloc((size_t)log_length + 1, 1);
        if (log_info)
        {
            glGetInfoLogARB(shader, log_length, &log_length, log_info);
            fprintf(compiler_output, "Shader (%s) InfoLog: %s\n", shader_type, log_info);
            free(log_info);
            return 1;
        }
        return 0;
    }
    return 1;
}

static int output_program_infolog(GLuint program, FILE *compiler_output)
{
    GLint log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length)
    {
        char *log_info = calloc((size_t)log_length + 1, 1);
        if (log_info)
        {
            glGetInfoLogARB(program, log_length, &log_length, log_info);
            fprintf(compiler_output, "Program InfoLog: %s\n", log_info);
            free(log_info);
            return 1;
        }
        return 0;
    }
    return 1;
}

static GLuint create_shader_program(FILE *compiler_output, const char *vs_code, const char *gs_code, const char *fs_code)
{
    GLuint program = glCreateProgram();
    GLint success = 0;

    if (vs_code)
    {
        const char *codes[] = {vs_code};
        const GLint lengths[] = {(GLint)strlen(vs_code)};
        GLuint vs;

        vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, codes, lengths);
        glCompileShader(vs);
        output_shader_infolog(vs, "VS", compiler_output);
        glAttachShader(program, vs);
        glDeleteShader(vs);
    }

    if (gs_code)
    {
        const char *codes[] = {gs_code};
        const GLint lengths[] = {(GLint)strlen(gs_code)};
        GLuint gs;

        gs = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(gs, 1, codes, lengths);
        glCompileShader(gs);
        output_shader_infolog(gs, "GS", compiler_output);
        glAttachShader(program, gs);
        glDeleteShader(gs);
    }

    if (fs_code)
    {
        const char *codes[] = {fs_code};
        const GLint lengths[] = {(GLint)strlen(fs_code)};
        GLuint fs;

        fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, codes, lengths);
        glCompileShader(fs);
        output_shader_infolog(fs, "FS", compiler_output);
        glAttachShader(program, fs);
        glDeleteShader(fs);
    }

    glLinkProgram(program);
    
    output_program_infolog(program, compiler_output);

    glGetObjectParameterivARB(program, GL_LINK_STATUS, &success);
    if (!success) goto FailExit;

    return program;
FailExit:
    glDeleteProgram(program);
    return 0;
}

static void opengl_data_destroy(opengl_data_p gld)
{
    if (gld)
    {
        size_t i;

        GLuint BuffersToDelete[] =
        {
            gld->PBO_src,
            gld->PBO_reduction,
            gld->PBO_final,
            gld->PBO_final_c
        };

        GLuint TexturesToDelete[] =
        {
            gld->src_texture,
            gld->match_texture,
            gld->reduction_texture1,
            gld->reduction_texture2,
            gld->final_texture,
            gld->final_texture_c,
            gld->font_matrix_texture
        };

        GLuint FBOsToDelete[] =
        {
            gld->FBO_match,
            gld->FBO_reduction,
            gld->FBO_final
        };

        GLuint ShadersToDelete[] =
        {
            gld->match_shader,
            gld->reduction_shader,
            gld->final_shader
        };

        glDeleteBuffers(sizeof BuffersToDelete / sizeof BuffersToDelete[0], BuffersToDelete);
        glDeleteTextures(sizeof TexturesToDelete / sizeof TexturesToDelete[0], TexturesToDelete);
        glDeleteFramebuffers(sizeof FBOsToDelete / sizeof FBOsToDelete[0], FBOsToDelete);
        for (i = 0; i < sizeof ShadersToDelete / sizeof ShadersToDelete[0]; i++) glDeleteShader(ShadersToDelete[i]);
        glDeleteVertexArrays(1, &gld->Quad_VAO);

        free(gld);
    }
}

static const char *strOpenGLError(GLenum glerr)
{
    switch (glerr)
    {
    case GL_NO_ERROR: return "GL_NO_ERROR";
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
    case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
    default: return "Unknown OpenGL Error";
    }
}

#define OPENGL_ERROR_ASSERT() {err_code = glGetError(); if (err_code != GL_NO_ERROR) {err_line = __LINE__; goto OpenGLErrorExit; }}
static opengl_data_p opengl_data_create(fontvideo_p fv, int output_init_info)
{
    opengl_data_p gld = calloc(1, sizeof gld[0]);
    size_t size;
    void *MapPtr;
    GLuint quad_vbo = 0;
    GLuint pbo_font_matrix = 0;
    GLint match_width = 0;
    GLint match_height = 0;
    GLint max_tex_size;
    GLint max_renderbuffer_size;
    GLint Location;
    GLenum err_code = GL_NO_ERROR;
    int inst_matrix_width;
    int inst_matrix_height;
    int err_line = __LINE__;
    const int min_loop_count = 5;
    struct Quad_Vertex_Struct
    {
        float PosX;
        float PosY;
    }*Quad_Vertex = NULL;

    if (!gld) goto NoMemFailExit;

    gld->output_info = output_init_info;

    if (output_init_info)
    {
        char *VersionStr = (char *)glGetString(GL_VERSION);
        fprintf(fv->log_fp, "OpenGL version: %s.\n", VersionStr);
    }

    if (!GLEW_VERSION_3_0)
    {
        fprintf(fv->log_fp, "OpenGL 3.0 not supported, the minimum requirement not meet.\n");
        goto FailExit;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glGenFramebuffers(1, &gld->FBO_match);
    glGenFramebuffers(1, &gld->FBO_reduction);
    glGenFramebuffers(1, &gld->FBO_final);

    gld->src_width = fv->output_w * fv->glyph_width;
    gld->src_height = fv->output_h * fv->glyph_height;
    size = (size_t)gld->src_width * gld->src_height * 4;

    glGenTextures(1, &gld->src_texture);
    glBindTexture(GL_TEXTURE_2D, gld->src_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gld->src_width, gld->src_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);

    glGenBuffers(1, &gld->PBO_src);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gld->PBO_src);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STATIC_DRAW);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
    assert(max_tex_size >= 1024); OPENGL_ERROR_ASSERT();
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
    if (max_tex_size > max_renderbuffer_size) max_tex_size = max_renderbuffer_size;

    size = (size_t)fv->glyph_matrix->Width * fv->glyph_matrix->Height * 4;

    glGenBuffers(1, &pbo_font_matrix);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_font_matrix);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STATIC_DRAW);
    MapPtr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(MapPtr, fv->glyph_matrix->BitmapData, size);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glGenTextures(1, &gld->font_matrix_texture);
    glBindTexture(GL_TEXTURE_2D, gld->font_matrix_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fv->glyph_matrix->Width, fv->glyph_matrix->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDeleteBuffers(1, &pbo_font_matrix);

    match_width = fv->output_w;
    match_height = fv->output_h;
    inst_matrix_width = 1;
    inst_matrix_height = 1;
    size = (size_t)inst_matrix_width * inst_matrix_height;
    while (size < fv->num_glyph_codes)
    {
        int loop_count = (int)(fv->num_glyph_codes / size);
        if (loop_count < min_loop_count) break;
        if (match_width > match_height)
        {
            int new_height = match_height + fv->output_h * 2;
            if (new_height > max_tex_size) break;
            match_height = new_height;
            inst_matrix_height++;
        }
        else
        {
            int new_width = match_width + fv->output_w * 2;
            if (new_width > max_tex_size) break;
            match_width = new_width;
            inst_matrix_width++;
        }
        size = (size_t)inst_matrix_width * inst_matrix_height;
    }

    gld->match_width = match_width;
    gld->match_height = match_height;

    glGenTextures(1, &gld->match_texture);
    glBindTexture(GL_TEXTURE_2D, gld->match_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, gld->match_width, gld->match_height, 0, GL_RG, GL_FLOAT, 0);
    OPENGL_ERROR_ASSERT();

    gld->match_shader = create_shader_program(fv->log_fp,
        "#version 130\n"
        "in vec2 vPosition;"
        "void main()"
        "{"
        "   gl_Position = vec4(vPosition, 0.0, 1.0);"
        "}"
        ,
        NULL,
        "#version 130\n"
        "precision highp float;"
        "out vec2 Output;"
        "uniform ivec2 Resolution;"
        "uniform sampler2D FontMatrix;"
        "uniform int FontMatrixCodeCount;"
        "uniform ivec2 FontMatrixSize;"
        "uniform ivec2 GlyphSize;"
        "uniform ivec2 ConsoleSize;"
        "uniform sampler2D SrcColor;"
        "uniform int SrcInvert;"
        "void main()"
        "{"
        "    ivec2 FragCoord = ivec2(gl_FragCoord.xy);"
        "    ivec2 InstDim = Resolution / ConsoleSize;"
        "    ivec2 InstCoord = FragCoord / ConsoleSize;"
        "    ivec2 ConsoleCoord = FragCoord - (InstCoord * ConsoleSize);"
        "    int inst_count = InstDim.x * InstDim.y;"
        "    int InstID = InstCoord.x + InstCoord.y * InstDim.x;"
        "    if (InstID >= FontMatrixCodeCount) discard;"
        "    int BestGlyph = 0;"
        "    float BestScore = -99999.0;"
        "    ivec2 GraphicalCoord = ConsoleCoord * GlyphSize;"
        "    for(int i = 0; i < FontMatrixCodeCount; i += inst_count)"
        "    {"
        "        int Glyph = (i + InstID);"
        "        if (Glyph >= FontMatrixCodeCount) break;"
        "        float Score = 0.0;"
        "        float SrcMod = 0.0;"
        "        float GlyphMod = 0.0;"
        "        ivec2 GlyphPos = ivec2(Glyph % FontMatrixSize.x, Glyph / FontMatrixSize.x) * GlyphSize;"
        "        for(int y = 0; y < GlyphSize.y; y++)"
        "        {"
        "            for(int x = 0; x < GlyphSize.x; x++)"
        "            {"
        "                ivec2 xy = ivec2(x, y);"
        "                float SrcLum = length(texelFetch(SrcColor, GraphicalCoord + xy, 0).rgb);"
        "                float GlyphLum = length(texelFetch(FontMatrix, GlyphPos + xy, 0).rgb);"
        "                if (SrcInvert != 0) SrcLum = 1.0 - SrcLum;"
        "                SrcLum -= 0.5;"
        "                GlyphLum -= 0.5;"
        "                Score += SrcLum * GlyphLum;"
        "                SrcMod += SrcLum * SrcLum;"
        "                GlyphMod += GlyphLum * GlyphLum;"
        "            }"
        "        }"
        "        if (SrcMod >= 0.000001) Score /= SrcMod;"
        "        if (GlyphMod >= 0.000001) Score /= GlyphMod;"
        "        if (Score >= BestScore)"
        "        {"
        "            BestScore = Score;"
        "            BestGlyph = Glyph;"
        "        }"
        "    }"
        "    Output = vec2(BestScore, float(BestGlyph));"
        "}"
    );
    if (!gld->match_shader) goto FailExit;
    OPENGL_ERROR_ASSERT();

    if (output_init_info)
    {
        int inst_count = inst_matrix_width * inst_matrix_height;
        int loop_count = (int)(fv->num_glyph_codes / inst_count);
        fprintf(fv->log_fp, "OpenGL Renderer: match size: %d x %d.\n", match_width, match_height);
        fprintf(fv->log_fp, "Expected loop count '%d' for total code count %zu (%d x %d = %d tests run in a pass).\n", loop_count, fv->num_glyph_codes, inst_matrix_width, inst_matrix_height, inst_count);
    }

    gld->reduction_width = match_width;
    gld->reduction_height = match_height;

    glGenTextures(1, &gld->reduction_texture1);
    glBindTexture(GL_TEXTURE_2D, gld->reduction_texture1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, gld->reduction_width, gld->reduction_height, 0, GL_RG, GL_FLOAT, 0);
    OPENGL_ERROR_ASSERT();

    glGenTextures(1, &gld->reduction_texture2);
    glBindTexture(GL_TEXTURE_2D, gld->reduction_texture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, gld->reduction_width, gld->reduction_height, 0, GL_RG, GL_FLOAT, 0);
    OPENGL_ERROR_ASSERT();

    gld->reduction_shader = create_shader_program(fv->log_fp,
        "#version 130\n"
        "in vec2 vPosition;"
        "void main()"
        "{"
        "   gl_Position = vec4(vPosition, 0.0, 1.0);"
        "}"
        ,
        NULL,
        "#version 130\n"
        "precision highp float;"
        "out vec2 Output;"
        "uniform ivec2 Resolution;"
        "uniform ivec2 ConsoleSize;"
        "uniform sampler2D ReductionTex;"
        "uniform ivec2 ReductionSize;"
        "void main()"
        "{"
        "    ivec2 FragCoord = ivec2(gl_FragCoord.xy);"
        "    ivec2 InstCoord = FragCoord / ConsoleSize;"
        "    ivec2 ConsoleCoord = FragCoord - (InstCoord * ConsoleSize);"
        "    ivec2 SrcInstCoord = InstCoord * ReductionSize;"
        "    float BestScore = -9999999.9;"
        "    int BestGlyph = 0;"
        "    for(int y = 0; y < ReductionSize.y; y++)"
        "    {"
        "        for(int x = 0; x < ReductionSize.x; x++)"
        "        {"
        "            ivec2 xy = ivec2(x, y);"
        "            vec2 Data = texelFetch(ReductionTex, (SrcInstCoord + xy) * ConsoleSize + ConsoleCoord, 0).rg;"
        "            if (Data.x >= BestScore)"
        "            {"
        "                BestScore = Data.x;"
        "                BestGlyph = int(Data.y);"
        "            }"
        "        }"
        "    }"
        ""
        "    Output = vec2(BestScore, float(BestGlyph));"
        "}"
    );
    if (!gld->reduction_shader) goto FailExit;
    OPENGL_ERROR_ASSERT();

    gld->final_width = fv->output_w;
    gld->final_height = fv->output_h;
    size = (size_t)gld->final_width * gld->final_height;

    glGenTextures(1, &gld->final_texture);
    glBindTexture(GL_TEXTURE_2D, gld->final_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, gld->final_width, gld->final_height, 0, GL_RED_INTEGER, GL_INT, 0);
    OPENGL_ERROR_ASSERT();

    glGenTextures(1, &gld->final_texture_c);
    glBindTexture(GL_TEXTURE_2D, gld->final_texture_c);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8I, gld->final_width, gld->final_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 0);
    OPENGL_ERROR_ASSERT();

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenBuffers(1, &gld->PBO_final);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final);
    glBufferData(GL_PIXEL_PACK_BUFFER, size * 4, NULL, GL_STATIC_DRAW);
    OPENGL_ERROR_ASSERT();

    glGenBuffers(1, &gld->PBO_final_c);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final_c);
    glBufferData(GL_PIXEL_PACK_BUFFER, size, NULL, GL_STATIC_DRAW);
    OPENGL_ERROR_ASSERT();

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    gld->final_shader = create_shader_program(fv->log_fp,
        "#version 130\n"
        "in vec2 vPosition;"
        "void main()"
        "{"
        "   gl_Position = vec4(vPosition, 0.0, 1.0);"
        "}"
        ,
        NULL,
        "#version 130\n"
        "precision highp float;"
        "out int Output;"
        "out int Color;"
        "uniform ivec2 Resolution;"
        "uniform sampler2D ReductionTex;"
        "uniform ivec2 GlyphSize;"
        "uniform ivec2 ConsoleSize;"
        "uniform sampler2D SrcColor;"
        "uniform int SrcInvert;"
        "uniform vec3 Palette[16] = vec3[16]"
        "("
        "    vec3(0.0, 0.0, 0.0),"
        "    vec3(0.0, 0.0, 0.5),"
        "    vec3(0.0, 0.5, 0.0),"
        "    vec3(0.0, 0.5, 0.5),"
        "    vec3(0.5, 0.0, 0.0),"
        "    vec3(0.5, 0.0, 0.5),"
        "    vec3(0.5, 0.5, 0.0),"
        "    vec3(0.5, 0.5, 0.5),"
        "    vec3(0.3, 0.3, 0.3),"
        "    vec3(0.0, 0.0, 1.0),"
        "    vec3(0.0, 1.0, 0.0),"
        "    vec3(0.0, 1.0, 1.0),"
        "    vec3(1.0, 0.0, 0.0),"
        "    vec3(1.0, 0.0, 1.0),"
        "    vec3(1.0, 1.0, 0.0),"
        "    vec3(1.0, 1.0, 1.0)"
        ");"
        "void main()"
        "{"
        "    ivec2 FragCoord = ivec2(gl_FragCoord.xy);"
        "    vec2 Data = texelFetch(ReductionTex, FragCoord, 0).rg;"
        "    float BestScore = Data.x;"
        "    int BestGlyph = int(Data.y);"
        "    vec3 AvrColor = vec3(0);"
        "    ivec2 BaseCoord = FragCoord * GlyphSize;"
        "    int PixelCount = GlyphSize.x * GlyphSize.y;"
        "    for(int y = 0; y < GlyphSize.y; y++)"
        "    {"
        "        for(int x = 0; x < GlyphSize.x; x++)"
        "        {"
        "            ivec2 xy = ivec2(x, y);"
        "            AvrColor += texelFetch(SrcColor, BaseCoord + xy, 0).rgb;"
        "        }"
        "    }"
        "    AvrColor /= PixelCount;"
        "    if (SrcInvert != 0) AvrColor = vec3(1) - AvrColor;"
        "    Output = BestGlyph;"
        "    Color = 0;"
        "    float ColorMinScore = -9999999.9;"
        "    for(int i = 0; i < 16; i++)"
        "    {"
        "        vec3 TV1 = normalize(AvrColor - vec3(0.5));"
        "        vec3 TV2 = normalize(Palette[i].bgr - vec3(0.5));"
        "        float ColorScore = dot(TV1, TV2);"
        "        if (ColorScore >= ColorMinScore)"
        "        {"
        "            ColorMinScore = ColorScore;"
        "            Color = i;"
        "        }"
        "    }"
        "    if (Color == 0) Color = 8;"
        "}"
    );
    if (!gld->final_shader) goto FailExit;
    OPENGL_ERROR_ASSERT();

    glGenVertexArrays(1, &gld->Quad_VAO);
    glBindVertexArray(gld->Quad_VAO);

    size = 4 * sizeof Quad_Vertex[0];

    glGenBuffers(1, &quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
    Quad_Vertex = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    Quad_Vertex[0].PosX = -1; Quad_Vertex[0].PosY = -1;
    Quad_Vertex[1].PosX = 1; Quad_Vertex[1].PosY = -1;
    Quad_Vertex[2].PosX = -1; Quad_Vertex[2].PosY = 1;
    Quad_Vertex[3].PosX = 1; Quad_Vertex[3].PosY = 1;
    glUnmapBuffer(GL_ARRAY_BUFFER);
    OPENGL_ERROR_ASSERT();

    Location = glGetAttribLocation(gld->match_shader, "vPosition");
    if (Location >= 0)
    {
        glEnableVertexAttribArray(Location);
        glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)0);
    }

    Location = glGetAttribLocation(gld->reduction_shader, "vPosition");
    if (Location >= 0)
    {
        glEnableVertexAttribArray(Location);
        glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)0);
    }

    Location = glGetAttribLocation(gld->final_shader, "vPosition");
    if (Location >= 0)
    {
        glEnableVertexAttribArray(Location);
        glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glUseProgram(0);
    OPENGL_ERROR_ASSERT();

    return gld;
OpenGLErrorExit:
    do
    {
        fprintf(fv->log_fp, "OpenGL Renderer: OpenGL error occured (from line %d): %u (%s).\n", err_line, err_code, strOpenGLError(err_code));
        err_code = glGetError();
    } while (err_code != GL_NO_ERROR);
    goto FailExit;
NoMemFailExit:
    fprintf(fv->log_fp, "OpenGL Renderer: Could not initialize OpenGL data: %s.\n", strerror(errno));
    goto FailExit;
FailExit:
    opengl_data_destroy(gld);
    return NULL;
}

typedef struct glctx_struct
{
    fontvideo_p fv;
    GLFWwindow *window;
    atomic_int lock;
    opengl_data_p data;
}glctx_t, *glctx_p;

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error: (%d) %s\n", error, description);
}

static void glctx_Destroy(glctx_p ctx)
{
    if (ctx)
    {
        if (ctx->window) glfwDestroyWindow(ctx->window);
        free(ctx);

        if (atomic_fetch_sub(&GLFWInitialized, 1) == 1)
        {
            glfwTerminate();
        }
    }
}

static glctx_p glctx_Create(fontvideo_p fv)
{
    glctx_p ctx = NULL;

    if (!atomic_fetch_add(&GLFWInitialized, 1))
    {
        if (!glfwInit())
        {
            fprintf(fv->log_fp, "OpenGL Renderer: glfwInit() failed.\n");
            goto FailExit;
        }

        glfwSetErrorCallback(glfw_error_callback);
    }

    ctx = calloc(1, sizeof ctx[0]);
    if (!ctx)
    {
        goto FailExit;
    }
    ctx->fv = fv;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    ctx->window = glfwCreateWindow(640, 480, "", NULL, NULL);
    if (!ctx->window)
    {
        fprintf(fv->log_fp, "OpenGL Renderer: glfwCreateWindow() failed.\n");
    }

    glfwMakeContextCurrent(ctx->window);
    glewInit();
    glfwMakeContextCurrent(NULL);

    return ctx;
FailExit:
    glctx_Destroy(ctx);
    return NULL;
}

static void glctx_MakeCurrent(glctx_p ctx)
{
    fontvideo_p fv = ctx->fv;
    uint32_t readout = 0;
    uint32_t cur_id = get_thread_id();
    int bo = 0;
    while ((readout = atomic_exchange(&ctx->lock, cur_id)) != 0)
    {
        if (fv->verbose_threading)
        {
            fprintf(fv->log_fp, "OpenGL Context locked by %u.\n", readout);
        }
        backoff(&bo);
    }
    glfwMakeContextCurrent(ctx->window);
}

static void glctx_UnMakeCurrent(glctx_p ctx)
{
    glfwMakeContextCurrent(NULL);
    atomic_exchange(&ctx->lock, 0);
    glfwPollEvents();
}

static int glctx_IsAvailableNow(glctx_p ctx)
{
    return !ctx->lock;
}

static int glctx_TryMakeCurrent(glctx_p ctx)
{
    fontvideo_p fv = ctx->fv;
    uint32_t expected = 0;
    uint32_t cur_id = get_thread_id();
    if (!glctx_IsAvailableNow(ctx)) return 0;
    if (atomic_compare_exchange_strong(&ctx->lock, &expected, cur_id))
    {
        if (fv->verbose_threading)
        {
            fprintf(fv->log_fp, "OpenGL Context acquired by %u.\n", cur_id);
        }
        glfwMakeContextCurrent(ctx->window);
        return 1;
    }
    else
    {
        return 0;
    }
}

typedef struct opengl_renderer_struct
{
    size_t count;
    glctx_p *contexts;
}opengl_renderer_t, *opengl_renderer_p;

static void opengl_renderer_destroy(opengl_renderer_p r)
{
    size_t i;

    if (!r) return;
    for (i = 0; i < r->count; i++)
    {
        glctx_p ctx = r->contexts[i];
        if (ctx)
        {
            glctx_MakeCurrent(ctx);
            opengl_data_destroy(ctx->data);
            glctx_UnMakeCurrent(ctx);
            glctx_Destroy(ctx);
        }
    }
    free(r);
}

static opengl_renderer_p opengl_renderer_create(fontvideo_p fv, int opengl_threads)
{
    opengl_renderer_p r = NULL;
    int ctx_count = opengl_threads;
    int i, j;

    if (!ctx_count) ctx_count = omp_get_max_threads() / 4;
    if (!ctx_count) ctx_count = 1;

    r = malloc(sizeof r[0]);
    if (!r) goto NoMemFailExit;
    memset(r, 0, sizeof r[0]);

    r->count = ctx_count;
    r->contexts = calloc(ctx_count, sizeof r->contexts[0]);
    if (!r->contexts) goto NoMemFailExit;

#pragma omp parallel for
    for (i = 0; i < ctx_count; i++)
    {
        glctx_p ctx;
#pragma omp critical
        ctx = glctx_Create(fv);
        if (ctx)
        {
            glctx_MakeCurrent(ctx);
            ctx->data = opengl_data_create(fv, !i);
            glctx_UnMakeCurrent(ctx);
            if (ctx->data)
            {
                r->contexts[i] = ctx;
            }
            else
            {
                r->contexts[i] = NULL;
#pragma omp critical
                glctx_Destroy(ctx);
            }
        }
    }

    for (i = j = 0; i < ctx_count; i++)
    {
        glctx_p ctx = r->contexts[i];
        if (ctx) r->contexts[j++] = ctx;
    }ctx_count = j;
    if (!ctx_count)
    {
        fprintf(fv->log_fp, "OpenGL Renderer: None of the OpenGL contexts finished initializing.\n");
        goto FailExit;
    }
    if (r->count != ctx_count)
    {
        fprintf(fv->log_fp, "OpenGL Renderer: Only some of the OpenGL contexts (%d out of %zu) finished initializing.\n", ctx_count, r->count);
        r->count = ctx_count;
    }
    else
    {
        fprintf(fv->log_fp, "OpenGL Renderer: Working thread number: %d.\n", ctx_count);
    }

    return r;
NoMemFailExit:
    fprintf(fv->log_fp, "OpenGL Renderer: Failed to initialize: %s.\n", strerror(errno));
    goto FailExit;
FailExit:
    opengl_renderer_destroy(r);
    return NULL;
}

int fv_allow_opengl(fontvideo_p fv, int opengl_threads)
{
    fv->allow_opengl = 1;
    fv->opengl_renderer = opengl_renderer_create(fv, opengl_threads);
    if (!fv->opengl_renderer) goto FailExit;
    return 1;
FailExit:
    fprintf(fv->log_fp, "Giving up using OpenGL.\n");
    fv->allow_opengl = 0;
    return 0;
}

#else
int fv_allow_opengl(fontvideo_p fv, int opengl_threads)
{
    opengl_threads;
    fprintf(fv->log_fp, "Macro `FONTVIDEO_ALLOW_OPENGL` not defined when compiling, giving up using OpenGL.\n");
    return 0;
}
#endif

// For locking the frames link list of `fv->frames` and `fv->frame_last`, not for locking every individual frames
static void lock_frame(fontvideo_p fv)
{
    uint32_t readout = 0;
    uint32_t cur_id = get_thread_id();
    int bo = 0;
    while (
        // First, do a non-atomic test for a quick peek.
        ((readout = fv->frame_lock) != 0) ||

        // Then, perform the actual atomic operation for acquiring the lock.
        ((readout = atomic_exchange(&fv->frame_lock, cur_id)) != 0))
    {
        if (fv->verbose_threading)
        {
            fprintf(fv->log_fp, "Frame link list locked by %d. (%d)\n", readout, cur_id);
        }
        backoff(&bo);
    }
}

static void unlock_frame(fontvideo_p fv)
{
    atomic_exchange(&fv->frame_lock, 0);
}

#ifndef FONTVIDEO_NO_SOUND
// Same mechanism for locking audio link list.
static void lock_audio(fontvideo_p fv)
{
    uint32_t readout;
    uint32_t cur_id = get_thread_id();
    int bo = 0;
    while ((readout = atomic_exchange(&fv->audio_lock, cur_id)) != 0)
    {
        if (fv->verbose_threading)
        {
            fprintf(fv->log_fp, "Audio link list locked by %d. (%d)\n", readout, cur_id);
        }
        backoff(&bo);
    }
}

static void unlock_audio(fontvideo_p fv)
{
    atomic_exchange(&fv->audio_lock, 0);
}
#endif

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
    lock_audio(fv);
    while (fv->audios)
    {
        // Next audio piece in the link list
        fontvideo_audio_p next = fv->audios->next;

        // Current time
        double current_time = rttimer_gettime(&fv->tmr);

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

        // Do audio piece skip
        if (!fv->no_frameskip && next)
        {
            if (current_time > next->timestamp)
            {
                audio_delete(fv->audios);
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
#endif

static int sort_glyph_codes_by_luminance(fontvideo_p fv);

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
    
// #ifdef DEBUG_OUTPUT_TO_SCREEN
//     DebugRawBitmap(0, 0, fv->glyph_matrix->BitmapData, fv->glyph_matrix->Width, fv->glyph_matrix->Height);
// #endif

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
                if (fv->do_color_invert) gp_lum = 1.0f - gp_lum;
                lum_row[x] = gp_lum;
                glyph_brightness += gp_lum;
            }
        }
        glyph_brightness /= glyph_pixel_count;
        fv->glyph_brightness[si] = glyph_brightness;
    }

//#ifdef DEBUG_OUTPUT_TO_SCREEN
//    DebugRawBitmap(0, 0, fv->glyph_matrix->BitmapData, fv->glyph_matrix->Width, fv->glyph_matrix->Height);
//#endif

    free(font_raw_code);
    dict_delete(d_meta);
    return sort_glyph_codes_by_luminance(fv);
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

int sort_glyph_codes_by_luminance(fontvideo_p fv)
{
    ptrdiff_t i;
    code_lum_p cl_array = NULL;
    float* glyph_array = NULL;
    UniformBitmap_p sorted_gm = NULL;
    size_t num_glyph_codes;
    size_t glyph_pixel_count;

    num_glyph_codes = fv->num_glyph_codes;
    glyph_pixel_count = (size_t)fv->glyph_width * fv->glyph_height;
    
    cl_array = calloc(num_glyph_codes, sizeof cl_array[0]);
    glyph_array = calloc(num_glyph_codes * glyph_pixel_count, sizeof glyph_array[0]);
    sorted_gm = UB_CreateFromCopy(fv->glyph_matrix);
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
        uint32_t gm_sx, gm_sy, gm_dx, gm_dy, y;

        j = cl->index;
        fv->glyph_codes[i] = cl->code;
        fv->glyph_brightness[i] = cl->lum;

        normalize_glyph(&glyph_array[glyph_pixel_count * i], &fv->glyph_vertical_array[glyph_pixel_count * j], glyph_pixel_count);

        // Sort the glyph matrix as well
        gm_sx = j % fv->glyph_matrix_cols;
        gm_sy = j / fv->glyph_matrix_cols;
        gm_dx = i % fv->glyph_matrix_cols;
        gm_dy = i / fv->glyph_matrix_cols;

        gm_sx *= fv->glyph_width;
        gm_sy *= fv->glyph_height;
        gm_dx *= fv->glyph_width;
        gm_dy *= fv->glyph_height;

        for (y = 0; y < fv->glyph_height; y++)
        {
            // uint32_t x;
            // for (x = 0; x < fv->glyph_width; x++)
            // {
            //     uint8_t c = 255 * (glyph_array[glyph_pixel_count * i + y * fv->glyph_width + x]);
            //     sorted_gm->RowPointers[gm_dy + y][gm_dx + x] = DEBUG_BGRX(c, c, c);
            // }
            memcpy(&sorted_gm->RowPointers[gm_dy + y][gm_dx], &fv->glyph_matrix->RowPointers[gm_sy + y][gm_sx], fv->glyph_width * sizeof(uint32_t));
        }
    }

    fv->glyph_brightness_min = fv->glyph_brightness[0];
    fv->glyph_brightness_max = fv->glyph_brightness[num_glyph_codes - 1];

    UB_Free(&fv->glyph_matrix);
    fv->glyph_matrix = sorted_gm;
    free(fv->glyph_vertical_array);
    fv->glyph_vertical_array = glyph_array;
    free(cl_array);

#ifdef DEBUG_OUTPUT_TO_SCREEN
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
        double in_val = in[i] - 0.5;
        lengthsq += in_val * in_val;
    }

    length = sqrt(lengthsq);
    if (length < 0.000001) length = 1.0;
    
    for (i = 0; i < glyph_pixel_count; i++)
    {
        double in_val = in[i] - 0.5;
        out[i] = (float)(in_val / length);
    }
}

// Create a frame, create buffers, copy the source bitmap, preparing for the rendering
static fontvideo_frame_p frame_create(uint32_t w, uint32_t h, double timestamp, void *bitmap, int bmp_width, int bmp_height, size_t bmp_pitch)
{
    ptrdiff_t i;
    fontvideo_frame_p f = calloc(1, sizeof f[0]);
    if (!f) return f;

    f->timestamp = timestamp;
    f->w = w;
    f->h = h;

    // Char code buffer
    f->data = calloc((size_t)w * h, sizeof f->data[0]); if (!f->data) goto FailExit;
    f->row = calloc(h, sizeof f->row[0]); if (!f->row) goto FailExit;

    // Char color buffer
    f->c_data = calloc((size_t)w * h, sizeof f->c_data[0]); if (!f->c_data) goto FailExit;
    f->c_row = calloc(h, sizeof f->c_row[0]); if (!f->c_row) goto FailExit;

    // Create row pointers for faster access to the matrix slots
    for (i = 0; i < (ptrdiff_t)h; i++)
    {
        f->row[i] = &f->data[i * w];
        f->c_row[i] = &f->c_data[i * w];
    }

    // Source bitmap buffer
    f->raw_w = bmp_width;
    f->raw_h = bmp_height;
    f->raw_data = calloc(bmp_height, bmp_pitch); if (!f->raw_data) goto FailExit;
    f->raw_data_row = calloc(bmp_height, sizeof f->raw_data_row[0]); if (!f->raw_data_row) goto FailExit;

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
    }palette[16];
    static int palette_initialized = 0;

    fw = f->w;
    fh = f->h;

    if (!palette_initialized)
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
            if (length >= 0.000001f)
            {
                palette[i].rgb.r /= length;
                palette[i].rgb.g /= length;
                palette[i].rgb.b /= length;
            }
        }
        palette_initialized = 1;
    }

    if (fv->normalize_input) frame_normalize_input(f);

    // OpenMP will automatically disable recursive multi-threading
#pragma omp parallel for
    for (fy = 0; fy < fh; fy++)
    {
        int fx, sx, sy;
        uint32_t *row = f->row[fy];
        uint8_t *c_row = f->c_row[fy];
        float *src_lum_buffer = malloc(glyph_pixel_count * sizeof src_lum_buffer[0]); // Monochrome image buffer of a single char in a frame
        sy = fy * fv->glyph_height;

        for (fx = 0; fx < fw; fx++)
        {
            int x, y;
            float avr_r = 0, avr_g = 0, avr_b = 0;
            int32_t cur_code_index = 0;
            int32_t best_code_index = 0;
            int32_t start_code_position = 0;
            int32_t end_code_position = 0;
            double src_normalize = 0;
            double src_brightness = 0;
            double best_score = -9999999.9f;
            uint8_t col = 0;
            sx = fx * fv->glyph_width;

            // Compute source luminance for further use
            for (y = 0; y < (int)fv->glyph_height; y++)
            {
                uint32_t *raw_row = f->raw_data_row[sy + y];
                float *buf_row = &src_lum_buffer[y * fv->glyph_width];
                for (x = 0; x < (int)fv->glyph_width; x++)
                {
                    union pixel
                    {
                        uint8_t u8[4];
                        uint32_t u32;
                    }   *src_pixel = (void *)&(raw_row[sx + x]);
                    float src_r, src_g, src_b;
                    float src_lum;
                    if (fv->do_color_invert) src_pixel->u32 ^= 0xffffff;
                    src_r = (float)src_pixel->u8[0] / 255.0f;
                    src_g = (float)src_pixel->u8[1] / 255.0f;
                    src_b = (float)src_pixel->u8[2] / 255.0f;
                    src_lum = (float)(sqrt(src_r * src_r + src_g * src_g + src_b * src_b) / sqrt_3);
                    src_brightness += (double)src_lum; // Get brightness
                    src_lum -= 0.5f; // Bias for convolutional
                    buf_row[x] = src_lum; // Store for normalize
                    src_normalize += (double)src_lum * src_lum; // LengthSq
                }
            }
            src_brightness /= glyph_pixel_count;
            src_normalize = sqrt(src_normalize);
            if (src_normalize < 0.000001) src_normalize = 1;

            // Instead of clamping the brightness, we scale the src_brightness to fit the capacity.
            src_brightness = fv->glyph_brightness_min + src_brightness * (fv->glyph_brightness_max - fv->glyph_brightness_min);
            if (1)
            {
                int32_t half_window = fv->candidate_glyph_window_size / 2 + 1;
                int32_t
                    left = 0,
                    right = (int32_t)(num_glyph_codes - 1),
                    mid = (int32_t)(num_glyph_codes / 2);
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
                        start_code_position = left - half_window;
                        end_code_position = start_code_position + fv->candidate_glyph_window_size - 1;
                        break;
                    }
                    mid = (right + left) / 2;
                    if (mid == left)
                    {
                        start_code_position = left - half_window;
                        end_code_position = start_code_position + fv->candidate_glyph_window_size - 1;
                        break;
                    }
                }
            }

            if (start_code_position < 0) start_code_position = 0;
            if (end_code_position < start_code_position) end_code_position = start_code_position;
            if (end_code_position > num_glyph_codes - 1) end_code_position = (int32_t)num_glyph_codes - 1;
            if (start_code_position > end_code_position) start_code_position = end_code_position;
            for (cur_code_index = start_code_position; cur_code_index <= end_code_position; cur_code_index++)
            {
                double score = 0;
                float *font_lum_img = &fv->glyph_vertical_array[cur_code_index * glyph_pixel_count];

                // Compare each pixels and collect the scores.
                for (y = 0; y < (int)fv->glyph_height; y++)
                {
                    size_t row_start = (size_t)y * fv->glyph_width;
                    float *buf_row = &src_lum_buffer[row_start];
                    float *font_row = &font_lum_img[row_start];
                    for (x = 0; x < (int)fv->glyph_width; x++)
                    {
                        double src_lum = buf_row[x] / src_normalize;
                        float font_lum = font_row[x];
                        score += src_lum * font_lum;
                    }
                }

                if (score >= best_score)
                {
                    best_score = score;
                    best_code_index = cur_code_index;
                }
            }

            // The best matching char code
            // row[fx] = fv->glyph_codes[best_code_index];
            row[fx] = best_code_index;

            // Get the average color of the char from the source frame
            for (y = 0; y < (int)fv->glyph_height; y++)
            {
                uint32_t *raw_row = f->raw_data_row[sy + y];
#ifdef DEBUG_OUTPUT_TO_SCREEN
                size_t row_start = (size_t)y * fv->glyph_width;
                float *font_row = &fv->glyph_vertical_array[(size_t)best_code_index * glyph_pixel_count + row_start];
                float *buf_row = &src_lum_buffer[y * fv->glyph_width];
#endif
                for (x = 0; x < (int)fv->glyph_width; x++)
                {
                    union pixel
                    {
                        uint8_t u8[4];
                        uint32_t u32;
                    }   *src_pixel = (void *)&(raw_row[sx + x]);

                    avr_b += (float)src_pixel->u8[0] / 255.0f;
                    avr_g += (float)src_pixel->u8[1] / 255.0f;
                    avr_r += (float)src_pixel->u8[2] / 255.0f;

#ifdef DEBUG_OUTPUT_TO_SCREEN
                    if (1)
                    {
                        // uint8_t uv = (uint8_t)((font_row[x] + 0.5) * 255.0f);
                        uint8_t uv = (uint8_t)(font_row[x] * 255.0f);
                        src_pixel->u8[0] = uv;
                        src_pixel->u8[1] = uv;
                        src_pixel->u8[2] = uv;
                    }
#endif
                }
            }

            avr_b /= glyph_pixel_count;
            avr_g /= glyph_pixel_count;
            avr_r /= glyph_pixel_count;
            // **The following code uses vector matching method to pickup best color**
            avr_b -= 0.5f;
            avr_g -= 0.5f;
            avr_r -= 0.5f;
            src_normalize = sqrt(avr_r * avr_r + avr_g * avr_g + avr_b * avr_b);
            if (src_normalize < 0.000001) src_normalize = 1;
            avr_b /= src_normalize;
            avr_g /= src_normalize;
            avr_r /= src_normalize;

            best_score = -9999999.9f;
            // best_score = 9999999.9f;
            col = 0;
            for (i = 0; i < 16; i++)
            {
                // **The following code uses vector matching method to pickup best color**
                float score =
                    avr_r * palette[i].rgb.r +
                    avr_g * palette[i].rgb.g +
                    avr_b * palette[i].rgb.b;
                if (score >= best_score)
                {
                    best_score = score;
                    col = (uint8_t)i;
                }
                // float
                //     dx = avr_r - palette[i].rgb.r,
                //     dy = avr_g - palette[i].rgb.g,
                //     dz = avr_b - palette[i].rgb.b;
                // float dist = dx * dx + dy * dy + dz * dz;
                // if (dist <= best_score)
                // {
                //     best_score = dist;
                //     col = (uint8_t)i;
                // }
            }

            // Avoid generating pure black characters.
            if (!col) col = 0x08;

            c_row[fx] = col;
        }

        free(src_lum_buffer);
    }
    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Finished CPU rendering a frame. (%u)\n", get_thread_id());
    }
}

#ifdef FONTVIDEO_ALLOW_OPENGL
static int do_opengl_render_command(fontvideo_p fv, fontvideo_frame_p f, opengl_data_p gld)
{
    GLint Location;
    size_t size;
    void *MapPtr;
    GLenum DrawBuffers[2] = {0};
    GLuint FBO_match = gld->FBO_match;
    GLuint FBO_reduction = gld->FBO_reduction;
    GLuint FBO_final = gld->FBO_final;
    GLuint src_texture = gld->src_texture;
    GLuint match_texture = gld->match_texture;
    GLuint final_texture = gld->final_texture;
    GLuint final_texture_c = gld->final_texture_c;
    GLuint reduction_tex1 = gld->reduction_texture1;
    GLuint reduction_tex2 = gld->reduction_texture2;
    GLenum err_code = GL_NO_ERROR;
    int reduction_src_width;
    int reduction_src_height;
    int reduction_dst_width;
    int reduction_dst_height;
    int reduction_pass = 0;
    int retval = 1;
    int err_line = __LINE__;

    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Start GPU rendering a frame. (%u)\n", get_thread_id());
    }

    if (fv->normalize_input) frame_normalize_input(f);

    // First step: issuing a rough matching with massive instances and a small number of glyphs to match.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_match);
    OPENGL_ERROR_ASSERT();

    size = (size_t)f->raw_w * f->raw_h * 4;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gld->PBO_src);
    MapPtr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(MapPtr, f->raw_data, size);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, src_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, f->raw_w, f->raw_h, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, match_texture, 0);
    DrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, DrawBuffers);
    glViewport(0, 0, gld->match_width, gld->match_height);
    glBindFragDataLocation(gld->match_shader, 0, "Output");

    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) goto FBONotCompleteExit;
    OPENGL_ERROR_ASSERT();

    glUseProgram(gld->match_shader);

    Location = glGetUniformLocation(gld->match_shader, "Resolution");
    if (Location >= 0) glUniform2i(Location, gld->match_width, gld->match_height);

    Location = glGetUniformLocation(gld->match_shader, "FontMatrix");
    if (Location >= 0)
    {
        glActiveTexture(GL_TEXTURE0 + 0); glBindTexture(GL_TEXTURE_2D, gld->font_matrix_texture);
        glUniform1i(Location, 0);
    }

    Location = glGetUniformLocation(gld->match_shader, "FontMatrixCodeCount");
    if (Location >= 0) glUniform1i(Location, (GLint)fv->num_glyph_codes);

    Location = glGetUniformLocation(gld->match_shader, "FontMatrixSize");
    if (Location >= 0) glUniform2i(Location, fv->glyph_matrix_cols, fv->glyph_matrix_rows);

    Location = glGetUniformLocation(gld->match_shader, "GlyphSize");
    if (Location >= 0) glUniform2i(Location, fv->glyph_width, fv->glyph_height);

    Location = glGetUniformLocation(gld->match_shader, "ConsoleSize");
    if (Location >= 0) glUniform2i(Location, fv->output_w, fv->output_h);

    Location = glGetUniformLocation(gld->match_shader, "SrcColor");
    if (Location >= 0)
    {
        glActiveTexture(GL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, gld->src_texture);
        glUniform1i(Location, 1);
    }

    Location = glGetUniformLocation(gld->match_shader, "SrcInvert");
    if (Location >= 0) glUniform1i(Location, fv->do_color_invert);

    glBindVertexArray(gld->Quad_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    OPENGL_ERROR_ASSERT();

    // Second step: merge instances for filtering the highest scores.
    reduction_tex1 = match_texture;
    reduction_tex2 = gld->reduction_texture1;
    reduction_src_width = gld->reduction_width;
    reduction_src_height = gld->reduction_height;
    reduction_pass = 0;
    while(1)
    {
        const int max_reduction_scale = 32;
        int src_instmat_w = 0;
        int src_instmat_h = 0;
        int dst_instmat_w = 0;
        int dst_instmat_h = 0;
        int reduction_x = 0;
        int reduction_y = 0;
        int i;

        reduction_pass++;

        reduction_dst_width = 0;
        reduction_dst_height = 0;

        src_instmat_w = reduction_src_width / gld->final_width;
        src_instmat_h = reduction_src_height / gld->final_height;

        if (src_instmat_w == 1 &&
            src_instmat_h == 1) break;

        if (src_instmat_w > 1)
        {
            for (i = 2; i < max_reduction_scale; i++)
            {
                if (!(src_instmat_w % i))
                {
                    reduction_x = i;
                    dst_instmat_w = src_instmat_w / i;
                    reduction_dst_width = dst_instmat_w * gld->final_width;
                    break;
                }
            }
        }
        if (!reduction_dst_width)
        {
            reduction_x = (reduction_src_width - 1) / gld->final_width + 1;
            dst_instmat_w = 1;
            reduction_dst_width = dst_instmat_w * gld->final_width;
        }

        if (src_instmat_h > 1)
        {
            for (i = 2; i < max_reduction_scale; i++)
            {
                if (!(src_instmat_h % i))
                {
                    reduction_y = i;
                    dst_instmat_h = src_instmat_h / i;
                    reduction_dst_height = dst_instmat_h * gld->final_height;
                    break;
                }
            }
        }
        if (!reduction_dst_height)
        {
            reduction_y = (reduction_src_height - 1) / gld->final_height + 1;
            dst_instmat_h = 1;
            reduction_dst_height = dst_instmat_h * gld->final_height;
        }

        if (fv->verbose && gld->output_info)
        {
            fprintf(fv->log_fp, "OpenGL Renderer: passing reduction %d from size %dx%d (%dx%d) to %dx%d (%dx%d) by %dx%d.\n",
                reduction_pass,
                reduction_src_width, reduction_src_height,
                src_instmat_w, src_instmat_h,
                reduction_dst_width, reduction_dst_height,
                dst_instmat_w, dst_instmat_h,
                reduction_x, reduction_y);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_reduction);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reduction_tex2, 0);
        DrawBuffers[0] = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, DrawBuffers);
        glViewport(0, 0, reduction_dst_width, reduction_dst_height);
        glBindFragDataLocation(gld->reduction_shader, 0, "Output");

        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) goto FBONotCompleteExit;

        glUseProgram(gld->reduction_shader);

        Location = glGetUniformLocation(gld->reduction_shader, "Resolution"); // assert(Location >= 0);
        glUniform2i(Location, reduction_dst_width, reduction_dst_height);

        Location = glGetUniformLocation(gld->reduction_shader, "ConsoleSize"); // assert(Location >= 0);
        glUniform2i(Location, fv->output_w, fv->output_h);

        Location = glGetUniformLocation(gld->reduction_shader, "ReductionTex"); // assert(Location >= 0);
        glActiveTexture(GL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, reduction_tex1);
        glUniform1i(Location, 1);

        Location = glGetUniformLocation(gld->reduction_shader, "ReductionSize"); // assert(Location >= 0);
        glUniform2i(Location, reduction_x, reduction_y);

        glBindVertexArray(gld->Quad_VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        OPENGL_ERROR_ASSERT();

        reduction_src_width = reduction_dst_width;
        reduction_src_height = reduction_dst_height;
        if (reduction_tex1 == match_texture)
        {
            reduction_tex1 = gld->reduction_texture1;
            reduction_tex2 = gld->reduction_texture2;
        }
        else
        {
            GLuint temp = reduction_tex1;
            reduction_tex1 = reduction_tex2;
            reduction_tex2 = temp;
        }
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_final);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, final_texture, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, final_texture_c, 0);
    DrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    DrawBuffers[1] = GL_COLOR_ATTACHMENT1;
    glDrawBuffers(2, DrawBuffers);
    glViewport(0, 0, gld->final_width, gld->final_height);
    glBindFragDataLocation(gld->final_shader, 0, "Output");
    glBindFragDataLocation(gld->final_shader, 1, "Color");

    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) goto FBONotCompleteExit;

    glUseProgram(gld->final_shader);

    Location = glGetUniformLocation(gld->final_shader, "Resolution");
    if (Location >= 0) glUniform2i(Location, gld->final_width, gld->final_height);

    Location = glGetUniformLocation(gld->final_shader, "ReductionTex");
    if (Location >= 0)
    {
        glActiveTexture(GL_TEXTURE0 + 2); glBindTexture(GL_TEXTURE_2D, reduction_tex1);
        glUniform1i(Location, 2);
    }

    Location = glGetUniformLocation(gld->final_shader, "GlyphSize");
    if (Location >= 0) glUniform2i(Location, fv->glyph_width, fv->glyph_height);

    Location = glGetUniformLocation(gld->final_shader, "ConsoleSize");
    if (Location >= 0) glUniform2i(Location, fv->output_w, fv->output_h);

    Location = glGetUniformLocation(gld->final_shader, "SrcColor");
    if (Location >= 0)
    {
        glActiveTexture(GL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, gld->src_texture);
        glUniform1i(Location, 1);
    }

    Location = glGetUniformLocation(gld->final_shader, "SrcInvert");
    if (Location >= 0) glUniform1i(Location, fv->do_color_invert);

    glBindVertexArray(gld->Quad_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, final_texture);
    size = (size_t)gld->final_width * gld->final_height * 4;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_INT, NULL);
    OPENGL_ERROR_ASSERT();

    MapPtr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    memcpy(f->data, MapPtr, size);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    OPENGL_ERROR_ASSERT();

    size = (size_t)gld->final_width * gld->final_height;
    retval = 0;
    if (size)
    {
        size_t i;
        for (i = 0; i < size; i++)
        {
            if (f->data[i] != 0)
            {
                retval = 1;
                break;
            }
        }
    }
    if (!retval)
    {
        goto BlackScreenFailExit;
    }

    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, final_texture_c);
    size = (size_t)gld->final_width * gld->final_height;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final_c);
    OPENGL_ERROR_ASSERT();
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    OPENGL_ERROR_ASSERT();

    MapPtr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    memcpy(f->c_data, MapPtr, size);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    OPENGL_ERROR_ASSERT();

    if (!retval) goto FailExit;

    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Finished GPU rendering a frame. (%u)\n", get_thread_id());
    }

    return retval;
OpenGLErrorExit:
    do
    {
        fprintf(fv->log_fp, "OpenGL Renderer: OpenGL error occured (from line %d): %u (%s).\n", err_line, err_code, strOpenGLError(err_code));
        err_code = glGetError();
    } while (err_code != GL_NO_ERROR);
    goto FailExit;
BlackScreenFailExit:
    fprintf(fv->log_fp, "OpenGL Renderer: Black screen output detected, this should be an OpenGL error.\n");
    goto FailExit;
FBONotCompleteExit:
    fprintf(fv->log_fp, "OpenGL Renderer: `Framebuffer not complete` occured.\n");
FailExit:
    return 0;
}

static void do_gpu_render(fontvideo_p fv, fontvideo_frame_p f)
{
    opengl_renderer_p r = NULL;
    glctx_p ctx = NULL;
    size_t i;

    r = fv->opengl_renderer;
    while (!ctx)
    {
        size_t ctx_cnt = 0;
        for (i = 0; i < r->count; i++)
        {
            glctx_p test = r->contexts[i];
            if (!test) continue;
            ctx_cnt++;
            if (glctx_TryMakeCurrent(test))
            {
                ctx = test;
                if (do_opengl_render_command(fv, f, ctx->data))
                {
                    glctx_UnMakeCurrent(ctx);
                    return;
                }
                else
                {
                    *(volatile glctx_p*)&(r->contexts[i]) = NULL;
                    opengl_data_destroy(ctx->data);
                    glctx_UnMakeCurrent(ctx);
                    glctx_Destroy(ctx);
                    ctx = NULL;
                    fprintf(fv->log_fp, "OpenGL Renderer failed to render the frame, delete the renderer to free memory.\n");
                }
            }
        }
        if (!ctx_cnt)
        {
#pragma omp critical
            {
                r = fv->opengl_renderer;
                fv->opengl_renderer = NULL;
                if (r)
                {
                    opengl_renderer_destroy(r);
                    fprintf(fv->log_fp, "None of the OpenGL Renderer is available, back to the CPU Renderer.\n");
                }
            }
            do_cpu_render(fv, f);
            return;
        }
        if (!ctx)
        {
            do_cpu_render(fv, f);
            return;
        }
    }
}

static void do_gpu_or_cpu_render(fontvideo_p fv, fontvideo_frame_p f)
{
    opengl_renderer_p r = NULL;
    size_t i;

    r = fv->opengl_renderer;
    if (r)
    {
        for (i = 0; i < r->count; i++)
        {
            glctx_p ctx = r->contexts[i];
            if (!ctx) continue;
            if (glctx_TryMakeCurrent(ctx))
            {
                if (do_opengl_render_command(fv, f, ctx->data))
                {
                    glctx_UnMakeCurrent(ctx);
                    return;
                }
                else
                {
                    *(volatile glctx_p*)&(r->contexts[i]) = NULL;
                    opengl_data_destroy(ctx->data);
                    glctx_UnMakeCurrent(ctx);
                    glctx_Destroy(ctx);
                    ctx = NULL;
                    fprintf(fv->log_fp, "OpenGL Renderer failed to render the frame, delete the renderer to free memory.\n");
                }
            }
        }
    }
    do_cpu_render(fv, f);
}
#endif

// Render the frame from bitmap into font char codes. One of the magic happens here.
static void render_frame_from_rgbabitmap(fontvideo_p fv, fontvideo_frame_p f)
{
    if (f->rendered) return;

    lock_frame(fv);
    fv->rendering_frame_count++;
    f->rendering_start_time = rttimer_gettime(&fv->tmr);
    unlock_frame(fv);

#ifdef FONTVIDEO_ALLOW_OPENGL
    if (fv->allow_opengl && fv->opengl_renderer)
    {
        if (fv->real_time_play) do_gpu_render(fv, f);
        else do_gpu_or_cpu_render(fv, f);
    }
    else
#endif
    do_cpu_render(fv, f);

    f->rendering_time_consuming = rttimer_gettime(&fv->tmr) - f->rendering_start_time;

    // Finished rendering, update statistics
    atomic_exchange(&f->rendered, get_thread_id());
    lock_frame(fv);
    fv->rendered_frame_count++;
    fv->rendering_frame_count--;
    unlock_frame(fv);

#ifdef DEBUG_OUTPUT_TO_SCREEN
    DebugRawBitmap(0, 0, f->raw_data, f->raw_w, f->raw_h);
#endif
}

// Lock the link list of the frames, insert the frame into the link list, preserve the ordering.
// The link list of the frames is the cached frames to be rendered.
static void locked_add_frame_to_fv(fontvideo_p fv, fontvideo_frame_p f)
{
    double timestamp;
    fontvideo_frame_p iter;

    lock_frame(fv);

    timestamp = f->timestamp;
    f->index = fv->frame_counter++;
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

#ifndef FONTVIDEO_NO_SOUND
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
    a->buffer = calloc(num_samples_per_channel * channel_count, sizeof a->buffer[0]);
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
#endif

static void fv_on_get_video(avdec_p av, void *bitmap, int width, int height, size_t pitch, double timestamp)
{
    fontvideo_p fv = av->userdata;
    fontvideo_frame_p f;

    f = frame_create(fv->output_w, fv->output_h, timestamp, bitmap, width, height, pitch);
    if (!f)
    {
        return;
    }
    locked_add_frame_to_fv(fv, f);
}

static void fv_on_get_audio(avdec_p av, void **samples_of_channel, int channel_count, size_t num_samples_per_channel, double timestamp)
{
#ifndef FONTVIDEO_NO_SOUND
    fontvideo_p fv = av->userdata;
    fontvideo_audio_p a = NULL;

    if (!fv->do_audio_output) return;
    
    a = audio_create(samples_of_channel[0], channel_count, num_samples_per_channel, timestamp);
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
    lock_frame(fv);
    if (fv->precached_frame_count <= fv->rendered_frame_count + fv->rendering_frame_count)
    {
        unlock_frame(fv);
        return 0;
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
                        fv->precached_frame_count--;
                        unlock_frame(fv);
                        return 0;
                    }
                    else
                    {
                        frame_delete(f);
                        f = fv->frames = next;
                        fv->precached_frame_count--;
                        continue;
                    }
                }
                else
                {
                    prev->next = next;
                    frame_delete(f);
                    f = next;
                    fv->precached_frame_count--;
                    continue;
                }
            }
            else
            {
                if (fv->verbose)
                {
                    fprintf(fv->log_fp, "Got frame to render. (%u)\n", get_thread_id());
                }
                unlock_frame(fv);
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
    unlock_frame(fv);
    return f ? 1 : 0;
}

static int create_rendered_image(fontvideo_p fv, fontvideo_frame_p rendered_frame, void ** out_file_content_buffer, size_t *out_file_content_size)
{
    void *buffer = NULL;
    size_t i;
    size_t buf_size = 0;
    size_t pitch;
    uint32_t cx, cy, bw, bh;
    uint8_t *bmp;
    UniformBitmap_p fm = fv->glyph_matrix;

    bw = fv->glyph_width * rendered_frame->w;
    bh = fv->glyph_height * rendered_frame->h;

    if (fv->font_is_blackwhite)
    {
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

        pitch = (((size_t)bw * 4 - 1) / 32 + 1) * 4;
        buf_size = sizeof header[0] + pitch * bh;
        buffer = malloc(buf_size);
        if (!buffer)
        {
            fprintf(fv->log_fp, "Could not allocate memory (%zu bytes) for images to be output: %s\n", buf_size, strerror(errno));
            goto FailExit;
        }
        memset(buffer, 0, buf_size);

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

        bmp = (void *)&header[1];
        for (cy = 0; cy < rendered_frame->h; cy++)
        {
            uint32_t *con_row = rendered_frame->row[cy];
            uint8_t *col_row = rendered_frame->c_row[cy];
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
                    uint32_t *src_row = fm->RowPointers[font_y + y];
                    uint8_t *dst_row = &bmp[(size_t)(bh - 1 - (dy + y)) * pitch];
                    for (x = 0; x < fv->glyph_width; x+=2)
                    {
                        size_t index = (size_t)(dx + x) >> 1;
                        dst_row[index] = 0;
                        if (src_row[font_x + x + 0] > 0xff7f7f7f) dst_row[index] |= color << 4;
                        if (src_row[font_x + x + 1] > 0xff7f7f7f) dst_row[index] |= color;
                    }
                }
            }
        }
    }
    else
    {
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
        } Palette[16];
#pragma pack(pop)

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

        bmp = (void *)&header[1];
        for (cy = 0; cy < rendered_frame->h; cy++)
        {
            uint32_t *con_row = rendered_frame->row[cy];
            uint8_t *col_row = rendered_frame->c_row[cy];
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
                    uint32_t *src_row = fm->RowPointers[font_y + y];
                    uint8_t *dst_row = &bmp[(size_t)(bh - 1 - (dy + y)) * pitch];
                    for (x = 0; x < fv->glyph_width; x ++)
                    {
                        size_t index = (size_t)(dx + x) * 3;
                        union pixel
                        {
                            uint8_t u8[4];
                            uint32_t u32;
                        }*src_pixel = (void *)&src_row[font_x + x];
                        dst_row[index + 0] = (uint8_t)((uint32_t)src_pixel->u8[0] * Palette[color].u8[2] / 255);
                        dst_row[index + 1] = (uint8_t)((uint32_t)src_pixel->u8[1] * Palette[color].u8[1] / 255);
                        dst_row[index + 2] = (uint8_t)((uint32_t)src_pixel->u8[2] * Palette[color].u8[0] / 255);
                    }
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

static int output_rendered_video(fontvideo_p fv, double timestamp)
{
    fontvideo_frame_p cur = NULL, next = NULL, prev = NULL;
    int cur_color = 0;
    int discard_threshold = 0;

    if (!fv->frames) return 0;
    if (!fv->rendered_frame_count) return 0;

    if (atomic_exchange(&fv->doing_output, 1))
    {
        return 0;
    }

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
            if (!fv->no_frameskip && timestamp >= cur->timestamp)
            {
                // Timestamp arrived
                if (timestamp < next->timestamp) break;

                // If the next frame also need to output right now, skip the current frame
                discard_threshold++;
                if (discard_threshold >= DISCARD_FRAME_NUM_THRESHOLD)
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
                            frame_delete(cur);
                            cur = next;
                            fv->precached_frame_count--;
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
                // Timestamp not arrived to this frame
                break;
            }
        }
    }
    if (timestamp < 0 || timestamp >= cur->timestamp || !fv->real_time_play)
    {
        uint32_t x, y;
        next = cur->next;
        unlock_frame(fv);
#if !defined(_DEBUG)
        if (fv->real_time_play)
        {
            set_cursor_xy(0, 0);
        }
#endif
        if (!cur->rendered)
        {
            unlock_frame(fv);
            goto FailExit;
        }
#pragma omp parallel sections
        {
#pragma omp section
            if (fv->do_colored_output)
            {
                int done_output = 0;
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
                        CHAR_INFO *ci = fv->old_console_buffer;
                        COORD BufferSize = {(SHORT)fv->output_w, (SHORT)fv->output_h};
                        COORD BufferCoord = {0, 0};
                        SMALL_RECT sr = {0, 0, (SHORT)fv->output_w - 1, (SHORT)fv->output_h - 1};
                        for (y = 0; y < cur->h && y < fv->output_h; y++)
                        {
                            CHAR_INFO *dst_row = &ci[y * fv->output_w];
                            uint32_t *row = cur->row[y];
                            uint8_t *c_row = cur->c_row[y];
                            for (x = 0; x < cur->w && x < fv->output_w; x++)
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
                    char *u8chr = fv->utf8buf;
                    cur_color = cur->c_row[0][0];
                    memset(fv->utf8buf, 0, fv->utf8buf_size);
                    set_console_color(fv, cur_color);
                    u8chr = strchr(u8chr, 0);
                    for (y = 0; y < cur->h; y++)
                    {
                        uint32_t *row = cur->row[y];
                        uint8_t *c_row = cur->c_row[y];
                        for (x = 0; x < cur->w; x++)
                        {
                            int new_color = c_row[x];
                            uint32_t char_code = fv->glyph_codes[row[x]];
                            if (x == cur->w - 1) char_code = '\n';
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
                    *u8chr = '\0';
                    fputs(fv->utf8buf, fv->graphics_out_fp);
                    done_output = 1;
                }
            }
            else
            {
                char *u8chr = fv->utf8buf;
                for (y = 0; y < (int)cur->h; y++)
                {
                    uint32_t *row = cur->row[y];
                    for (x = 0; x < (int)cur->w; x++)
                    {
                        u32toutf8(&u8chr, fv->glyph_codes[row[x]]);
                    }
                    *u8chr++ = '\n';
                }
                *u8chr = '\0';
                fputs(fv->utf8buf, fv->graphics_out_fp);
            }

#pragma omp section
            while (fv->output_frame_images_prefix)
            {
                char buf[4096];
                FILE *fp = NULL;
                void *bmp_buf = NULL;
                size_t bmp_len = 0;

                snprintf(buf, sizeof buf, "%s%08u.bmp", fv->output_frame_images_prefix, cur->index);
                fp = fopen(buf, "wb");
                if (!fp)
                {
                    fprintf(fv->log_fp, "Could not open '%s' for writting the current frame %u: %s.\n", buf, cur->index, strerror(errno));
                    break;
                }

                if (create_rendered_image(fv, cur, &bmp_buf, &bmp_len))
                {
                    if (!fwrite(bmp_buf, bmp_len, 1, fp))
                    {
                        fprintf(fv->log_fp, "Could not write current frame %u to '%s': %s.\n", cur->index, buf, strerror(errno));
                    }
                }

                free(bmp_buf);
                fclose(fp);
                break;
            }
        }

        fprintf(fv->graphics_out_fp, "%u\n", cur->index);
        if (!fv->real_time_play) fprintf(stderr, "%u\r", cur->index);
        lock_frame(fv);
        fv->frames = next;
        if (!next)
        {
            fv->frame_last = NULL;
        }
        frame_delete(cur);
        fv->precached_frame_count--;
        fv->rendered_frame_count--;
        fv->last_output_time = timestamp;
    }
    unlock_frame(fv);
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
            lock_frame(fv);
            if (fv->frame_last && fv->frame_last->timestamp > target_timestamp) should_break_loop = 1;
            unlock_frame(fv);
#ifndef FONTVIDEO_NO_SOUND
            if (fv->do_audio_output)
            {
                lock_audio(fv);
                if (!fv->audio_last || fv->audio_last->timestamp <= target_timestamp) should_break_loop = 0;
                unlock_audio(fv);
            }
#endif
            if (should_break_loop) break;
        }
        else
        {
            if (fv->precached_frame_count >= (uint32_t)max_precache_frame_count) break;
        }

        ret = 1;
        fv->tailed = !avdec_decode(fv->av, fv_on_get_video, fv->do_audio_output ? fv_on_get_audio : NULL);
    }
    atomic_exchange(&fv->doing_decoding, 0);
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
    int no_auto_aspect_adjust
)
{
    fontvideo_p fv = NULL;
    avdec_audio_format_t af = {0};

    fv = calloc(1, sizeof fv[0]);
    if (!fv) return fv;
    fv->log_fp = log_fp;
#ifdef _DEBUG
    log_verbose = 1;
#endif
    fv->verbose = log_verbose;
    fv->verbose_threading = log_verbose_threading;
    fv->graphics_out_fp = graphics_out_fp;
    fv->do_audio_output = do_audio_output;

    if (graphics_out_fp == stdout || graphics_out_fp == stderr)
    {
        fv->do_colored_output = 1;
        fv->real_time_play = 1;
    }

    fv->av = avdec_open(input_file, stderr);
    if (!fv->av) goto FailExit;
    fv->av->userdata = fv;

    fv->precache_seconds = precache_seconds;

    if (!load_font(fv, "assets", assets_meta_file)) goto FailExit;
    if (fv->need_chcp || fv->real_time_play || log_fp == stdout || log_fp == stderr)
    {
        init_console(fv);
    }

    if (!y_resolution) y_resolution = 24;
    if (!x_resolution || !no_auto_aspect_adjust)
    {
        avdec_video_format_t vf;
        uint32_t desired_x;
        avdec_get_video_format(fv->av, &vf);
        desired_x = y_resolution * 2 * vf.width / vf.height;
        desired_x += desired_x & 1;
        if (!desired_x) desired_x = 2;
        if (!x_resolution) x_resolution = desired_x;
        else if (desired_x > x_resolution)
        {
            uint32_t desired_y;
            desired_x = x_resolution;
            desired_y = (x_resolution * vf.height / vf.width) / 2;
            if (!desired_y) desired_y = 1;
            y_resolution = desired_y;
        }
    }
    if (!fv_set_output_resolution(fv, x_resolution, y_resolution)) goto FailExit;

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

#pragma omp parallel
    if (run_mt)
    {
        int bo = 0;
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
    if (fv->do_audio_output)
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

    while (fv->frames)
    {
        fontvideo_frame_p next = fv->frames->next;
        frame_delete(fv->frames);
        fv->frames = next;
    }

#ifndef FONTVIDEO_NO_SOUND
    while (fv->audios)
    {
        fontvideo_audio_p next = fv->audios->next;
        audio_delete(fv->audios);
        fv->audios = next;
    }
#endif

#ifdef _WIN32
    free(fv->old_console_buffer);
#endif

    free(fv->utf8buf);
    free(fv);
}
