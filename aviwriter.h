#ifndef _AVI_WRITER_H_
#define _AVI_WRITER_H_ 1

#include<stdint.h>
#include<stdio.h>

#include<unibmp.h>

typedef struct WaveFormatEx_struct
{
	uint16_t  FormatTag;
	uint16_t  Channels;
	uint32_t  SamplesPerSec;
	uint32_t AvgBytesPerSec;
	uint16_t  BlockAlign;
	uint16_t  BitsPerSample;
}WaveFormatEx_t, * WaveFormatEx_p;

typedef struct AVIWriter_struct
{
	FILE* fp;

	BitmapInfoHeader_t VideoFormat;
	uint32_t VideoPalette[256];
	int PaletteUsed;
	uint32_t Bitfields[3];
	int HaveAudio;
	WaveFormatEx_t AudioFormat;

	uint32_t Framerate;
	uint32_t Framesize;
	size_t VideoWritten;
	size_t AudioWritten;
}AVIWriter_t, * AVIWriter_p;

AVIWriter_p AVIWriterStart
(
	const char *write_to,
	uint32_t framerate,
	BitmapInfoHeader_p video_format,
	uint32_t *palette,
	uint32_t *bitfields,
	WaveFormatEx_p audio_format
);

int AVIWriterWriteVideo(AVIWriter_p aw, void* video_data /* the whole bitmap picture */);
int AVIWriterWriteAudio(AVIWriter_p aw, void* audio_data, uint32_t audio_size_bytes);

double AVIWriterGetCurrentVideoTimestamp(AVIWriter_p aw);
double AVIWriterGetCurrentAudioTimestamp(AVIWriter_p aw);

int AVIWriterFinish(AVIWriter_p* paw);










#endif
