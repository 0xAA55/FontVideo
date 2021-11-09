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
#endif

static int get_thread_id()
{
    return thrd_current();
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

static void yield()
{
    if (!SwitchToThread()) Sleep(1);
}

#else // For non-Win32 appliction, assume it's linux/unix and runs in terminal

#ifdef FONTVIDEO_ALLOW_OPENGL
#include <GL/glxew.h>
#include <GL/glx.h>
#include <unistd.h>
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

#endif

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
            fputs("\n", compiler_output);
        }
    }

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
    GLint max_fbo_width;
    GLint max_fbo_height;
    GLint Location;
    GLenum err_code = GL_NO_ERROR;
    int InstMatrixWidth;
    int InstMatrixHeight;
    struct Quad_Vertex_Struct
    {
        float PosX;
        float PosY;
        float TexU;
        float TexV;
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

    glGenFramebuffers(1, &gld->FBO_match);
    glGenFramebuffers(1, &gld->FBO_reduction);
    glGenFramebuffers(1, &gld->FBO_final);

    gld->src_width = fv->output_w * fv->font_w;
    gld->src_height = fv->output_w * fv->font_w;
    size = (size_t)gld->src_width * gld->src_height * 4;

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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fv->font_matrix->Width, fv->font_matrix->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, gld->match_width, gld->match_height, 0, GL_RG, GL_FLOAT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    gld->match_shader = create_shader_program(fv->log_fp,
        "#version 130\n"
        "in vec2 vPosition;"
        "in vec2 vTexCoord;"
        "out vec2 TexCoord;"
        "void main()"
        "{"
        "   TexCoord = vTexCoord;"
        "   gl_Position = vec4(vPosition, 0.0, 1.0);"
        "}"
        ,
        NULL,
        "#version 130\n"
        "precision highp float;"
        "in vec2 TexCoord;"
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

    if (output_init_info)
    {
        int InstCount = InstMatrixWidth * InstMatrixHeight;
        int LoopCount = (int)(fv->font_code_count / InstCount);
        fprintf(fv->log_fp, "OpenGL Renderer: match size: %d x %d.\n", match_width, match_height);
        fprintf(fv->log_fp, "Expected loop count '%d' for total code count %zu (%d x %d = %d tests run in a pass).\n", LoopCount, fv->font_code_count, InstMatrixWidth, InstMatrixHeight, InstCount);
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
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &gld->reduction_texture2);
    glBindTexture(GL_TEXTURE_2D, gld->reduction_texture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, gld->reduction_width, gld->reduction_height, 0, GL_RG, GL_FLOAT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    gld->reduction_shader = create_shader_program(fv->log_fp,
        "#version 130\n"
        "in vec2 vPosition;"
        "in vec2 vTexCoord;"
        "out vec2 TexCoord;"
        "void main()"
        "{"
        "   TexCoord = vTexCoord;"
        "   gl_Position = vec4(vPosition, 0.0, 1.0);"
        "}"
        ,
        NULL,
        "#version 130\n"
        "precision highp float;"
        "in vec2 TexCoord;"
        "out vec2 Output;"
        "uniform ivec2 Resolution;"
        "uniform ivec2 ConsoleSize;"
        "uniform sampler2D ReductionTex;"
        "uniform ivec2 ReductionSize;"
        "void main()"
        "{"
        "    ivec2 FragCoord = ivec2(TexCoord * vec2(Resolution));"
        "    ivec2 InstCoord = FragCoord / ConsoleSize;"
        "    ivec2 ConsoleCoord = FragCoord - (InstCoord * ConsoleSize);"
        "    ivec2 SrcInstCoord = InstCoord * ReductionSize;"
        "    float BestScore = -99999.9;"
        "    int BestGlyph = 0;"
        "    for(int y = 0; y < ReductionSize.y; y++)"
        "    {"
        "        for(int x = 0; x < ReductionSize.x; x++)"
        "        {"
        "            ivec2 xy = ivec2(x, y);"
        "            vec2 Data = texelFetch(ReductionTex, (SrcInstCoord + xy) * ConsoleSize + ConsoleCoord, 0).rg;"
        "            if (Data.x > BestScore)"
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
        "in vec2 vPosition;"
        "in vec2 vTexCoord;"
        "out vec2 TexCoord;"
        "void main()"
        "{"
        "   TexCoord = vTexCoord;"
        "   gl_Position = vec4(vPosition, 0.0, 1.0);"
        "}"
        ,
        NULL,
        "#version 130\n"
        "precision highp float;"
        "in vec2 TexCoord;"
        "out int Output;"
        "out int Color;"
        "uniform ivec2 Resolution;"
        "uniform sampler2D ReductionTex;"
        "uniform ivec2 GlyphSize;"
        "uniform ivec2 ConsoleSize;"
        "uniform sampler2D SrcColor;"
        "uniform int SrcInvert;"
        "uniform vec3 Palette[16] ="
        "{"
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
        "};"
        "void main()"
        "{"
        "    ivec2 FragCoord = ivec2(TexCoord * vec2(Resolution));"
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
        "    float ColorMinDist = 999999.9;"
        "    for(int i = 0; i < 16; i++)"
        "    {"
        "        vec3 TV1 = normalize(AvrColor - vec3(0.5));"
        "        vec3 TV2 = normalize(Palette[i].bgr - vec3(0.5));"
        "        float ColorDist = 1.0 - dot(TV1, TV2);"
        "        if (ColorDist < ColorMinDist)"
        "        {"
        "            ColorMinDist = ColorDist;"
        "            Color = i;"
        "        }"
        "    }"
        "    if (Color == 0) Color = 8;"
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
    Quad_Vertex[0].TexU = 0; Quad_Vertex[0].TexV = 0;
    Quad_Vertex[1].TexU = 1; Quad_Vertex[1].TexV = 0;
    Quad_Vertex[2].TexU = 0; Quad_Vertex[2].TexV = 1;
    Quad_Vertex[3].TexU = 1; Quad_Vertex[3].TexV = 1;
    glUnmapBuffer(GL_ARRAY_BUFFER);

    Location = glGetAttribLocation(gld->match_shader, "vPosition"); // assert(Location >= 0);
    glEnableVertexAttribArray(Location);
    glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)0);

    Location = glGetAttribLocation(gld->match_shader, "vTexCoord"); // assert(Location >= 0);
    glEnableVertexAttribArray(Location);
    glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)8);

    Location = glGetAttribLocation(gld->reduction_shader, "vPosition"); // assert(Location >= 0);
    glEnableVertexAttribArray(Location);
    glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)0);

    Location = glGetAttribLocation(gld->reduction_shader, "vTexCoord"); // assert(Location >= 0);
    glEnableVertexAttribArray(Location);
    glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)8);

    Location = glGetAttribLocation(gld->final_shader, "vPosition"); // assert(Location >= 0);
    glEnableVertexAttribArray(Location);
    glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)0);

    Location = glGetAttribLocation(gld->final_shader, "vTexCoord"); // assert(Location >= 0);
    glEnableVertexAttribArray(Location);
    glVertexAttribPointer(Location, 2, GL_FLOAT, GL_FALSE, sizeof Quad_Vertex[0], (void *)8);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glUseProgram(0);

    err_code = glGetError();
    if (err_code != GL_NO_ERROR) goto OpenGLErrorExit;

    return gld;
OpenGLErrorExit:
    do
    {
        fprintf(fv->log_fp, "OpenGL Renderer: OpenGL error occured: %u (%s).\n", err_code, strOpenGLError(err_code));
        err_code = glGetError();
    } while (err_code != GL_NO_ERROR);
    goto FailExit;
NoMemFailExit:
    fprintf(fv->log_fp, "OpenGL Renderer: Could not initialize OpenGL data: %s.\n", strerror(errno));
    goto FailExit;
FailExit:
    free(Quad_Vertex);
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
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
    atomic_int readout = 0;
    int cur_id = get_thread_id();
    while ((readout = atomic_exchange(&ctx->lock, cur_id)) != 0)
    {
        if (fv->verbose_threading)
        {
            fprintf(fv->log_fp, "OpenGL Context locked by %u.\n", readout);
        }
        yield();
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
    int expected = 0;
    int cur_id = get_thread_id();
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

static opengl_renderer_p opengl_renderer_create(fontvideo_p fv)
{
    opengl_renderer_p r = NULL;
    int ctx_count = omp_get_max_threads();
    int i, j;
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

    return r;
NoMemFailExit:
    fprintf(fv->log_fp, "OpenGL Renderer: Failed to initialize: %s.\n", strerror(errno));
    goto FailExit;
FailExit:
    opengl_renderer_destroy(r);
    return NULL;
}

int fv_allow_opengl(fontvideo_p fv)
{
    fv->allow_opengl = 1;
    fv->opengl_renderer = opengl_renderer_create(fv);
    if (!fv->opengl_renderer) goto FailExit;
    return 1;
FailExit:
    fprintf(fv->log_fp, "Giving up using OpenGL.\n");
    fv->allow_opengl = 0;
    return 0;
}

#else
int fv_allow_opengl(fontvideo_p fv)
{
    fprintf(fv->log_fp, "Macro `FONTVIDEO_ALLOW_OPENGL` not defined when compiling, giving up using OpenGL.\n");
    return 0;
}
#endif

// For locking the frames link list of `fv->frames` and `fv->frame_last`, not for locking every individual frames
static void lock_frame(fontvideo_p fv)
{
    int readout = 0;
    int cur_id = get_thread_id();
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
    size_t font_pixel_count;

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
    fv->font_w = dictcfg_getint(d_font, "font_width", 12);
    fv->font_h = dictcfg_getint(d_font, "font_height", 12);
    fv->font_face = dictcfg_getstr(d_font, "font_face", NULL);
    fv->font_code_count = font_count_max = dictcfg_getint(d_font, "font_count", 127);
    fv->font_codes = malloc(font_count_max * sizeof fv->font_codes[0]);
    if (!fv->font_codes)
    {
        fprintf(log_fp, "Could not allocate font codes: '%s'.\n", strerror(errno));
        goto FailExit;
    }
    font_pixel_count = (size_t)fv->font_w * fv->font_h;

    snprintf(buf, sizeof buf, "%s"SUBDIR"%s", assets_dir, font_bmp);
    fv->font_matrix = UB_CreateFromFile((const char*)buf, log_fp);
    if (!fv->font_matrix)
    {
        fprintf(log_fp, "Could not load font matrix bitmap file from '%s'.\n", buf);
        goto FailExit;
    }
    
#ifdef DEBUG_OUTPUT_TO_SCREEN
    DebugRawBitmap(0, 0, fv->font_matrix->BitmapData, fv->font_matrix->Width, fv->font_matrix->Height);
#endif

    // Font matrix dimension
    fv->font_mat_w = fv->font_matrix->Width / fv->font_w;
    fv->font_mat_h = fv->font_matrix->Height / fv->font_h;
    expected_font_code_count = (size_t)fv->font_mat_w * fv->font_mat_h;

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

    // Detect if the font matrix size doesn't match
    if (fv->font_code_count > expected_font_code_count)
    {
        fprintf(log_fp, "Font meta-file issue: font bitmap file size of %ux%u"
            " for glyph size %ux%u"
            " should have a maximum of %zu"
            " glyphs, but the given code count %zu"
            " exceeded the glyph count. Please correct the meta-file and run this program again.\n",
            fv->font_matrix->Width, fv->font_matrix->Height, fv->font_w, fv->font_h, expected_font_code_count, fv->font_code_count);
        goto FailExit;
    }

    fv->font_luminance_image = malloc(fv->font_code_count * font_pixel_count * sizeof fv->font_luminance_image[0]);
    if (!fv->font_luminance_image)
    {
        fprintf(log_fp, "Could not calculate font glyph lum values: %s\n", strerror(errno));
        goto FailExit;
    }

    fv->font_is_blackwhite = 1;
#pragma omp parallel for
    for (si = 0; si < (ptrdiff_t)fv->font_code_count; si++)
    {
        UniformBitmap_p fm = fv->font_matrix;
        uint32_t x, y;
        int srcx = (int)((si % fv->font_mat_w) * fv->font_w);
        int srcy = (int)((si / fv->font_mat_w) * fv->font_h);
        float *lum_of_glyph = &fv->font_luminance_image[si * font_pixel_count];
        float lum = 0;
        for (y = 0; y < fv->font_h; y++)
        {
            uint32_t *row = fm->RowPointers[fm->Height - 1 - (srcy + y)];
            float *lum_row = &lum_of_glyph[y * fv->font_w];
            for (x = 0; x < fv->font_w; x++)
            {
                union pixel
                {
                    uint8_t u8[4];
                    uint32_t u32;
                    float f32;
                }   *font_pixel = (void *)&(row[srcx + x]);
                float font_lum = sqrtf((float)(
                    (uint32_t)font_pixel->u8[0] * font_pixel->u8[0] +
                    (uint32_t)font_pixel->u8[1] * font_pixel->u8[1] +
                    (uint32_t)font_pixel->u8[2] * font_pixel->u8[2])) / 441.672955930063709849498817084f;
                if (font_pixel->u32 != 0xff000000 && font_pixel->u32 != 0xffffffff)
                {
                    fv->font_is_blackwhite = 0;
                }
                font_lum -= 0.5f;
                lum_row[x] = font_lum;
                lum += font_lum * font_lum;
            }
        }
        if (lum >= 0.000001f)
        {
            for (y = 0; y < fv->font_h; y++)
            {
                float *lum_row = &lum_of_glyph[y * fv->font_w];
                for (x = 0; x < fv->font_w; x++)
                {
                    lum_row[x] /= sqrtf(lum);
                }
            }
        }
    }

#ifdef DEBUG_OUTPUT_TO_SCREEN
    DebugRawBitmap(0, 0, fv->font_matrix->BitmapData, fv->font_matrix->Width, fv->font_matrix->Height);
#endif

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

static void do_cpu_render(fontvideo_p fv, fontvideo_frame_p f)
{
    int fy, fw, fh;
    uint32_t font_pixel_count = fv->font_w * fv->font_h;

    fw = f->w;
    fh = f->h;

    // OpenMP will automatically disable recursive multi-threading
#pragma omp parallel for
    for (fy = 0; fy < fh; fy++)
    {
        int fx, sx, sy;
        uint32_t *row = f->row[fy];
        uint8_t *c_row = f->c_row[fy];
        float *src_lum_buffer = malloc(font_pixel_count * sizeof src_lum_buffer[0]);
        sy = fy * fv->font_h;

        for (fx = 0; fx < fw; fx++)
        {
            int x, y;
            uint32_t avr_r = 0, avr_g = 0, avr_b = 0;
            uint32_t cur_code_index = 0;
            uint32_t best_code_index = 0;
            float src_normalize = 0;
            float best_score = -9999999.0f;
            int bright = 0;
            sx = fx * fv->font_w;

            // Replace the rightmost char to newline
            if (fx == fw - 1)
            {
                row[fx] = '\n';
                continue;
            }

            // Compute source luminance for further use
            for (y = 0; y < (int)fv->font_h; y++)
            {
                uint32_t *raw_row = f->raw_data_row[sy + y];
                float *buf_row = &src_lum_buffer[y * fv->font_w];
                for (x = 0; x < (int)fv->font_w; x++)
                {
                    union pixel
                    {
                        uint8_t u8[4];
                        uint32_t u32;
                    }   *src_pixel = (void *)&(raw_row[sx + x]);
                    float src_lum;
                    if (fv->do_color_invert)
                    {
                        src_pixel->u32 ^= 0xffffff;
                    }
                    src_lum = sqrtf((float)(
                        (uint32_t)src_pixel->u8[0] * src_pixel->u8[0] +
                        (uint32_t)src_pixel->u8[1] * src_pixel->u8[1] +
                        (uint32_t)src_pixel->u8[2] * src_pixel->u8[2])) / 441.672955930063709849498817084f;
                    src_lum -= 0.5f;
                    buf_row[x] = src_lum;
                    src_normalize += src_lum * src_lum;
                }
            }
            src_normalize = sqrtf(src_normalize);

            // Iterate the font matrix
            for (cur_code_index = 0; cur_code_index < fv->font_code_count; cur_code_index++)
            {
                float score = 0;
                float *font_lum_img = &fv->font_luminance_image[cur_code_index * font_pixel_count];

                // Compare each pixels and collect the scores.
                for (y = 0; y < (int)fv->font_h; y++)
                {
                    size_t row_start = (size_t)y * fv->font_w;
                    float *buf_row = &src_lum_buffer[row_start];
                    float *font_row = &font_lum_img[row_start];
                    for (x = 0; x < (int)fv->font_w; x++)
                    {
                        float src_lum = buf_row[x];
                        float font_lum = font_row[x];
                        score += src_lum * font_lum;
                    }
                }

                // Do vector normalize
                if (src_normalize >= 0.000001f) score /= src_normalize;
                if (score > best_score)
                {
                    best_score = score;
                    best_code_index = cur_code_index;
                }

                cur_code_index++;
                if (cur_code_index >= fv->font_code_count) break;
            }

            // The best matching char code
            // row[fx] = fv->font_codes[best_code_index];
            row[fx] = best_code_index;

            // Get the average color of the char
            for (y = 0; y < (int)fv->font_h; y++)
            {
                uint32_t *raw_row = f->raw_data_row[sy + y];
#ifdef DEBUG_OUTPUT_TO_SCREEN
                size_t row_start = (size_t)y * fv->font_w;
                float *font_row = &fv->font_luminance_image[best_code_index * font_pixel_count + row_start];
                float *buf_row = &src_lum_buffer[y * fv->font_w];
#endif
                for (x = 0; x < (int)fv->font_w; x++)
                {
                    union pixel
                    {
                        uint8_t u8[4];
                        uint32_t u32;
                    }   *src_pixel = (void *)&(raw_row[sx + x]);

                    avr_b += src_pixel->u8[0];
                    avr_g += src_pixel->u8[1];
                    avr_r += src_pixel->u8[2];

#ifdef DEBUG_OUTPUT_TO_SCREEN
                    if (1)
                    {
                        uint8_t uv = (uint8_t)((font_row[x] + 0.5) * 255.0f);
                        src_pixel->u8[0] = uv;
                        src_pixel->u8[1] = uv;
                        src_pixel->u8[2] = uv;
                    }
#endif
                }
            }

            avr_b /= font_pixel_count;
            avr_g /= font_pixel_count;
            avr_r /= font_pixel_count;

            bright = (avr_r + avr_g + avr_b > 127 + 127 + 127);

            // Encode the color into 3-bit BGR with 1-bit brightness
            c_row[fx] =
                ((avr_b & 0x80) >> 5) |
                ((avr_g & 0x80) >> 6) |
                ((avr_r & 0x80) >> 7) |
                (bright ? 0x08 : 0x00);
        }

        free(src_lum_buffer);
    }
    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Finished CPU rendering a frame. (%d)\n", get_thread_id());
    }
}

#ifdef FONTVIDEO_ALLOW_OPENGL
static int do_opengl_render_command(fontvideo_p fv, fontvideo_frame_p f, opengl_data_p gld)
{
    GLint Location;
    size_t size, i;
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

    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Start GPU rendering a frame. (%d)\n", get_thread_id());
    }

    // First step: issuing a rough matching with massive instances and a small number of glyphs to match.
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

    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) goto FBONotCompleteExit;

    glUseProgram(gld->match_shader);

    Location = glGetUniformLocation(gld->match_shader, "Resolution"); // assert(Location >= 0);
    glUniform2i(Location, gld->match_width, gld->match_height);

    Location = glGetUniformLocation(gld->match_shader, "FontMatrix"); // assert(Location >= 0);
    glActiveTexture(GL_TEXTURE0 + 0); glBindTexture(GL_TEXTURE_2D, gld->font_matrix_texture);
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
    glActiveTexture(GL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, gld->src_texture);
    glUniform1i(Location, 1);

    Location = glGetUniformLocation(gld->match_shader, "SrcInvert"); // assert(Location >= 0);
    glUniform1i(Location, fv->do_color_invert);

    glBindVertexArray(gld->Quad_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Second step: merge instances for filtering the highest scores.
    reduction_tex1 = match_texture;
    reduction_tex2 = gld->reduction_texture1;
    reduction_src_width = gld->reduction_width;
    reduction_src_height = gld->reduction_height;
    reduction_pass = 0;
    while(1)
    {
        const int max_reduction_scale = 32;
        int i;
        int src_instmat_w = 0;
        int src_instmat_h = 0;
        int dst_instmat_w = 0;
        int dst_instmat_h = 0;
        int reduction_x = 0;
        int reduction_y = 0;

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

    Location = glGetUniformLocation(gld->final_shader, "Resolution"); // assert(Location >= 0);
    glUniform2i(Location, gld->final_width, gld->final_height);

    Location = glGetUniformLocation(gld->final_shader, "ReductionTex"); // assert(Location >= 0);
    glActiveTexture(GL_TEXTURE0 + 2); glBindTexture(GL_TEXTURE_2D, reduction_tex2);
    glUniform1i(Location, 2);

    Location = glGetUniformLocation(gld->final_shader, "GlyphSize"); // assert(Location >= 0);
    glUniform2i(Location, fv->font_w, fv->font_h);

    Location = glGetUniformLocation(gld->final_shader, "ConsoleSize"); // assert(Location >= 0);
    glUniform2i(Location, fv->output_w, fv->output_h);

    Location = glGetUniformLocation(gld->final_shader, "SrcColor"); // assert(Location >= 0);
    glActiveTexture(GL_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, gld->src_texture);
    glUniform1i(Location, 1);

    Location = glGetUniformLocation(gld->final_shader, "SrcInvert"); // assert(Location >= 0);
    glUniform1i(Location, fv->do_color_invert);

    glBindVertexArray(gld->Quad_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, final_texture);
    size = (size_t)gld->final_width * gld->final_height * 4;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, gld->PBO_final);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_INT, NULL);
    MapPtr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    memcpy(f->data, MapPtr, size);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

    size = gld->final_width * gld->final_height;
    retval = 0;
    for (i = 0; i < size; i++)
    {
        if (f->data[i] != 0)
        {
            retval = 1;
            break;
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
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    MapPtr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    memcpy(f->c_data, MapPtr, size);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    err_code = glGetError();
    while (err_code != GL_NO_ERROR)
    {
        fprintf(fv->log_fp, "OpenGL Renderer (%d): Failed to GPU rendering a frame: %u (%s)\n", get_thread_id(), err_code, strOpenGLError(err_code));
        retval = 0;
        err_code = glGetError();
    }
    if (!retval) goto FailExit;

    if (fv->verbose)
    {
        fprintf(fv->log_fp, "Finished GPU rendering a frame. (%d)\n", get_thread_id());
    }

    return retval;
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
                    atomic_exchange(&(r->contexts[i]), NULL);
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
        if (!ctx) yield();
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
                    atomic_exchange(&(r->contexts[i]), NULL);
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
                        return NULL;
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

static int create_rendered_image(fontvideo_p fv, fontvideo_frame_p rendered_frame, void ** out_file_content_buffer, size_t *out_file_content_size)
{
    void *buffer = NULL;
    size_t buf_size = 0;
    size_t pitch;
    uint32_t cx, cy, bw, bh;
    uint8_t *bmp;
    UniformBitmap_p fm = fv->font_matrix;

    bw = fv->font_w * rendered_frame->w;
    bh = fv->font_h * rendered_frame->h;

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
        header->Palette[000] = 0x000000;
        header->Palette[001] = 0x7F0000;
        header->Palette[002] = 0x007F00;
        header->Palette[003] = 0x7F7F00;
        header->Palette[004] = 0x00007F;
        header->Palette[005] = 0x7F007F;
        header->Palette[006] = 0x007F7F;
        header->Palette[007] = 0x7F7F7F;
        header->Palette[010] = 0x3F3F3F;
        header->Palette[011] = 0xFF0000;
        header->Palette[012] = 0x00FF00;
        header->Palette[013] = 0xFFFF00;
        header->Palette[014] = 0x0000FF;
        header->Palette[015] = 0xFF00FF;
        header->Palette[016] = 0x00FFFF;
        header->Palette[017] = 0xFFFFFF;

        bmp = (void *)&header[1];
        for (cy = 0; cy < rendered_frame->h; cy++)
        {
            uint32_t *con_row = rendered_frame->row[cy];
            uint8_t *col_row = rendered_frame->c_row[cy];
            uint32_t dy = cy * fv->font_h;
            for (cx = 0; cx < rendered_frame->w; cx++)
            {
                uint32_t code = con_row[cx];
                uint8_t color = col_row[cx];
                int font_x = (int)((code % fv->font_mat_w) * fv->font_w);
                int font_y = (int)((code / fv->font_mat_w) * fv->font_h);
                uint32_t dx = cx * fv->font_w;
                uint32_t x, y;
                for (y = 0; y < fv->font_h; y++)
                {
                    uint32_t *src_row = fm->RowPointers[fm->Height - 1 - (font_y + y)];
                    uint8_t *dst_row = &bmp[(size_t)(bh - 1 - (dy + y)) * pitch];
                    for (x = 0; x < fv->font_w; x+=2)
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
        } Palette[16] =
        {
            {0x000000},
            {0x00007F},
            {0x007F00},
            {0x007F7F},
            {0x7F0000},
            {0x7F007F},
            {0x7F7F00},
            {0x7F7F7F},
            {0x3F3F3F},
            {0x0000FF},
            {0x00FF00},
            {0x00FFFF},
            {0xFF0000},
            {0xFF00FF},
            {0xFFFF00},
            {0xFFFFFF}
        };
#pragma pack(pop)

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
            uint32_t dy = cy * fv->font_h;
            for (cx = 0; cx < rendered_frame->w; cx++)
            {
                uint32_t code = con_row[cx];
                uint8_t color = col_row[cx];
                int font_x = (int)((code % fv->font_mat_w) * fv->font_w);
                int font_y = (int)((code / fv->font_mat_w) * fv->font_h);
                uint32_t dx = cx * fv->font_w;
                uint32_t x, y;
                for (y = 0; y < fv->font_h; y++)
                {
                    uint32_t *src_row = fm->RowPointers[fm->Height - 1 - (font_y + y)];
                    uint8_t *dst_row = &bmp[(size_t)(bh - 1 - (dy + y)) * pitch];
                    for (x = 0; x < fv->font_w; x ++)
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
                if (cur->rendered)
                {
                    discard_threshold++;

                    if (discard_threshold >= 10)
                    {
                        if (fv->verbose)
                        {
                            fprintf(fv->log_fp, "Discarding rendered frame %u due to lag. (%d)\n", cur->index, get_thread_id());
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
                }
                else
                {
                    atomic_int expected = 0;

                    // Acquire it to prevent it from being rendered
                    if (atomic_compare_exchange_strong(&cur->rendering, &expected, get_thread_id()))
                    {
                        fprintf(fv->log_fp, "Discarding frame %u to render due to lag. (%d)\n", cur->index, get_thread_id());
                        
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
                                uint32_t Char = fv->font_codes[row[x]];
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
                            uint32_t char_code = fv->font_codes[row[x]];
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
                        u32toutf8(&u8chr, fv->font_codes[row[x]]);
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

static int do_decode(fontvideo_p fv, int keeprun)
{
    double target_timestamp = rttimer_gettime(&fv->tmr) + fv->precache_seconds;
    int ret = 0;
    if (!fv->tailed)
    {
        if (atomic_exchange(&fv->doing_decoding, 1)) return 0;

        lock_frame(fv);
        while (!fv->tailed && ((fv->real_time_play && (
            !fv->frame_last || (fv->frame_last && fv->frame_last->timestamp < target_timestamp) ||
#ifndef FONTVIDEO_NO_SOUND
            (fv->do_audio_output &&
            (!fv->audio_last || (fv->audio_last && fv->audio_last->timestamp < target_timestamp))))) ||
#else
            0)) ||
#endif
            !fv->real_time_play))
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
    int log_verbose,
    int log_verbose_threading,
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

    fv->output_w = x_resolution;
    fv->output_h = y_resolution;
    fv->precache_seconds = precache_seconds;

    if (!load_font(fv, "assets", assets_meta_file)) goto FailExit;
    if (fv->need_chcp || fv->real_time_play || log_fp == stdout || log_fp == stderr)
    {
        init_console(fv);
    }

    // Wide-glyph detect
    if (fv->font_w > fv->font_h / 2)
    {
        fv->font_is_wide = 1;
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
        do_decode(fv, 0);
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
#else
    int run_mt = 0;
#endif

    if (!fv) return 0;

    if (run_mt)
    {
#pragma omp parallel
        for (;;)
        {
            if ((!fv->tailed && !do_decode(fv, 1)) ||
                (fv->frames && fv->rendered_frame_count && !output_rendered_video(fv, rttimer_gettime(&fv->tmr))) ||
                (fv->frames && fv->precached_frame_count > fv->rendered_frame_count && !get_frame_and_render(fv)) ||
                    0)
            {
                continue;
            }
            else
            {
                yield();
            }
            if (fv->tailed && !fv->frames) break;
        }
    }
    else
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
            if (fv->tailed && !fv->frames) break;
        }
    }
#ifndef FONTVIDEO_NO_SOUND
    while (fv->do_audio_output && (fv->audios || fv->audio_last)) yield();
#endif
    return 1;
}

void fv_destroy(fontvideo_p fv)
{
    if (!fv) return;

#ifndef FONTVIDEO_NO_SOUND
    siowrap_destroy(fv->sio);
#endif

    free(fv->font_luminance_image);
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
