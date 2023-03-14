#include "aviwriter.h"

#include<stdlib.h>
#include<string.h>

typedef struct AVIStreamHeader_struct{
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
}AVIStreamHeader_t, *AVIStreamHeader_p;

AVIWriter_p AVIWriterStart
(
	const char* write_to,
	uint32_t framerate,
	BitmapInfoHeader_p video_format,
	uint32_t* palette,
	uint32_t* bitfields,
	WaveFormatEx_p audio_format
)
{
	AVIWriter_p aw;

	if (!video_format) return NULL;

	aw = malloc(sizeof * aw);
	if (!aw) return aw;
	memset(aw, 0, sizeof * aw);

	aw->VideoFormat = *video_format;
	if (video_format->biBitCount <= 8)
	{
		aw->PaletteUsed = min(1 << video_format->biBitCount, video_format->biClrUsed);
		memcpy(&aw->VideoPalette, palette, aw->PaletteUsed * sizeof palette[0]);
	}
	else if (video_format->biCompression == BI_Bitfields)
	{
		memcpy(&aw->Bitfields, bitfields, sizeof aw->Bitfields);
	}

	aw->Framerate = framerate;

	if (audio_format)
	{
		aw->AudioFormat = *audio_format;
		aw->HaveAudio = 1;
	}

	aw->fp = fopen(write_to, "wb");
	if (!aw->fp) goto FailExit;

	/*
	RIFF('AVI '
		LIST('hdrl'
			'avih'(AVIMAINHEADER)
			LIST('strl'
				'strh'(AVIStreamHeader)
				'strf'(BITMAPINFO)
			)
			LIST('strl'
				'strh'(AVIStreamHeader)
				'strf'(WAVEFORMATEX)
			)
		)
		LIST('movi'
			'00dc'(bitmap)
			'01wb'(pcm)
			'01wb'(pcm)
			'00dc'(bitmap)
			'01wb'(pcm)
			'01wb'(pcm)
			'00dc'(bitmap)
			'01wb'(pcm)
			'01wb'(pcm)
			...
		)
	)
	*/

	return aw;
FailExit:
	free(aw);
	return NULL;
}

int AVIWriterWriteVideo(AVIWriter_p aw, void* video_data /* the whole bitmap picture */)
{
	if (!fwrite("00dc", 4, 1, aw->fp)) return 0;
	if (!fwrite(&aw->Framesize, 4, 1, aw->fp)) return 0;
	return fwrite(video_data, aw->Framesize, 1, aw->fp);
}

int AVIWriterWriteAudio(AVIWriter_p aw, void* audio_data, uint32_t audio_size_bytes)
{
	if (!fwrite("01wb", 4, 1, aw->fp)) return 0;
	if (!fwrite(&audio_size_bytes, 4, 1, aw->fp)) return 0;
	return fwrite(audio_data, audio_size_bytes, 1, aw->fp);
}

int AVIWriterFinish(AVIWriter_p* paw)
{
	if (paw)
	{
		AVIWriter_p aw = *paw;
		*paw = NULL;
		if (!aw) return 0;
		if (!aw->fp)
		{
			free(aw);
			return 0;
		}






		fclose(aw->fp);
		free(aw);
		return 1;
	}
	return 0;
}

