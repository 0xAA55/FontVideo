#include "aviwriter.h"

#include<stdlib.h>
#include<string.h>

static size_t get_pitch(size_t width, size_t bits)
{
	return ((width * bits - 1) / 32 + 1) * 4;
}

static int WriteBitmapHeader(AVIWriter_p aw)
{
	FILE* fp = aw->fp;
	if (!fwrite(&aw->VideoFormat, sizeof aw->VideoFormat, 1, fp)) return 0;
	if (aw->PaletteUsed)
	{
		if (!fwrite(&aw->VideoPalette, sizeof aw->VideoPalette[0] * aw->PaletteUsed, 1, fp)) return 0;
	}
	else if (aw->BitfieldUsed)
	{
		if (!fwrite(&aw->Bitfields, sizeof aw->Bitfields, 1, fp)) return 0;
	}
	return 1;
}

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
	FILE* fp;
	int32_t Width;
	int32_t Height;
	uint32_t ListSize_HDRL;
	uint32_t ListSize_STRL_V;
	uint32_t ListSize_STRL_A;
	uint32_t buf;
	size_t Pitch;
	size_t Framesize;
	AVIMainHeader_t AVIH = { 0 };
	AVIStreamHeader_t STRH_Video = { 0 };
	AVIStreamHeader_t STRH_Audio = { 0 };

	if (!video_format) return NULL;

	Width = video_format->biWidth;
	Height = video_format->biHeight < 0 ? -video_format->biHeight : video_format->biHeight;
	Pitch = get_pitch(Width, video_format->biBitCount);
	Framesize = Pitch * Height;

	aw = malloc(sizeof * aw);
	if (!aw) return aw;
	memset(aw, 0, sizeof * aw);
	fp = NULL;

	aw->Width = Width;
	aw->Height = Height;

	aw->VideoFormat = *video_format;
	aw->BitmapHeaderSize = sizeof aw->VideoFormat;
	if (video_format->biBitCount <= 8)
	{
		aw->PaletteUsed = min(1 << video_format->biBitCount, video_format->biClrUsed);
		memcpy(&aw->VideoPalette, palette, aw->PaletteUsed * sizeof palette[0]);
		aw->BitmapHeaderSize += aw->PaletteUsed * sizeof palette[0];
	}
	else if (video_format->biCompression == BI_Bitfields)
	{
		aw->BitfieldUsed = 3;
		memcpy(&aw->Bitfields, bitfields, sizeof aw->Bitfields);
		aw->BitmapHeaderSize += sizeof aw->Bitfields;
	}

	if (audio_format)
	{
		aw->AudioFormat = *audio_format;
		aw->HaveAudio = 1;
	}

	aw->Framerate = framerate;
	aw->Framesize = Framesize;

	/*
	RIFF(AVI_
		LIST('hdrl'
			avih(AVIMAINHEADER)
			LIST('strl'
				strh(AVIStreamHeader)
				strf(BITMAPINFO)
			)
			LIST('strl'
				strh(AVIStreamHeader)
				strf(WAVEFORMATEX)
			)
		)
		LIST('movi'
			00dc(bitmap)
			01wb(pcm)
			01wb(pcm)
			00dc(bitmap)
			01wb(pcm)
			01wb(pcm)
			00dc(bitmap)
			01wb(pcm)
			01wb(pcm)
			...
		)
	)
	*/

	ListSize_STRL_V = 4 + // 'strl'
		8 + // 'strh'
		sizeof(AVIStreamHeader_t) +
		8 + // 'strf'
		aw->BitmapHeaderSize;
	ListSize_STRL_A = !aw->HaveAudio ? 0 : 4 + // 'strl'
		8 + // 'strh'
		sizeof(AVIStreamHeader_t) +
		8 + // 'strf'
		sizeof(WaveFormatEx_t);
	ListSize_HDRL =
		4 + // 'hdrl'
		sizeof(AVIMainHeader_t) +
		8 + // LIST
		4 + // 'strl'
		ListSize_STRL_V;
	if (aw->HaveAudio)
		ListSize_HDRL +=
		8 + // LIST
		4 + // 'strl'
		ListSize_STRL_A;
	aw->fp = fp = fopen(write_to, "wb"); if (!fp) goto FailExit;
	if (!fwrite("RIFF\0\0\0\0", 8, 1, fp))goto FailExit;
	if (!fwrite("AVI ", 4, 1, fp))goto FailExit;
	if (!fwrite("LIST", 4, 1, fp))goto FailExit;
	if (!fwrite(&ListSize_HDRL, 4, 1, fp))goto FailExit;
	if (!fwrite("hdrl", 4, 1, fp))goto FailExit;
	aw->OffsetToAVIH = ftell(fp);
	if (!fwrite(&AVIH, sizeof AVIH, 1, fp))goto FailExit;
	if (!fwrite("LIST", 4, 1, fp))goto FailExit;
	if (!fwrite(&ListSize_STRL_V, 4, 1, fp))goto FailExit;
	if (!fwrite("strl", 4, 1, fp))goto FailExit;
	if (!fwrite("strh", 4, 1, fp))goto FailExit;
	buf = sizeof(AVIStreamHeader_t);
	if (!fwrite(&buf, 4, 1, fp))goto FailExit;
	aw->OffsetToSTRH_Video = ftell(fp);
	if (!fwrite(&STRH_Video, sizeof STRH_Video, 1, fp))goto FailExit;
	if (!fwrite("strf", 4, 1, fp))goto FailExit;
	buf = aw->BitmapHeaderSize;
	if (!fwrite(&buf, 4, 1, fp))goto FailExit;
	if (!WriteBitmapHeader(aw))goto FailExit;
	if (aw->HaveAudio)
	{
		if (!fwrite("LIST", 4, 1, fp))goto FailExit;
		if (!fwrite(&ListSize_STRL_A, 4, 1, fp))goto FailExit;
		if (!fwrite("strl", 4, 1, fp))goto FailExit;
		if (!fwrite("strh", 4, 1, fp))goto FailExit;
		buf = sizeof(AVIStreamHeader_t);
		if (!fwrite(&buf, 4, 1, fp))goto FailExit;
		aw->OffsetToSTRH_Audio = ftell(fp);
		if (!fwrite(&STRH_Audio, sizeof STRH_Audio, 1, fp))goto FailExit;
		if (!fwrite("strf", 4, 1, fp))goto FailExit;
		buf = sizeof aw->AudioFormat;
		if (!fwrite(&buf, 4, 1, fp))goto FailExit;
		if (!fwrite(&aw->AudioFormat, sizeof aw->AudioFormat, 1, fp))goto FailExit;
	}
	if (!fwrite("LIST", 4, 1, fp))goto FailExit;
	buf = 0;
	if (!fwrite(&buf, 4, 1, fp))goto FailExit;
	if (!fwrite("movi", 4, 1, fp))goto FailExit;
	aw->OffsetToFirstFrame = ftell(fp);

	return aw;
FailExit:
	if (fp) fclose(fp);
	free(aw);
	return NULL;
}

int AVIWriterWriteVideo(AVIWriter_p aw, void* video_data /* the whole bitmap picture */)
{
	if (!aw || !video_data) return 0;
	if (!fwrite("00dc", 4, 1, aw->fp)) return 0;
	if (!fwrite(&aw->Framesize, 4, 1, aw->fp)) return 0;
	if (!fwrite(video_data, aw->Framesize, 1, aw->fp)) return 0;
	aw->VideoWritten++;
	return 1;
}

int AVIWriterWriteAudio(AVIWriter_p aw, void* audio_data, uint32_t audio_size_bytes)
{
	if (!aw || !audio_data || !audio_size_bytes) return 0;
	if (!fwrite("01wb", 4, 1, aw->fp)) return 0;
	if (!fwrite(&audio_size_bytes, 4, 1, aw->fp)) return 0;
	if (!fwrite(audio_data, audio_size_bytes, 1, aw->fp)) return 0;
	aw->AudioWritten += audio_size_bytes;
	return 1;
}

double AVIWriterGetCurrentVideoTimestamp(AVIWriter_p aw)
{
	if (!aw) return 0;
	return (double)aw->VideoWritten / aw->Framerate;
}

double AVIWriterGetCurrentAudioTimestamp(AVIWriter_p aw)
{
	if (!aw || !aw->HaveAudio) return 0;
	return (double)aw->AudioWritten / aw->AudioFormat.AvgBytesPerSec;
}

int AVIWriterFinish(AVIWriter_p* paw)
{
	AVIWriter_p aw;
	int ret = 0;
	if (!paw) return 0;

	aw = *paw;
	*paw = NULL;
	if (!aw) return 0;
	if (!aw->fp)
	{
		free(aw);
		return 0;
	}

	do
	{
		AVIMainHeader_t AVIH = { 0 };
		AVIStreamHeader_t STRH_Video = { 0 };
		AVIStreamHeader_t STRH_Audio = { 0 };
		long FileSize;
		int32_t buf;
		FILE* fp = aw->fp;

		AVIH.FourCC = 0x68697661;
		AVIH.cb = sizeof AVIH - 8;
		AVIH.Width = aw->Width;
		AVIH.Height = aw->Height;
		AVIH.MicroSecPerFrame = 1000000 / aw->Framerate;
		AVIH.MaxBytesPerSec = aw->Framesize * aw->Framerate + aw->AudioFormat.AvgBytesPerSec;
		AVIH.Flags = aw->HaveAudio ? AVIF_ISINTERLEAVED : 0;
		AVIH.Streams = aw->HaveAudio ? 2 : 1;
		AVIH.SuggestedBufferSize = aw->Framesize;

		STRH_Video.FourCC = 0x73646976;
		STRH_Video.InitialFrames = AVIH.InitialFrames;
		STRH_Video.Scale = 1;
		STRH_Video.Rate = aw->Framerate;
		STRH_Video.Length = aw->VideoWritten;
		STRH_Video.SuggestedBufferSize = aw->Framesize;
		STRH_Video.Quality = -1;
		STRH_Video.Frame[0] = 0;
		STRH_Video.Frame[1] = 0;
		STRH_Video.Frame[2] = aw->Width;
		STRH_Video.Frame[3] = aw->Height;

		if (aw->HaveAudio)
		{
			STRH_Audio.FourCC = 0x73647561;
			STRH_Audio.Handler = aw->AudioFormat.FormatTag;
			STRH_Audio.Scale = 1;
			STRH_Audio.Rate = aw->AudioFormat.SamplesPerSec;
			STRH_Audio.Length = aw->AudioWritten / aw->AudioFormat.BlockAlign;
			STRH_Audio.SuggestedBufferSize = 256;
			STRH_Audio.Quality = -1;
			STRH_Audio.SampleSize = aw->AudioFormat.BlockAlign;
		}

		FileSize = ftell(fp);
		buf = FileSize - 8;
		if (fseek(fp, 4, SEEK_SET) != 0) break;
		if (!fwrite(&buf, 4, 1, fp))break;

		if (fseek(fp, aw->OffsetToAVIH, SEEK_SET) != 0) break;
		if (!fwrite(&AVIH, sizeof AVIH, 1, fp))break;

		if (fseek(fp, aw->OffsetToSTRH_Video, SEEK_SET) != 0) break;
		if (!fwrite(&STRH_Video, sizeof STRH_Video, 1, fp))break;

		if (aw->HaveAudio)
		{
			if (fseek(fp, aw->OffsetToSTRH_Audio, SEEK_SET) != 0) break;
			if (!fwrite(&STRH_Audio, sizeof STRH_Audio, 1, fp))break;
		}

		buf = FileSize - aw->OffsetToFirstFrame;
		if (fseek(fp, aw->OffsetToFirstFrame - 8, SEEK_SET) != 0) break;
		if (!fwrite(&buf, 4, 1, fp))break;

		ret = 1;
	} while (0);
	fclose(aw->fp);
	free(aw);
	return ret;
}

