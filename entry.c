#include<stdio.h>

#include"fontvideo.h"

#if defined(_WIN32) || defined(__MINGW32__)
#include<Windows.h>
#endif

void usage(char *argv0)
{
    fprintf(stderr, "Usage: %s -i <input> [-o output.txt] [-v] [-p seconds] [-m] [-w width] [-h height] [-s width height] [-S from_sec] [-b]\n"
        "Or: %s <input>\n"
        "\t-i : Specify the input video file name.\n"
        "\t-o : [Optional] Specify the output text file name.\n"
        "\t-v : Verbose mode, output debug informations.\n"
        "\t-p : [Optional] Specify pre-render seconds, Longer value results longer delay but better quality.\n"
        "\t-m : Mute sound output.\n"
        "\t-w : [Optional] Width of the output.\n"
        "\t-h : [Optional] Height of the output.\n"
        "\t-s : [Optional] Size of the output, default is to detect the size of the console window, or 80x25 if failed.\n"
        "\t-b : Only do white-black output.\n"
        "\t-S : [Optional] Set the playback start time of seconds.\n"
        "\t--no-opengl: [Optional] Do not use OpenGL to accelerate rendering.\n"
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
    int mute = 0;
    double prerender_secs = 5.0;
    double start_sec = -1.0;
    int output_width = 80;
    int output_height = 25;
    int no_colors = 0;
    int no_opengl = 0;

#if defined(_WIN32) || defined(__MINGW32__)
#pragma comment(lib, "user32.lib")
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
            }
            else if (!strcmp(argv[i], "-v"))
            {
                i++;
                verbose = 1;
            }
            else if (!strcmp(argv[i], "-p"))
            {
                if (++i >= argc) goto BadUsageExit;
                prerender_secs = atof(argv[i++]);
            }
            else if (!strcmp(argv[i], "-m"))
            {
                i++;
                mute = 1;
            }
            else if (!strcmp(argv[i], "-w"))
            {
                if (++i >= argc) goto BadUsageExit;
                output_width = atoi(argv[i++]);
            }
            else if (!strcmp(argv[i], "-h"))
            {
                if (++i >= argc) goto BadUsageExit;
                output_height = atoi(argv[i++]);
            }
            else if (!strcmp(argv[i], "-s"))
            {
                if (++i >= argc) goto BadUsageExit;
                output_width = atoi(argv[i++]);
                if (i >= argc) goto BadUsageExit;
                output_height = atoi(argv[i++]);
            }
            else if (!strcmp(argv[i], "-b"))
            {
                i++;
                no_colors = 1;
            }
            else if (!strcmp(argv[i], "-S"))
            {
                if (++i >= argc) goto BadUsageExit;
                start_sec = atof(argv[i++]);
            }
            else if (!strcmp(argv[i], "--no-opengl"))
            {
                i++;
                no_opengl = 1;
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

    fv = fv_create(input_file, stderr, verbose, fp_out, output_width, output_height, prerender_secs, !mute, start_sec);
    if (!fv) goto FailExit;
    if (no_colors) fv->do_colored_output = 0;

    if (!no_opengl) fv_allow_opengl(fv);

    if (real_time_show)
        fv_show(fv);
    else
        fv_render(fv);

    fv_destroy(fv);
    if (fp_out != stdout && fp_out != NULL) fclose(fp_out);
    return 0;
BadUsageExit:
    usage(argv[0]);
    fv_destroy(fv);
    if (fp_out != stdout && fp_out != NULL) fclose(fp_out);
    return 1;
FailExit:
    fv_destroy(fv);
    if (fp_out != stdout && fp_out != NULL) fclose(fp_out);
    return 2;
}
