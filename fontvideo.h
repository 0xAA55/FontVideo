#ifndef _FONTVIDEO_H_
#define _FONTVIDEO_H_ 1

#include"avdec.h"
#include"siowrap.h"

#include<stdio.h>

typedef struct fontvideo_frame_struct
{
    double timestamp;
    uint32_t w, h;
    uint32_t *data;
    uint32_t **row;
}fontvideo_frame_t, *fontvideo_frame_p;

typedef struct fontvideo_struct
{
    void *userdata;
    FILE *log_fp;
    int output_utf8;

    uint32_t w, h;

    avdec_p av;
    siowrap_p sio;
}fontvideo_t, *fontvideo_p;

fontvideo_p fv_create(char *input_file, FILE *log_fp, uint32_t x_resolution, uint32_t y_resolution);
int fv_poll_show(fontvideo_p fv, FILE *graphics_out_fp);
void fv_destroy(fontvideo_p fv);

#endif
