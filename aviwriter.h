#ifndef _AVI_WRITER_H_
#define _AVI_WRITER_H_ 1

#include<stdint.h>
#include<stdio.h>

#include<unibmp.h>

#define AVIF_HASINDEX           0x00000010
#define AVIF_MUSTUSEINDEX       0x00000020
#define AVIF_ISINTERLEAVED      0x00000100
#define AVIF_TRUSTCKTYPE        0x00000800
#define AVIF_WASCAPTUREFILE     0x00010000
#define AVIF_COPYRIGHTED        0x00020000

#pragma pack(push, 1)

typedef struct AVIMainHeader_struct {
	uint32_t FourCC;
	uint32_t cb;
	uint32_t MicroSecPerFrame;
	uint32_t MaxBytesPerSec;
	uint32_t PaddingGranularity;
	uint32_t Flags;
	uint32_t TotalFrames;
	uint32_t InitialFrames;
	uint32_t Streams;
	uint32_t SuggestedBufferSize;
	uint32_t Width;
	uint32_t Height;
	uint32_t Reserved[4];
} AVIMainHeader_t, * AVIMainHeader_p;

typedef struct AVIStreamHeader_struct {
	uint32_t FourCC;
	uint32_t Handler;
	uint32_t Flags;
	uint16_t Priority;
	uint16_t Language;
	uint32_t InitialFrames;
	uint32_t Scale;
	uint32_t Rate;
	uint32_t Start;
	uint32_t Length;
	uint32_t SuggestedBufferSize;
	uint32_t Quality;
	uint32_t SampleSize;
	int16_t  Frame[4];
}AVIStreamHeader_t, * AVIStreamHeader_p;

typedef struct WaveFormatEx_struct
{
	uint16_t  FormatTag;
	uint16_t  Channels;
	uint32_t  SamplesPerSec;
	uint32_t AvgBytesPerSec;
	uint16_t  BlockAlign;
	uint16_t  BitsPerSample;
}WaveFormatEx_t, * WaveFormatEx_p;

#pragma pack(pop)

typedef struct AVIWriter_struct
{
	FILE* fp;
	int32_t Width;
	int32_t Height;

	BitmapInfoHeader_t VideoFormat;
	uint32_t VideoPalette[256];
	int PaletteUsed;
	int BitfieldUsed;
	uint32_t Bitfields[3];
	uint32_t BitmapHeaderSize;
	int HaveAudio;
	WaveFormatEx_t AudioFormat;
	int OffsetToAVIH;
	int OffsetToSTRH_Video;
	int OffsetToSTRH_Audio;
	int OffsetToFirstFrame;

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
