#include<stdio.h>

#include"fontvideo.h"

#ifdef _WIN32
#include<Windows.h>
#define SUBDIR "\\"
#else
#include<sys/ioctl.h>
#include<unistd.h>
#define SUBDIR "/"
#endif

void usage(char *argv0)
{
    fprintf(stderr, "Usage: %s -i <input> [-o <output.txt>] [-v] [-p <seconds>] [-m] [-n] [-w <width>] [-h <height>] [-s <width> <height>] [-S <from_sec>] [-b] [--white-background] [--no-frameskip] [--no-auto-aspect-adjust] [--assets-meta <metafile.ini>] [--output-frame-image-sequence <prefix>]\n"
        "Or: %s <input>\n"
        "\t-i: Specify the input video file name.\n"
        "\t-o: [Optional] Specify the output text file name.\n"
        "\t-v: [Optional] Verbose mode, output debug informations.\n"
        "\t  Alias: --verbose\n"
        "\t-vt: [Optional] Verbose mode for multithreading, output lock acquirement informations.\n"
        "\t  Alias: --verbose-threading\n"
        "\t-p: [Optional] Specify pre-render seconds, Longer value results longer delay but better quality.\n"
        "\t  Alias: --pre-render\n"
        "\t-m: [Optional] Mute sound output.\n"
        "\t  Aliases: --mute, --no-sound, --no-audio\n"
        "\t-n: [Optional] Normalize input.\n"
        "\t-w: [Optional] Width of the output.\n"
        "\t  Alias: --width\n"
        "\t-h: [Optional] Height of the output.\n"
        "\t  Alias: --height\n"
        "\t-s: [Optional] Size of the output, default is to detect the size of the console window, or 80x25 if failed.\n"
        "\t  Alias: --size\n"
        "\t-b: Only do white-black output.\n"
        "\t  Alias: --black-white\n"
        "\t-S: [Optional] Set the playback start time of seconds.\n"
        "\t  Alias: --start-time\n"
        "\t--log: [Optional] Specify the log file.\n"
        "\t--white-background: [Optional] Do color invert.\n"
        "\t  Alias: --white-bg\n"
        "\t--no-frameskip: [Optional] Do not skip frames, which may cause video and audio could not sync.\n"
        "\t--no-auto-aspect-adjust: [Optional] Do not adjsut aspect ratio by changing output width automatically.\n"
        "\t--assets-meta: [Optional] Use specified meta file, default is to use 'assets"SUBDIR"meta.ini'.\n"
        "\t--output-frame-image-sequence: [Optional] Output each frame image to a directory. The format of the image is `bmp`.\n"
        "", argv0, argv0);
}

int main(int argc, char **argv)
{
    char *input_file = NULL;
    fontvideo_p fv = NULL;
    FILE *fp_out = NULL;
    int i;
    int real_time_show = 1;
    int verbose = 0;
    int verbose_threading = 0;
    int mute = 0;
    double prerender_secs = 5.0;
    double start_sec = -1.0;
    int output_width = 80;
    int output_height = 25;
    int white_background = 0;
    int normalize_input = 0;
    int no_colors = 0;
    int no_frameskip = 0;
    int no_auto_aspect_adjust = 0;
    char *assets_meta = "assets"SUBDIR"meta.ini";
    char *output_frame_images_prefix = NULL;
    FILE *fp_log = stderr;

#ifdef _WIN32
    if (1) // Try to detect console window size.
    {
        CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;
        // CONSOLE_FONT_INFO FontInfo;
        // RECT ClientRect;
        // GetCurrentConsoleFont(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &FontInfo);
        // GetClientRect(GetConsoleWindow(), &ClientRect))
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ConsoleInfo))
        {
            output_width = ConsoleInfo.dwSize.X;
            // ConsoleInfo.dwSize.Y is not the height of the terminal window.
            // It's actually the buffer size of the CMD output.
            // The commented code below works well with CMD by calculating the height by
            // referring to the CMD window size, but does not work with WSL.
            //     output_height = (ClientRect.bottom - ClientRect.top) / FontInfo.dwFontSize.Y - 2;
            // Gave up trying to detect the height of the terminal.
            output_height = output_width / 4 - 2;
        }
    }
#else
    if (1)
    {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        output_width = w.ws_col;
        output_height = w.ws_row - 2;
    }
#endif

    // If there's only one argument, it is the input file name.
    if (argc == 2)
    {
        input_file = argv[1];
    }
    else
    {
        // Parse arguments
        for (i = 1; i < argc;)
        {
            if (!strcmp(argv[i], "-i"))
            {
                if (++i >= argc) goto BadUsageExit;
                input_file = argv[i++];
            }
            else if (!strcmp(argv[i], "-o"))
            {
                if (++i >= argc) goto BadUsageExit;
                fp_out = fopen(argv[i++], "w");
                if (!fp_out)
                {
                    perror("Trying to open output file.");
                    goto BadUsageExit;
                }
                mute = 1;
                real_time_show = 0;
                no_frameskip = 1;
            }
            else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
            {
                i++;
                verbose = 1;
            }
            else if (!strcmp(argv[i], "-vt") || !strcmp(argv[i], "--verbose-threading"))
            {
                i++;
                verbose_threading = 1;
            }
            else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--pre-render"))
            {
                if (++i >= argc) goto BadUsageExit;
                prerender_secs = atof(argv[i++]);
            }
            else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--mute") || !strcmp(argv[i], "--no-sound") || !strcmp(argv[i], "--no-audio"))
            {
                i++;
                mute = 1;
            }
            else if (!strcmp(argv[i], "-n"))
            {
                i++;
                normalize_input = 1;
            }
            else if (!strcmp(argv[i], "-w") || !strcmp(argv[i], "--width"))
            {
                if (++i >= argc) goto BadUsageExit;
                output_width = atoi(argv[i++]);
            }
            else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--height"))
            {
                if (++i >= argc) goto BadUsageExit;
                output_height = atoi(argv[i++]);
            }
            else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--size"))
            {
                if (++i >= argc) goto BadUsageExit;
                output_width = atoi(argv[i++]);
                if (i >= argc) goto BadUsageExit;
                output_height = atoi(argv[i++]);
            }
            else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--black-white"))
            {
                i++;
                no_colors = 1;
            }
            else if (!strcmp(argv[i], "-S") || !strcmp(argv[i], "--start-time"))
            {
                if (++i >= argc) goto BadUsageExit;
                start_sec = atof(argv[i++]);
            }
            else if (!strcmp(argv[i], "--white-bg") || !strcmp(argv[i], "--white-background"))
            {
                i++;
                white_background = 1;
                no_colors = 1;
            }
            else if (!strcmp(argv[i], "--no-frameskip"))
            {
                i++;
                no_frameskip = 1;
            }
            else if (!strcmp(argv[i], "--no-auto-aspect-adjust"))
            {
                i++;
                no_auto_aspect_adjust = 1;
            }
            else if (!strcmp(argv[i], "--assets-meta"))
            {
                if (++i >= argc) goto BadUsageExit;
                assets_meta = argv[i++];
            }
            else if (!strcmp(argv[i], "--output-frame-image-sequence"))
            {
                if (++i >= argc) goto BadUsageExit;
                output_frame_images_prefix = argv[i++];
            }
            else if (!strcmp(argv[i], "--log"))
            {
                char *log_file;
                if (++i >= argc) goto BadUsageExit;
                log_file = argv[i++];
                fp_log = fopen(log_file, "a");
                if (!fp_log)
                {
                    fp_log = stderr;
                    fprintf(stderr, "Trying to open log file '%s' failed: %s.\n", log_file, strerror(errno));
                }
            }
            else
            {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                goto BadUsageExit;
            }
        }
    }

    if (!input_file)
    {
        fprintf(stderr, "No input file provided.\n");
        goto BadUsageExit;
    }

    if (output_width <= 0 || output_height <= 0)
    {
        fprintf(stderr, "Bad size value parsed out: %d, %d\n", output_width, output_height);
        goto BadUsageExit;
    }

    if (prerender_secs < 0.0)
    {
        fprintf(stderr, "Bad pre-render seconds value: %lf\n", prerender_secs);
        goto BadUsageExit;
    }

    if (!fp_out) fp_out = stdout;

    fv = fv_create
    (
        input_file,
        fp_log,
        verbose,
        verbose_threading,
        fp_out,
        assets_meta,
        output_width,
        output_height,
        prerender_secs,
        !mute,
        start_sec,
        white_background,
        no_auto_aspect_adjust
    );
    if (!fv) goto FailExit;

    if (no_colors) fv->do_colored_output = 0;
    if (no_frameskip) fv->no_frameskip = 1;
    if (normalize_input) fv->normalize_input = 1;
    if (output_frame_images_prefix) fv->output_frame_images_prefix = output_frame_images_prefix;

    if (real_time_show) fv_show(fv);
    else
    {
        fv->real_time_play = 0;
        fv_render(fv);
    }

    fv_destroy(fv);
    if (fp_out != stdout && fp_out != NULL) fclose(fp_out);
    if (fp_log != stderr && fp_log != NULL) fclose(fp_log);
    return 0;
BadUsageExit:
    usage(argv[0]);
    fv_destroy(fv);
    if (fp_out != stdout && fp_out != NULL) fclose(fp_out);
    if (fp_log != stderr && fp_log != NULL) fclose(fp_log);
    return 1;
FailExit:
    fv_destroy(fv);
    if (fp_out != stdout && fp_out != NULL) fclose(fp_out);
    if (fp_log != stderr && fp_log != NULL) fclose(fp_log);
    return 2;
}
