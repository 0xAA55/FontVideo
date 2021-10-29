#include<stdio.h>

#include"fontvideo.h"

#if defined(_WIN32) || defined(__MINGW32__)
#include<Windows.h>
#endif

void usage(char *argv0)
{
    fprintf(stderr, "Usage: %s -i <input> [-o output.txt] [-v] [-p seconds] [-m] [-s width height] [-b]\n"
        "\t-i : Specify the input video file name.\n"
        "\t-o : [Optional] Specify the output text file name.\n"
        "\t-v : Verbose mode, output debug informations.\n"
        "\t-p : [Optional] Specify pre-render seconds, Longer value results longer delay but better quality.\n"
        "\t-m : Mute sound output.\n"
        "\t-s : [Optional] Size of the output, default is 80 characters per row and 25 rows.\n"
        "\t-b : Only do white-black output.\n"
        "", argv0);
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
    int output_width = 80;
    int output_height = 25;
    int no_colors = 0;

#if defined(_WIN32) || defined(__MINGW32__)
#pragma comment(lib, "user32.lib")
    if (0)
    {
        CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;
        CONSOLE_FONT_INFO FontInfo;
        RECT ClientRect;
        GetCurrentConsoleFont(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &FontInfo);
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ConsoleInfo);
        GetClientRect(GetConsoleWindow(), &ClientRect);
        output_width = ConsoleInfo.dwSize.X;
        output_height = (ClientRect.bottom - ClientRect.top) / FontInfo.dwFontSize.Y - 2;
    }
#endif

    for (i = 1; i < argc;)
    {
        if (!stricmp(argv[i], "-i"))
        {
            if (++i >= argc) goto BadUsageExit;
            input_file = argv[i++];
        }
        else if (!stricmp(argv[i], "-o"))
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
        else if (!stricmp(argv[i], "-v"))
        {
            i++;
            verbose = 1;
        }
        else if (!stricmp(argv[i], "-p"))
        {
            if (++i >= argc) goto BadUsageExit;
            prerender_secs = atof(argv[i++]);
            if (prerender_secs < 0.0)
            {
                fprintf(stderr, "Bad pre-render seconds value: %lf\n", prerender_secs);
                goto BadUsageExit;
            }
        }
        else if (!stricmp(argv[i], "-m"))
        {
            i++;
            mute = 1;
        }
        else if (!stricmp(argv[i], "-s"))
        {
            if (++i >= argc) goto BadUsageExit;
            output_width = atoi(argv[i++]);
            if (i >= argc) goto BadUsageExit;
            output_height = atoi(argv[i++]);
            if (output_width <= 0 || output_height <= 0)
            {
                fprintf(stderr, "Bad size value parsed out: %d, %d\n", output_width, output_height);
                goto BadUsageExit;
            }
        }
        else if (!stricmp(argv[i], "-b"))
        {
            i++;
            no_colors = 1;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            goto BadUsageExit;
        }
    }

    if (!input_file)
    {
        goto BadUsageExit;
    }

    if (!fp_out) fp_out = stdout;

    fv = fv_create(input_file, stderr, verbose, fp_out, output_width, output_height, prerender_secs, !mute);
    if (!fv) goto FailExit;

    if (no_colors) fv->do_colored_output = 0;

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
