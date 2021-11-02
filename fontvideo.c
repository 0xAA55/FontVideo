#include"fontvideo.h"

#ifdef FONTVIDEO_ALLOW_OPENGL
#include<GL/glew.h>
#endif

#include<stdlib.h>
#include<locale.h>
#include<assert.h>
#include<C_dict/dictcfg.h>
#include<omp.h>
#include<threads.h>

#include"utf.h"

#ifdef _DEBUG
#   define DEBUG_OUTPUT_TO_SCREEN 0
#endif

#ifdef _WIN32

#ifdef FONTVIDEO_ALLOW_OPENGL
#include<GL/wglew.h>
#endif

#include<Windows.h>

#define SUBDIR "\\"

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
        if (!SetConsoleMode(hConsole, ConMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            fv->do_old_console_output = 1;
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

static void yield()
{
    if (!SwitchToThread()) Sleep(1);
}

#ifdef FONTVIDEO_ALLOW_OPENGL
typedef struct glctx_struct
{
    fontvideo_p fv;
    HWND hWnd;
    HDC hDC;
    HGLRC hGLRC;
    atomic_int lock;
}glctx_t, *glctx_p;

static void glctx_Destroy(glctx_p ctx)
{
    if (ctx)
    {
        if (ctx->hGLRC) wglDeleteContext(ctx->hGLRC);
        if (ctx->hDC) ReleaseDC(ctx->hWnd, ctx->hDC);
        if (ctx->hWnd) DestroyWindow(ctx->hWnd);
        free(ctx);
    }
}

static LRESULT CALLBACK glctx_WndProc(HWND hWnd, UINT Msg, WPARAM wp, LPARAM lp)
{
    switch (Msg)
    {
    case WM_CREATE:
        if (1)
        {
            CREATESTRUCT *cs = (void *)lp;
            SetWindowLongPtr(hWnd, 0, (LONG_PTR)cs->lpCreateParams);
            return 0;
        }
    case WM_DESTROY:
        SetWindowLongPtr(hWnd, 0, 0);
        return 0;
    default:
        return DefWindowProc(hWnd, Msg, wp, lp);
    }
}

static glctx_p glctx_Create(fontvideo_p fv)
{
    glctx_p ctx = NULL;
    int PixelFormat;
    WNDCLASSW wc = {0};
    ATOM ca = 0;
    PIXELFORMATDESCRIPTOR pfd = {0};

    ctx = calloc(1, sizeof ctx[0]);
    if (!ctx)
    {
        goto FailExit;
    }
    ctx->fv = fv;

    wc.lpfnWndProc = glctx_WndProc;
    wc.cbWndExtra = sizeof ctx;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FontVideoOpenGLContextWindow";
    ca = RegisterClassW(&wc);
    ctx->hWnd = CreateWindowExW(0, (LPWSTR)(ca), L"FontVideo OpenGL Context Window", WS_DISABLED | WS_MAXIMIZE, 0, 0, 1919, 810, NULL, NULL, wc.hInstance, ctx);

    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;

    ctx->hDC = GetDC(ctx->hWnd);
    PixelFormat = ChoosePixelFormat(ctx->hDC, &pfd);
    if (!SetPixelFormat(ctx->hDC, PixelFormat, &pfd))
    {
        fprintf(fv->log_fp, "SetPixelFormat() failed: ");
        PrintLastError(fv->log_fp);
        goto FailExit;
    }

    ctx->hGLRC = wglCreateContext(ctx->hDC);
    if (!ctx->hGLRC)
    {
        fprintf(fv->log_fp, "wglCreateContext() failed: ");
        PrintLastError(fv->log_fp);
        goto FailExit;
    }

    return ctx;
FailExit:
    glctx_Destroy(ctx);
    return NULL;
}

static void glctx_MakeCurrent(glctx_p ctx)
{
    assert(ctx);
    while (atomic_exchange(&ctx->lock, 1)) yield();
    if (!wglMakeCurrent(ctx->hDC, ctx->hGLRC))
    {
        fprintf(ctx->fv->log_fp, "wglMakeCurrent() failed: ");
        PrintLastError(ctx->fv->log_fp);
    }
}

static void glctx_UnMakeCurrent(glctx_p ctx)
{
    wglMakeCurrent(NULL, NULL);
    atomic_exchange(&ctx->lock, 0);
}

#endif

#else // For non-Win32 appliction, assume it's linux/unix and runs in terminal

#ifdef FONTVIDEO_ALLOW_OPENGL
#include <GL/glxew.h>
#endif

#include <sched.h>

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

static void yield()
{
    sched_yield();
}

#ifdef FONTVIDEO_ALLOW_OPENGL
typedef struct glctx_struct
{
    int unused;
}glctx_t, *glctx_p;

static void glctx_Destroy(glctx_p ctx)
{
    free(ctx);
}

static glctx_p glctx_Create(fontvideo_p fv)
{
    fprintf(fv->log_fp, "Could not create OpenGL context, not implemented for this platform.\n");
    return NULL;
}

static void glctx_MakeCurrent(glctx_p ctx)
{
}

static void glctx_UnMakeCurrent(glctx_p ctx)
{
}
#endif

#endif

#ifdef FONTVIDEO_ALLOW_OPENGL
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
}opengl_data_t, *opengl_data_p;

static GLuint create_shader_program(FILE *compiler_output, const char *vs_code, const char *gs_code, const char *fs_code)
{
    GLuint program = glCreateProgram();
    GLint success = 0;
    GLint log_length;

    if (vs_code)
    {
        const char *codes[] = {vs_code};
        const GLint lengths[] = {(GLint)strlen(vs_code)};
        GLuint vs;

        vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, codes, lengths);
        glCompileShader(vs);
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
        glAttachShader(program, fs);
        glDeleteShader(fs);
    }

    glLinkProgram(program);
    
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length)
    {
        char *log_info = calloc((size_t)log_length + 1, 1);
        if (log_info)
        {
            glGetInfoLogARB(program, log_length, &log_length, log_info);
            fputs(log_info, compiler_output);
            free(log_info);
        }
    }

    glGetObjectParameterivARB(program, GL_LINK_STATUS, &success);
    if (!success) goto FailExit;

    return program;
FailExit:
    glDeleteProgram(program);
    return 0;
}

#endif

int fv_allow_opengl(fontvideo_p fv)
{
#ifndef FONTVIDEO_ALLOW_OPENGL
    fprintf(fv->log_fp, "Macro `FONTVIDEO_ALLOW_OPENGL` not defined when compiling, giving up using OpenGL.\n");
    return 0;
#else
    glctx_p glctx = NULL;
    opengl_data_p gld = NULL;
    size_t size;
    void *MapPtr;
    char *VersionStr = NULL;
    GLuint quad_vbo = 0;
    GLuint pbo_font_matrix = 0;
    GLint match_width = 0;
    GLint match_height = 0;
    GLint max_tex_size;
    GLint max_fbo_width;
    GLint max_fbo_height;
    GLint Location;
    int InstMatrixWidth;
    int InstMatrixHeight;
    struct Quad_Vertex_Struct
    {
        float PosX;
        float PosY;
    }*Quad_Vertex = NULL;

    fv->allow_opengl = 1;

    fv->opengl_context = glctx = glctx_Create(fv);
    if (!glctx) goto FailExit;

    fv->opengl_data = gld = calloc(1, sizeof gld[0]);
    if (!gld)
    {
        fprintf(fv->log_fp, "%s.\n", strerror(errno));
        goto FailExit;
    }

    glctx_MakeCurrent(glctx);
    glewInit();

    VersionStr = (char*)glGetString(GL_VERSION);
    fprintf(fv->log_fp, "OpenGL version: %s.\n", VersionStr);

    if (!GLEW_VERSION_3_3)
    {
        fprintf(fv->log_fp, "OpenGL 3.3 not supported, the minimum requirement not meet.\n");
        goto FailExit;
    }

    glGenFramebuffers(1, &gld->FBO_match);
    glGenFramebuffers(1, &gld->FBO_final);

    gld->src_width = fv->output_w * fv->font_w;
    gld->src_height = fv->output_w * fv->font_w;
    size = gld->src_width * gld->src_height * 4;

    glGenTextures(1, &gld->src_texture);
    glBindTexture(GL_TEXTURE_2D, gld->src_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gld->src_width, gld->src_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenBuffers(1, &gld->PBO_src);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gld->PBO_src);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STATIC_DRAW);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
    assert(max_tex_size >= 1024);
    glGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH, &max_fbo_width);
    glGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT, &max_fbo_height);
    if (max_tex_size > max_fbo_width) max_tex_size = max_fbo_width;
    if (max_tex_size > max_fbo_height) max_tex_size = max_fbo_height;

    size = (size_t)fv->font_matrix->Width * fv->font_matrix->Height * 4;

    glGenBuffers(1, &pbo_font_matrix);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_font_matrix);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STATIC_DRAW);
    MapPtr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(MapPtr, fv->font_matrix->BitmapData, size);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glGenTextures(1, &gld->font_matrix_texture);
    glBindTexture(GL_TEXTURE_2D, gld->font_matrix_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fv->font_matrix->Width, fv->font_matrix->Height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDeleteBuffers(1, &pbo_font_matrix);

    match_width = fv->output_w;
    match_height = fv->output_h;
    InstMatrixWidth = 1;
    InstMatrixHeight = 1;
    size = (size_t)InstMatrixWidth * InstMatrixHeight;
    while (size < fv->font_code_count)
    {
        if (match_width > match_height)
        {
            int new_height = match_height + fv->output_h;
            if (new_height > max_tex_size) break;
            match_height = new_height;
            InstMatrixHeight++;
        }
        else
        {
            int new_width = match_width + fv->output_w;
            if (new_width > max_tex_size) break;
            match_width = new_width;
            InstMatrixWidth++;
        }
        size = (size_t)InstMatrixWidth * InstMatrixHeight;
    }

    gld->match_width = match_width;
    gld->match_height = match_height;

    glGenTextures(1, &gld->match_texture);
    glBindTexture(GL_TEXTURE_2D, gld->match_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, gld->match_width, gld->match_height, 0, GL_RGBA, GL_FLOAT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    gld->match_shader = create_shader_program(fv->log_fp,
        "#version 130\n"
        "in vec2 Position;"
        "out vec2 TexCoord;"
        "void main()"
        "{"
        "   TexCoord = (Position + vec2(1)) * vec2(0.5);"
        "   gl_Position = vec4(Position, 0.0, 1.0);"
        "}"
        ,
        NULL,
        "#version 130\n"
        "precision highp float;"
        "in vec2 TexCoord;"
        "out vec4 Output;"
        "uniform ivec2 Resolution;"
        "uniform sampler2D FontMatrix;"
        "uniform int FontMatrixCodeCount;"
        "uniform ivec2 FontMatrixSize;"
        "uniform ivec2 GlyphSize;"
        "uniform ivec2 ConsoleSize;"
        "uniform sampler2D SrcColor;"
        "void main()"
        "{"
        "    ivec2 FragCoord = ivec2(TexCoord * vec2(Resolution));"
        "    ivec2 InstDim = Resolution / ConsoleSize;"
        "    ivec2 InstCoord = FragCoord / ConsoleSize;"
        "    ivec2 ConsoleCoord = FragCoord - (InstCoord * ConsoleSize);"
        "    int InstCount = InstDim.x * InstDim.y;"
        "    int InstID = InstCoord.x + InstCoord.y * InstDim.x;"
        "    if (InstID >= FontMatrixCodeCount) discard;"
        "    int BestGlyph = 0;"
        "    float BestScore = -99999.0;"
        "    ivec2 GraphicalCoord = ConsoleCoord * GlyphSize;"
        "    for(int i = 0; i < FontMatrixCodeCount; i += InstCount)"
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
        "                SrcLum -= 0.5;"
        "                GlyphLum -= 0.5;"
        "                Score += SrcLum * GlyphLum;"
        "                SrcMod += SrcLum * SrcLum;"
        "                GlyphMod += GlyphLum * GlyphLum;"
        "            }"
        "        }"
        "        if (SrcMod >= 0.000001) Score /= sqrt(SrcMod);"
        "        if (GlyphMod >= 0.000001) Score /= sqrt(GlyphMod);"
        "        if (Score >= BestScore)"
        "        {"
        "            BestScore = Score;"
        "            BestGlyph = Glyph;"
        "        }"
        "    }"
        "    Output = vec4(BestScore, float(BestGlyph), 0.0, 0.0);"
        "}"
    );
    if (!gld->match_shader) goto FailExit;

    fprintf(fv->log_fp, "OpenGL Renderer: match size: %d x %d.\n", match_width, match_height);
    if (1)
    {
        int InstCount = InstMatrixWidth * InstMatrixHeight;
        int LoopCount = fv->font_code_count / InstCount;
        fprintf(fv->log_fp, "Expected loop count '%d' for total code count %zu (%d tests run in a pass).\n", LoopCount, fv->font_code_count, InstCount);
    }

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
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &gld->final_texture_c);
    glBindTexture(GL_TEXTURE_2D, gld->final_texture_c);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8I, gld->final_width, gld->final_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenBuffers(1, &gld->PBO_final);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final);
    glBufferData(GL_PIXEL_PACK_BUFFER, size * 4, NULL, GL_STATIC_DRAW);

    glGenBuffers(1, &gld->PBO_final_c);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final_c);
    glBufferData(GL_PIXEL_PACK_BUFFER, size, NULL, GL_STATIC_DRAW);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    gld->final_shader = create_shader_program(fv->log_fp,
        "#version 130\n"
        "in vec2 Position;"
        "out vec2 TexCoord;"
        "void main()"
        "{"
        "   TexCoord = (Position + vec2(1)) * vec2(0.5);"
        "   gl_Position = vec4(Position, 0.0, 1.0);"
        "}"
        ,
        NULL,
        "#version 130\n"
        "precision highp float;"
        "in vec2 TexCoord;"
        "out int Output;"
        "out int Color;"
        "uniform ivec2 Resolution;"
        "uniform ivec2 MatchTexSize;"
        "uniform sampler2D MatchTex;"
        "uniform int FontMatrixCodeCount;"
        "uniform ivec2 GlyphSize;"
        "uniform ivec2 ConsoleSize;"
        "uniform sampler2D SrcColor;"
        "void main()"
        "{"
        "    ivec2 FragCoord = ivec2(TexCoord * vec2(Resolution));"
        "    ivec2 InstMatrix = MatchTexSize / ConsoleSize;"
        "    float BestScore = 0.0;"
        "    int BestGlyph = 0;"
        "    int InstID = 0;"
        "    for(int y = 0; y < InstMatrix.y; y++)"
        "    {"
        "        for(int x = 0; x < InstMatrix.x; x++)"
        "        {"
        "            ivec2 xy = ivec2(x, y);"
        "            vec4 Data = texelFetch(MatchTex, xy * ConsoleSize + FragCoord, 0);"
        "            if (Data.x > BestScore)"
        "            {"
        "                BestScore = Data.x;"
        "                BestGlyph = int(Data.y);"
        "            }"
        "            InstID ++;"
        "        }"
        "    }"
        "    ivec3 AvrColor = ivec3(0);"
        "    ivec2 BaseCoord = FragCoord * GlyphSize;"
        "    int PixelCount = 0;"
        "    bool Bright;"
        "    for(int y = 0; y < GlyphSize.y; y++)"
        "    {"
        "        for(int x = 0; x < GlyphSize.x; x++)"
        "        {"
        "            ivec2 xy = ivec2(x, y);"
        "            AvrColor += ivec3(texelFetch(SrcColor, BaseCoord + xy, 0).rgb * vec3(255));"
        "            PixelCount ++;"
        "        }"
        "    }"
        "    AvrColor /= PixelCount;"
        "    Bright = (AvrColor.r + AvrColor.g + AvrColor.b > 127 + 127 + 127);"
        "    Output = BestGlyph;"
        "    Color ="
        "        ((AvrColor.b & 0x80) >> 5) |"
        "        ((AvrColor.g & 0x80) >> 6) |"
        "        ((AvrColor.r & 0x80) >> 7) |"
        "        (Bright ? 0x08 : 0x00);"
        "}"
    );
    if (!gld->final_shader) goto FailExit;

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

    Location = glGetAttribLocation(gld->match_shader, "Position"); // assert(Location >= 0);
    glEnableVertexAttribArray(Location);
    glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], NULL);
    glVertexAttribDivisor(Location, 0);

    Location = glGetAttribLocation(gld->final_shader, "Position"); // assert(Location >= 0);
    glEnableVertexAttribArray(Location);
    glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], NULL);
    glVertexAttribDivisor(Location, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0 + 0); glBindTexture(GL_TEXTURE_2D, gld->font_matrix_texture);
    glActiveTexture(GL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, gld->src_texture);
    glActiveTexture(GL_TEXTURE0 + 2); glBindTexture(GL_TEXTURE_2D, gld->match_texture);
    glActiveTexture(GL_TEXTURE0 + 3); glBindTexture(GL_TEXTURE_2D, gld->final_texture);
    glActiveTexture(GL_TEXTURE0 + 4); glBindTexture(GL_TEXTURE_2D, gld->final_texture_c);

    glUseProgram(gld->match_shader);

    Location = glGetUniformLocation(gld->match_shader, "Resolution"); // assert(Location >= 0);
    glUniform2i(Location, gld->match_width, gld->match_height);

    Location = glGetUniformLocation(gld->match_shader, "FontMatrix"); // assert(Location >= 0);
    glUniform1i(Location, 0);

    Location = glGetUniformLocation(gld->match_shader, "FontMatrixCodeCount"); // assert(Location >= 0);
    glUniform1i(Location, (GLint)fv->font_code_count);

    Location = glGetUniformLocation(gld->match_shader, "FontMatrixSize"); // assert(Location >= 0);
    glUniform2i(Location, fv->font_mat_w, fv->font_mat_h);

    Location = glGetUniformLocation(gld->match_shader, "GlyphSize"); // assert(Location >= 0);
    glUniform2i(Location, fv->font_w, fv->font_h);

    Location = glGetUniformLocation(gld->match_shader, "ConsoleSize"); // assert(Location >= 0);
    glUniform2i(Location, fv->output_w, fv->output_h);

    Location = glGetUniformLocation(gld->match_shader, "SrcColor"); // assert(Location >= 0);
    glUniform1i(Location, 1);

    glUseProgram(gld->final_shader);

    Location = glGetUniformLocation(gld->final_shader, "Resolution"); // assert(Location >= 0);
    glUniform2i(Location, gld->final_width, gld->final_height);

    Location = glGetUniformLocation(gld->final_shader, "MatchTexSize"); // assert(Location >= 0);
    glUniform2i(Location, gld->match_width, gld->match_height);

    Location = glGetUniformLocation(gld->final_shader, "MatchTex"); // assert(Location >= 0);
    glUniform1i(Location, 2);

    Location = glGetUniformLocation(gld->final_shader, "FontMatrixCodeCount"); // assert(Location >= 0);
    glUniform1i(Location, (GLint)fv->font_code_count);

    Location = glGetUniformLocation(gld->final_shader, "GlyphSize"); // assert(Location >= 0);
    glUniform2i(Location, fv->font_w, fv->font_h);

    Location = glGetUniformLocation(gld->final_shader, "ConsoleSize"); // assert(Location >= 0);
    glUniform2i(Location, fv->output_w, fv->output_h);

    Location = glGetUniformLocation(gld->final_shader, "SrcColor"); // assert(Location >= 0);
    glUniform1i(Location, 1);

    glUseProgram(0);

    glctx_UnMakeCurrent(glctx);
    return 1;
FailExit:
    if (glctx) glctx_UnMakeCurrent(glctx);
    free(Quad_Vertex);
    fv->opengl_context = NULL;
    fv->opengl_data = NULL;
    glctx_Destroy(glctx);
    free(gld);
    fprintf(fv->log_fp, "Giving up using OpenGL.\n");
    fv->allow_opengl = 0;
    return 0;
#endif
}

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
            fprintf(fv->log_fp, "Frame link list locked by %d. (%d)\n", readout, cur_id);
        }
        yield();
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
    atomic_int readout;
    atomic_int cur_id = get_thread_id();
    while ((readout = atomic_exchange(&fv->audio_lock, cur_id)) != 0)
    {
        if (fv->verbose_threading)
        {
            fprintf(fv->log_fp, "Audio link list locked by %d. (%d)\n", readout, cur_id);
        }
        yield();
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
#endif

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

    // snprintf(buf, sizeof buf, "%s"SUBDIR"%s", assets_dir, meta_file);
    d_meta = dictcfg_load(meta_file, log_fp); // Parse ini file
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
        fprintf(log_fp, "Could not allocate font codes: '%s'.\n", strerror(errno));
        goto FailExit;
    }

    snprintf(buf, sizeof buf, "%s"SUBDIR"%s", assets_dir, font_bmp);
    fv->font_matrix = UB_CreateFromFile((const char*)buf, log_fp);
    if (!fv->font_matrix)
    {
        fprintf(log_fp, "Could not load font matrix bitmap file from '%s'.\n", buf);
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
static fontvideo_frame_p frame_create(uint32_t w, uint32_t h, double timestamp, void *bitmap, int bmp_width, int bmp_height, size_t bmp_pitch)
{
    ptrdiff_t i;
    fontvideo_frame_p f = calloc(1, sizeof f[0]);
    if (!f) return f;

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

static void do_cpu_render(fontvideo_p fv, fontvideo_frame_p f)
{
    int fy, fw, fh;
    UniformBitmap_p fm = fv->font_matrix;
    uint32_t font_pixel_count = fv->font_w * fv->font_h;

    fw = f->w;
    fh = f->h;

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
#ifdef DEBUG_OUTPUT_SCREEN
            int best_fmx = 0;
            int best_fmy = 0;
#endif
            float best_score = -9999999.0f;
            int bright = 0;
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
                    float src_normalize = 0;
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

                            src_lum -= 0.5;
                            font_lum -= 0.5;
                            score += src_lum * font_lum;
                            src_normalize += src_lum * src_lum;
                            font_normalize += font_lum * font_lum;
                        }
                    }

                    // Do vector normalize
                    if (src_normalize >= 0.000001f) score /= sqrtf(src_normalize);
                    if (font_normalize >= 0.000001f) score /= sqrtf(font_normalize);
                    if (score > best_score)
                    {
                        best_score = score;
                        best_code_index = cur_code_index;
#ifdef DEBUG_OUTPUT_SCREEN
                        best_fmx = fmx * fv->font_w;
                        best_fmy = fmy * fv->font_h;
#endif
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

            bright = (avr_r + avr_g + avr_b > 127 + 127 + 127);

            // Encode the color into 3-bit BGR with 1-bit brightness
            f->c_row[fy][fx] =
                ((avr_b & 0x80) >> 5) |
                ((avr_g & 0x80) >> 6) |
                ((avr_r & 0x80) >> 7) |
                (bright ? 0x08 : 0x00);
        }
    }
}

#ifdef FONTVIDEO_ALLOW_OPENGL
static void do_gpu_render(fontvideo_p fv, fontvideo_frame_p f)
{
    opengl_data_p gld = fv->opengl_data;
    GLuint FBO_match = gld->FBO_match;
    GLuint FBO_final = gld->FBO_final;
    size_t size;
    void *MapPtr;
    GLenum DrawBuffers[2];
    GLuint src_texture = gld->src_texture;
    GLuint match_texture = gld->match_texture;
    GLuint final_texture = gld->final_texture;
    GLuint final_texture_c = gld->final_texture_c;
    int x, y;

    glctx_MakeCurrent(fv->opengl_context);

    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Start GPU rendering a frame. (%d)\n", get_thread_id());
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_match);

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

    assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glUseProgram(gld->match_shader);
    glBindVertexArray(gld->Quad_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_final);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, final_texture, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, final_texture_c, 0);
    DrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    DrawBuffers[1] = GL_COLOR_ATTACHMENT1;
    glDrawBuffers(2, DrawBuffers);
    glViewport(0, 0, gld->final_width, gld->final_height);
    glBindFragDataLocation(gld->final_shader, 0, "Output");
    glBindFragDataLocation(gld->final_shader, 1, "Color");

    assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glUseProgram(gld->final_shader);
    glBindVertexArray(gld->Quad_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, final_texture);
    size = (size_t)gld->final_width * gld->final_height * 4;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_INT, NULL);
    MapPtr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    memcpy(f->data, MapPtr, size);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, final_texture_c);
    size = (size_t)gld->final_width * gld->final_height;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final_c);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    MapPtr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    memcpy(f->c_data, MapPtr, size);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

    glctx_UnMakeCurrent(fv->opengl_context);

    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Finished GPU rendering a frame. (%d)\n", get_thread_id());
    }

    for (y = 0; y < (int)f->h; y++)
    {
        uint32_t *row = f->row[y];
        for (x = 0; x < (int)f->w - 1; x++)
        {
            row[x] = fv->font_codes[row[x]];
        }
        row[x] = '\n';
    }
}
#endif

// Render the frame from bitmap into font char codes. One of the magic happens here.
static void render_frame_from_rgbabitmap(fontvideo_p fv, fontvideo_frame_p f)
{
    if (f->rendered) return;

    f->rendering_start_time = rttimer_gettime(&fv->tmr);

#ifdef FONTVIDEO_ALLOW_OPENGL
    if (fv->allow_opengl && fv->opengl_context && fv->opengl_data) do_gpu_render(fv, f); else
#endif
    do_cpu_render(fv, f);

    f->rendering_time_consuming = rttimer_gettime(&fv->tmr) - f->rendering_start_time;

    // Finished rendering, update statistics
    atomic_exchange(&f->rendered, get_thread_id());
    lock_frame(fv);
    fv->rendered_frame_count++;
    fv->rendering_frame_count--;
    unlock_frame(fv);

#ifdef DEBUG_OUTPUT_SCREEN
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
            double cur_time = rttimer_gettime(&fv->tmr);
            double lagged_time = cur_time - f->timestamp;
            // If the frame isn't being rendered, first detect if it's too late to render it, then do frame skipping.
            if (fv->real_time_play && lagged_time >= 1.0)
            {
                fprintf(fv->log_fp, "Discarding frame to render due to lag (%lf seconds). (%d)\n", lagged_time, get_thread_id());
                
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

                if (fv->verbose)
                {
                    fprintf(fv->log_fp, "Got frame to render. (%d)\n", get_thread_id());
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
            if (timestamp >= cur->timestamp)
            {
                // Timestamp arrived
                if (timestamp < next->timestamp) break;

                // If the next frame also need to output right now, skip the current frame
                if (cur->rendered)
                {
                    discard_threshold++;

                    if (discard_threshold >= 10)
                    {
                        if (fv->verbose)
                        {
                            fprintf(fv->log_fp, "Discarding rendered frame due to lag. (%d)\n", get_thread_id());
                        }
                    }
                    else break;
                    
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
                        fprintf(fv->log_fp, "Discarding frame to render due to lag. (%d)\n", get_thread_id());
                        
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
            int done_output = 0;
#ifdef _WIN32
            if (fv->do_old_console_output)
            {
                if (!fv->old_console_buffer)
                {
                    fv->old_console_buffer = calloc((size_t)fv->output_w * fv->output_h, sizeof (CHAR_INFO));
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
                    for (y = 0; y < (int)cur->h && y < (int)fv->output_h; y++)
                    {
                        CHAR_INFO *dst_row = &ci[y * fv->output_w];
                        uint32_t *row = cur->row[y];
                        uint8_t *c_row = cur->c_row[y];
                        for (x = 0; x < (int)cur->w && x < (int)fv->output_w; x++)
                        {
                            uint32_t Char = row[x];
                            uint8_t Color = c_row[x];
                            WORD Attr =
                                (Color & 0x01 ? FOREGROUND_RED : 0) |
                                (Color & 0x02 ? FOREGROUND_GREEN : 0) |
                                (Color & 0x04 ? FOREGROUND_BLUE : 0) |
                                (Color & 0x08 ? FOREGROUND_INTENSITY : 0);
                            if (!Attr) Attr = FOREGROUND_INTENSITY;
                            dst_row[x].Attributes = Attr;
                            dst_row[x].Char.UnicodeChar = Char;
                        }
                    }
                    done_output = WriteConsoleOutputW(GetStdHandle(STD_OUTPUT_HANDLE), ci, BufferSize, BufferCoord, &sr);
                }
            }
#endif
            if (!done_output)
            {
                char *u8chr = utf8buf;
                cur_color = cur->c_row[0][0];
                memset(utf8buf, 0, fv->utf8buf_size);
                set_console_color(fv, cur_color);
                u8chr = strchr(u8chr, 0);
                for (y = 0; y < (int)cur->h; y++)
                {
                    uint32_t *row = cur->row[y];
                    uint8_t *c_row = cur->c_row[y];
                    for (x = 0; x < (int)cur->w; x++)
                    {
                        int new_color = c_row[x];
                        if (new_color != cur_color && row[x] != ' ')
                        {
                            cur_color = new_color;
                            *u8chr = '\0';
                            set_console_color(fv, new_color);
                            u8chr = strchr(u8chr, 0);
                        }
                        u32toutf8(&u8chr, row[x]);
                    }
                }
                *u8chr = '\0';
                fputs(utf8buf, fv->graphics_out_fp);
                done_output = 1;
            }
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
#ifndef FONTVIDEO_NO_SOUND
            (fv->do_audio_output &&
            (!fv->audio_last || (fv->audio_last && fv->audio_last->timestamp < target_timestamp)))))
#else
            0))
#endif
        {
            unlock_frame(fv);
            if (fv->verbose)
            {
                fprintf(fv->log_fp, "Decoding frames. (%d)\n", get_thread_id());
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
            if (fv->verbose)
            {
                fprintf(fv->log_fp, "All frames decoded. (%d)\n", get_thread_id());
            }
        }
        atomic_exchange(&fv->doing_decoding, 0);
        return ret;
    }
    return ret;
}

fontvideo_p fv_create
(
    char *input_file,
    FILE *log_fp,
    int do_verbose_log,
    FILE *graphics_out_fp,
    char *assets_meta_file,
    uint32_t x_resolution,
    uint32_t y_resolution,
    double precache_seconds,
    int do_audio_output,
    double start_timestamp
)
{
    fontvideo_p fv = NULL;
    avdec_video_format_t vf = {0};
    avdec_audio_format_t af = {0};

    fv = calloc(1, sizeof fv[0]);
    if (!fv) return fv;
    fv->log_fp = log_fp;
    fv->verbose = do_verbose_log;
#ifdef _DEBUG
    fv->verbose = 1;
#else
    fv->verbose = 0;
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

    if (!load_font(fv, "assets", assets_meta_file)) goto FailExit;
    if (fv->need_chcp || fv->real_time_play)
    {
        init_console(fv);
    }

    // Wide-glyph detect
    if (fv->font_w > fv->font_h / 2)
    {
        fv->output_w /= 2;
    }

    fv->utf8buf_size = (size_t)fv->output_w * fv->output_h * 32 + 1;
    fv->utf8buf = malloc(fv->utf8buf_size);
    if (!fv->utf8buf) goto FailExit;

    vf.width = fv->output_w * fv->font_w;
    vf.height = fv->output_h * fv->font_h;
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

    return fv;
FailExit:
    fv_destroy(fv);
    return NULL;
}

int fv_show_prepare(fontvideo_p fv)
{
    int i;
    int tc = omp_get_max_threads();
    int mt = 1;
    rttimer_t tmr;
    char *bar_buf = NULL;
    int bar_len;

    if (!fv) return 0;

    if (fv->prepared) return 1;

    fprintf(fv->log_fp, "Pre-rendering frames.\n");
    if (fv->allow_opengl && fv->opengl_context && fv->opengl_data) mt = 0;

    if (fv->real_time_play)
    {
        rttimer_init(&tmr, 0);
        bar_len = fv->output_w - 1;
        bar_buf = calloc(bar_len + 1, 1);
        do_decode(fv, 0);
        while (fv->rendered_frame_count + fv->rendering_frame_count < fv->precached_frame_count)
        {
            if (mt)
            {
#pragma omp parallel for
                for (i = 0; i < tc; i++)
                {
                    get_frame_and_render(fv);
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
    int i;
    int tc = omp_get_max_threads();
#ifdef _OPENMP
    int run_mt = 1;
#else
    int run_mt = 0;
#endif

    if (!fv) return 0;

    if (!fv->prepared) fv_show_prepare(fv);

    fv->real_time_play = 1;

    if (fv->allow_opengl && fv->opengl_context && fv->opengl_data) run_mt = 0;

    if (!run_mt)
    {
        for (;;)
        {
            while (!fv->tailed)
            {
                if (!do_decode(fv, 1)) break;
            }
            if (fv->precached_frame_count > fv->rendered_frame_count)
            {
                get_frame_and_render(fv);
            }
            if (fv->rendered_frame_count)
            {
                output_rendered_video(fv, rttimer_gettime(&fv->tmr));
            }
            if (fv->tailed && !fv->precached_frame_count && !fv->rendered_frame_count) break;
        }
    }
    else
    {
#pragma omp parallel for // ordered
        for (i = 0; i < tc; i++)
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
    }
#ifndef FONTVIDEO_NO_SOUND
    while (fv->audios || fv->audio_last) yield();
#endif
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

#ifndef FONTVIDEO_NO_SOUND
    siowrap_destroy(fv->sio);
#endif

    free(fv->font_codes);
    UB_Free(&fv->font_matrix);
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
