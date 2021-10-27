#include<stdio.h>

#include"fontvideo.h"

int main(int argc, char **argv)
{
    char *input_file = NULL;
    fontvideo_p fv = NULL;
    FILE *fp_out = NULL;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input> [output]\n", argv[0]);
        return 1;
    }

    input_file = argv[1];

    if (argc >= 3)
    {
        fp_out = fopen(argv[2], "w");
        if (!fp_out)
        {
            perror("Trying to open output file");
        }
    }
    else
    {
        fp_out = stdout; 
    }

    fv = fv_create(input_file, stderr, fp_out, 80, 25, 5);
    if (!fv) goto FailExit;

    fv_show(fv);

    fv_destroy(fv);
    if (fp_out != stdout && fp_out != NULL) fclose(fp_out);
    return 0;
FailExit:
    fv_destroy(fv);
    if (fp_out != stdout && fp_out != NULL) fclose(fp_out);
    return 2;
}
