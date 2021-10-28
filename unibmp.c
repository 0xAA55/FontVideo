#include"unibmp.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>

#pragma pack(push, 1)

// λͼ�ļ�ͷ
typedef struct BitmapFileHeader_struct
{
	uint16_t bfType;
	uint32_t bfSize;
	uint16_t bfReserved1;
	uint16_t bfReserved2;
	uint32_t bfOffbits;
}BitmapFileHeader_t, *BitmapFileHeader_p;

// λͼ��Ϣͷ
typedef struct BitmapInfoHeader_struct
{
	uint32_t biSize;
	int32_t biWidth;
	int32_t biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	uint32_t biXPelsPerMeter;
	uint32_t biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
}BitmapInfoHeader_t, *BitmapInfoHeader_p;

#pragma pack(pop)

enum BitmapCompression
{
	BI_RGB = 0,
	BI_RLE8 = 1,
	BI_RLE4 = 2,
	BI_Bitfields = 3
};

// ȡ��λ��λ��
static uint32_t GetBitfieldShift(uint32_t Bitfield)
{
	uint32_t Shift = 0;
	if (!Bitfield) return 0;
	while (!(Bitfield & 1))
	{
		Shift++;
		Bitfield >>= 1;
	}
	while (Bitfield & 1)
	{
		Shift++;
		Bitfield >>= 1;
	}
	return Shift - 8;
}

// ȡ��λ��λ��
static uint32_t GetBitfieldBitCount(uint32_t Bitfield)
{
	uint32_t Count = 0;
	if (!Bitfield) return 0;
	while (!(Bitfield & 1)) Bitfield >>= 1;
	while (Bitfield & 1)
	{
		Count++;
		Bitfield >>= 1;
	}
	return Count;
}

// ������ɫֵ
static uint32_t ARGB(uint8_t R, uint8_t G, uint8_t B, uint8_t A)
{
	union Pixel
	{
		uint8_t u8[4];
		uint32_t u32;
	}Block = { B, G, R, A};
	return Block.u32;
}

// ���ļ�����λͼ
// λͼ��������RLEѹ������λͼ�����Ǵ�λ���λͼ������ɫ���������ɫλͼ��
// ��������ͼ�����ݻᱻǿ��ת��Ϊ��ARGB ��ʽ��ÿͨ�� 8 bit λ�ÿ������4�ֽڣ��ֱ��ǣ������̣��죬Alpha
// �������ͼ���Alphaͨ����Ϊ0����������ͼ�񲻰���Alphaͨ�������������λͼ��Alphaͨ���ᱻ����Ϊ���ֵ���� 255��
UniformBitmap_p UB_CreateFromFile(const char *FilePath, FILE *log_fp)
{
	BitmapFileHeader_t BMFH;
	BitmapInfoHeader_t BMIF;
	UniformBitmap_p UB = NULL;
	size_t Pitch; // ԭλͼ�ļ�ÿ�����ص����ֽ������������룩
	uint32_t Palette[256]; // ��ɫ�壬����У�Ҫ����
	unsigned PaletteColorCount = 0;
	uint32_t Bitfields[3]; // λ������У�Ҫ����
	uint32_t Bitfield_A = 0; // ͸��ͨ����λ��ͨ��û��͸��ͨ����
	uint8_t *ReadInLineBuffer = NULL;
	size_t i;

	// ׼����ȡ�ļ�
	FILE *fp = fopen(FilePath, "rb");
	if (!fp)
	{
		fprintf(log_fp, "�򲻿��ļ���%s ��%s��\n",
			FilePath, strerror(errno));
		return NULL;
	}

	// ��ȡλͼ�ļ�ͷ
	if (fread(&BMFH, sizeof BMFH, 1, fp) != 1)
	{
		fprintf(log_fp, "��ȡλͼ�ļ� %s ��λͼ�ļ�ͷʧ�ܣ�%s\n",
			FilePath, strerror(errno));
		goto FailExit;
	}

	// �ж�ͷ���ֶ��Ƿ����
	if (BMFH.bfType != 0x4D42 || !BMFH.bfOffbits)
	{
		fprintf(log_fp, "���Ų���һ��BMPλͼ�ļ���");
		goto FailExit;
	}

	// ��ȡλͼ��Ϣͷ
	if (fread(&BMIF, sizeof BMIF, 1, fp) != 1)
	{
		fprintf(log_fp, "��ȡλͼ�ļ� %s ��λͼ��Ϣͷʧ�ܣ�%s\n",
			FilePath, strerror(errno));
		goto FailExit;
	}

	if (!BMIF.biPlanes || !BMIF.biWidth || !BMIF.biHeight)
	{
		fprintf(log_fp, "λͼ�ļ� %s ��λͼ��Ϣͷ�����ݲ�����\n", FilePath);
		goto FailExit;
	}

	// �ж�λͼ��ѹ����ʽ
	switch (BMIF.biCompression)
	{
	case BI_Bitfields:
		// ��λ�򣬶�ȡλ����Ϣ
		if (fread(&Bitfields, sizeof Bitfields, 1, fp) != 1)
		{
			fprintf(log_fp, "��ȡλͼ�ļ� %s ��λ����Ϣʧ�ܣ�%s\n",
				FilePath, strerror(errno));
			goto FailExit;
		}
		break;
	case BI_RLE4:
	case BI_RLE8:
		fprintf(log_fp, "λͼ�ļ� %s ʹ����RLEѹ�����ݲ�֧��RLEѹ����λͼ������Ū��\n", FilePath);
		goto FailExit;
	case BI_RGB:
		// û��λ�򣬵����е�ɫ�壬��ȡ��ɫ����Ϣ
		switch (BMIF.biBitCount)
		{
		case 1: case 2: case 4: case 8:
			if (BMIF.biClrUsed) PaletteColorCount = BMIF.biClrUsed;
			else PaletteColorCount = (1u << BMIF.biBitCount);
			if (fread(&Palette, 4, PaletteColorCount, fp) != PaletteColorCount)
			{
				fprintf(log_fp, "��ȡλͼ�ļ� %s �ĵ�ɫ����Ϣʧ�ܣ�%s\n",
					FilePath, strerror(errno));
				goto FailExit;
			}
			// ����ɫ���Alphaͨ���Ƿ��������
			for (i = 0; i < PaletteColorCount; i++)
			{
				if (Palette[i] & 0xff000000) break;
			}
			// Alphaͨ������������ʱ����Alphaͨ������Ϊ255
			if (i == PaletteColorCount)
			{
				for (i = 0; i < PaletteColorCount; i++)
				{
					Palette[i] |= 0xff000000;
				}
			}
			break;
		case 16:
			Bitfields[0] = 0x7c00;
			Bitfields[1] = 0x03e0;
			Bitfields[2] = 0x001f;
			Bitfield_A = 0x8000;
			break;
		case 24:
		case 32:
			Bitfields[0] = 0xff0000;
			Bitfields[1] = 0x00ff00;
			Bitfields[2] = 0x0000ff;
			break;
		default:
			fprintf(log_fp, "λͼ�ļ� %s ��λͼ��Ϣͷ�����ݲ�������ɫλ���� %d��\n", FilePath, (int)BMIF.biBitCount);
			goto FailExit;
		}
		break;
	default:
		fprintf(log_fp, "λͼ�ļ� %s ��λͼ��Ϣͷ�����ݲ�����\n", FilePath);
		goto FailExit;
	}

	// �����ļ�ָ��ָ��λͼ����
	fseek(fp, BMFH.bfOffbits, SEEK_SET);

	// �����ڴ棬�洢������ͼ�����Ϣ
	UB = malloc(sizeof UB[0]);
	if (!UB)
	{
		perror("��Ӧ��ѽ");
		goto FailExit;
	}

	UB->Width = BMIF.biWidth;
	UB->Height = BMIF.biHeight < 0 ? -BMIF.biHeight : BMIF.biHeight;

	// ����DPI��Ϣ
	UB->XPelsPerMeter = BMIF.biXPelsPerMeter;
	UB->YPelsPerMeter = BMIF.biYPelsPerMeter;

	// λͼ����Buffer
	UB->BitmapData = malloc((size_t)UB->Width * UB->Height * sizeof UB->BitmapData[0]);
	if (!UB->BitmapData)
	{
		perror("�����ڴ����ڴ洢λͼ����");
		goto FailExit;
	}
	UB->RowPointers = malloc(UB->Height * sizeof UB->RowPointers[0]);
	if (!UB->RowPointers)
	{
		perror("�����ڴ����ڴ洢λͼ������ָ��");
		goto FailExit;
	}
	// λͼ�Ĵ洢�ȿ�������������Ҳ�����ǵߵ�����
	if (BMIF.biHeight < 0)
	{
		for (i = 0; i < UB->Height; i++)
		{
			UB->RowPointers[i] = &UB->BitmapData[i * UB->Width];
		}
	}
	else
	{
		for (i = 0; i < UB->Height; i++)
		{
			UB->RowPointers[i] = &UB->BitmapData[(UB->Height - i - 1) * UB->Width];
		}
	}

	Pitch = ((size_t)(BMIF.biWidth * BMIF.biBitCount - 1) / 32 + 1) * 4;
	ReadInLineBuffer = malloc(Pitch);
	if (!ReadInLineBuffer)
	{
		perror("�����ڴ����ڰ��ж�ȡԴλͼ��");
		goto FailExit;
	}

	// ��ʼ��ȡλͼ
	switch (BMIF.biCompression)
	{
		// ԭʼλͼ������λ����Ϣ
	case BI_RGB:
	{
		// ����λ���ж��Ƿ�Ϊ��ɫ����ɫ
		switch (BMIF.biBitCount)
		{
			// ÿ���ֽڿ��ܰ����������
		case 1: case 2: case 4:
		{
			uint32_t x, y;
			uint32_t ShiftCount = 8 - BMIF.biBitCount;
			for (y = 0; y < UB->Height; y++)
			{
				uint32_t PixelsPerBytes = 8 / BMIF.biBitCount;
				uint8_t LastByte = 0;
				uint32_t BytePosition = 0;
				uint32_t PalIndex;
				uint32_t PixelsRemainLastByte = 0;
				uint32_t *Row = UB->RowPointers[y];
				if (fread(ReadInLineBuffer, 1, Pitch, fp) != Pitch)
				{
					perror("δ��ȡ����λͼ��");
					goto FailExit;
				}
				for (x = 0; x < UB->Width; x++)
				{
					if (!PixelsRemainLastByte)
					{
						PixelsRemainLastByte = PixelsPerBytes;
						LastByte = ReadInLineBuffer[BytePosition++];
					}
					PalIndex = LastByte >> ShiftCount;
					LastByte <<= BMIF.biBitCount;
					PixelsRemainLastByte--;
					Row[x] = Palette[PalIndex];
				}
			}
			break;
		}
		// һ���ֽ�һ�����أ��ֽ�ֵ��Ϊ��������
		case 8:
		{
			uint32_t x, y;
			for (y = 0; y < UB->Height; y++)
			{
				uint32_t *Row = UB->RowPointers[y];
				if (fread(ReadInLineBuffer, 1, Pitch, fp) != Pitch)
				{
					perror("δ��ȡ����λͼ��");
					goto FailExit;
				}
				for (x = 0; x < UB->Width; x++)
				{
					Row[x] = Palette[ReadInLineBuffer[x]];
				}
			}
			break;
		}
		// ��������ɫ��ÿ16��bit���մӸߵ��� 1:5:5:5 �洢 ARGB �ĸ�ͨ��
		case 16:
		{
			uint32_t x, y;
			int HasAlpha = 0;
			for (y = 0; y < UB->Height; y++)
			{
				uint32_t *Row = UB->RowPointers[y];
				if (fread(ReadInLineBuffer, 1, Pitch, fp) != Pitch)
				{
					perror("δ��ȡ����λͼ��");
					goto FailExit;
				}
				for (x = 0; x < UB->Width; x++)
				{
					uint32_t PixelData = ((uint16_t *)ReadInLineBuffer)[x * 2];
					uint32_t R5 = (PixelData & 0x7c00) >> 10;
					uint32_t G5 = (PixelData & 0x03e0) >> 5;
					uint32_t B5 = (PixelData & 0x001f) >> 0;

					Row[x] = ARGB(
						(uint8_t)((R5 << 3) | (R5 >> 2)),
						(uint8_t)((G5 << 3) | (G5 >> 2)),
						(uint8_t)((B5 << 3) | (B5 >> 2)),
						(uint8_t)(PixelData & 0x8000 ? (HasAlpha = 1) * 255 : 0));
				}
			}
			if (!HasAlpha)
			{
				for (y = 0; y < UB->Height; y++)
				{
					uint32_t *Row = UB->RowPointers[y];
					for (x = 0; x < UB->Width; x++)
					{
						Row[x] |= 0xff000000;
					}
				}
			}
			break;
		}
		// ��������ɫ��ÿͨ��1�ֽ�
		case 24:
		{
			uint32_t x, y;
			for (y = 0; y < UB->Height; y++)
			{
				uint32_t *Row = UB->RowPointers[y];
				if (fread(ReadInLineBuffer, 1, Pitch, fp) != Pitch)
				{
					perror("δ��ȡ����λͼ��");
					goto FailExit;
				}
				for (x = 0; x < UB->Width; x++)
				{
					uint32_t ByteIndex = x * 3;
					Row[x] = ARGB(ReadInLineBuffer[ByteIndex + 2],
						ReadInLineBuffer[ByteIndex + 1],
						ReadInLineBuffer[ByteIndex + 0],
						255);
				}
			}
			break;
		}
		// ��������ɫ��ÿͨ��1�ֽڣ����ܰ���Alphaͨ��
		case 32:
		{
			uint32_t x, y;
			int HasAlpha = 0;
			for (y = 0; y < UB->Height; y++)
			{
				uint32_t *Row = UB->RowPointers[y];
				if (fread(ReadInLineBuffer, 1, Pitch, fp) != Pitch)
				{
					perror("δ��ȡ����λͼ��");
					goto FailExit;
				}
				for (x = 0; x < UB->Width; x++)
				{
					uint32_t ByteIndex = x * 4;
					Row[x] = ARGB(ReadInLineBuffer[ByteIndex + 2],
						ReadInLineBuffer[ByteIndex + 1],
						ReadInLineBuffer[ByteIndex + 0],
						ReadInLineBuffer[ByteIndex + 3]);
					if (ReadInLineBuffer[ByteIndex + 3]) HasAlpha = 1;
				}
			}
			break;
		}
		}
		break;
	}

	case BI_Bitfields:
	{
		uint32_t RShift = GetBitfieldShift(Bitfields[0]);
		uint32_t GShift = GetBitfieldShift(Bitfields[1]);
		uint32_t BShift = GetBitfieldShift(Bitfields[2]);
		uint32_t AShift = GetBitfieldShift(Bitfield_A);
		uint32_t RBitCount = GetBitfieldBitCount(Bitfields[0]);
		uint32_t GBitCount = GetBitfieldBitCount(Bitfields[1]);
		uint32_t BBitCount = GetBitfieldBitCount(Bitfields[2]);
		uint32_t ABitCount = GetBitfieldBitCount(Bitfield_A);
		uint32_t BytesPerPixels = BMIF.biBitCount / 8;
		uint32_t x, y;

		if (!BytesPerPixels)
		{
			fprintf(log_fp, "����λ����Ϣ��λͼ��ÿ����λ��Ӧ������8��\n");
			goto FailExit;
		}

		for (y = 0; y < UB->Height; y++)
		{
			uint32_t *Row = UB->RowPointers[y];
			uint8_t *PixelPointer = ReadInLineBuffer;
			if (fread(ReadInLineBuffer, 1, Pitch, fp) != Pitch)
			{
				perror("δ��ȡ����λͼ��");
				goto FailExit;
			}
			for (x = 0; x < UB->Width; x++)
			{
				// ��һ�����ص�ȫ�����ݶ��룬�ٲ�������������ͨ������ɫֵ
				uint32_t PixelData = *(uint32_t *)PixelPointer;
				uint32_t RV = PixelData & ((Bitfields[0]) >> RShift);
				uint32_t GV = PixelData & ((Bitfields[1]) >> GShift);
				uint32_t BV = PixelData & ((Bitfields[2]) >> BShift);
				uint32_t AV = PixelData & ((Bitfield_A) >> AShift);
				uint32_t RBC = RBitCount;
				uint32_t GBC = GBitCount;
				uint32_t BBC = BBitCount;
				uint32_t ABC = ABitCount;
				PixelPointer += BytesPerPixels;

				if (RBC) while (RBC < 8) { RV |= RV >> RBC; RBC *= 2; } else RV = 255;
				if (GBC) while (GBC < 8) { GV |= GV >> GBC; GBC *= 2; } else GV = 255;
				if (BBC) while (BBC < 8) { BV |= BV >> BBC; BBC *= 2; } else BV = 255;
				if (ABC) while (ABC < 8) { AV |= AV >> ABC; ABC *= 2; } else AV = 255;

				Row[x] = ARGB((uint8_t)RBC, (uint8_t)GBC, (uint8_t)BBC, (uint8_t)ABC);
			}
		}
		break;
	}

	default:
		fprintf(log_fp, "�ȷ�λ���ʽ�ַ�RGB��ʽ��ֹͣ������\n");
		goto FailExit;
	}

	fclose(fp);
	free(ReadInLineBuffer);

	return UB;
FailExit:
	if (fp) fclose(fp);
	UB_Free(&UB);
	free(ReadInLineBuffer);
	return NULL;
}

int UB_SaveToFile_24(UniformBitmap_p UB, const char *FilePath)
{
	FILE *fp = NULL;
	size_t Pitch;
	uint8_t *Buffer = NULL;
	uint32_t x, y;
	BitmapFileHeader_t BMFH;
	BitmapInfoHeader_t BMIF;

	if (!UB || !FilePath) return 0;

	fp = fopen(FilePath, "wb");
	if (!fp) return 0;

	BMIF.biSize = 40;
	BMIF.biWidth = UB->Width;
	BMIF.biHeight = UB->Height;
	BMIF.biPlanes = 1;
	BMIF.biBitCount = 24;
	BMIF.biCompression = 0;
	BMIF.biSizeImage = 0;
	BMIF.biXPelsPerMeter = 0;
	BMIF.biYPelsPerMeter = 0;
	BMIF.biClrUsed = 0;
	BMIF.biClrImportant = 0;

	Pitch = ((size_t)(BMIF.biWidth * BMIF.biBitCount - 1) / 32 + 1) * 4;

	BMFH.bfType = 0x4D42;
	BMFH.bfSize = (uint32_t)(sizeof BMFH + sizeof BMIF + Pitch * UB->Height);
	BMFH.bfReserved1 = 0;
	BMFH.bfReserved2 = 0;
	BMFH.bfOffbits = sizeof BMFH + sizeof BMIF;

	if (!fwrite(&BMFH, sizeof BMFH, 1, fp)) goto FailExit;
	if (!fwrite(&BMIF, sizeof BMIF, 1, fp)) goto FailExit;

	Buffer = malloc(Pitch);
	if (!Buffer) goto FailExit;
	memset(Buffer, 0, Pitch);
	for (y = 0; y < UB->Height; y++)
	{
		union Pixel
		{
			uint8_t u8[4];
			uint32_t u32;
		} * RowPtr = (void*)UB->RowPointers[UB->Height - 1 - y];
		uint8_t *Ptr = Buffer;
		for (x = 0; x < UB->Width; x++)
		{
			*Ptr++ = RowPtr[x].u8[0];
			*Ptr++ = RowPtr[x].u8[1];
			*Ptr++ = RowPtr[x].u8[2];
		}
		if (!fwrite(Buffer, Pitch, 1, fp)) goto FailExit;
	}

	free(Buffer);
	fclose(fp);
	return 1;
FailExit:
	free(Buffer);
	if (fp) fclose(fp);
	return 0;
}

int UB_SaveToFile_32(UniformBitmap_p UB, const char *FilePath)
{
	FILE *fp = NULL;
	size_t Pitch;
	uint32_t y;
	BitmapFileHeader_t BMFH;
	BitmapInfoHeader_t BMIF;

	if (!UB || !FilePath) return 0;

	fp = fopen(FilePath, "wb");
	if (!fp) return 0;

	BMIF.biSize = 40;
	BMIF.biWidth = UB->Width;
	BMIF.biHeight = UB->Height;
	BMIF.biPlanes = 1;
	BMIF.biBitCount = 32;
	BMIF.biCompression = 0;
	BMIF.biSizeImage = 0;
	BMIF.biXPelsPerMeter = 0;
	BMIF.biYPelsPerMeter = 0;
	BMIF.biClrUsed = 0;
	BMIF.biClrImportant = 0;

	Pitch = (size_t)BMIF.biWidth * 4;

	BMFH.bfType = 0x4D42;
	BMFH.bfSize = (uint32_t)(sizeof BMFH + sizeof BMIF + Pitch * UB->Height);
	BMFH.bfReserved1 = 0;
	BMFH.bfReserved2 = 0;
	BMFH.bfOffbits = sizeof BMFH + sizeof BMIF;

	if (!fwrite(&BMFH, sizeof BMFH, 1, fp)) goto FailExit;
	if (!fwrite(&BMIF, sizeof BMIF, 1, fp)) goto FailExit;

	for (y = 0; y < UB->Height; y++)
	{
		uint32_t *RowPtr = UB->RowPointers[UB->Height - 1 - y];
		if (!fwrite(RowPtr, Pitch, 1, fp)) goto FailExit;
	}

	fclose(fp);
	return 1;
FailExit:
	if (fp) fclose(fp);
	return 0;
}

// �ͷ�λͼ��Դ
void UB_Free(UniformBitmap_p *pUB)
{
	if (pUB && *pUB)
	{
		UniformBitmap_p UB = *pUB;
		*pUB = NULL;

		free(UB->BitmapData);
		free(UB->RowPointers);
		free(UB);
	}
}
