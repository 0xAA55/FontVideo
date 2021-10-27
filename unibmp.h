#ifndef _UNIBMP_H_
#define _UNIBMP_H_ 1

#include <stdint.h>
#include <stdio.h>

// ���뵽�ڴ��λͼ�ļ�����
// ���е�λͼ��ʽ�ڱ��������󣬶��ᱻת��Ϊ ARGB ��ʽ��ÿͨ�� 8 bit λ�
// UniformBitmap��Ϊ����ʽͳһ�˵�λͼ��
typedef struct UniformBitmap_struct
{
	// λͼ��Ϣ
	uint32_t Width;
	uint32_t Height;

	// λͼDPI��Ϣ
	uint32_t XPelsPerMeter;
	uint32_t YPelsPerMeter;

	// λͼ����
	uint32_t *BitmapData;

	// λͼ���ݵ���ָ��
	uint32_t **RowPointers;
}UniformBitmap_t, *UniformBitmap_p;

// ���ļ�����λͼ
// λͼ��������RLEѹ������λͼ�����Ǵ�λ���λͼ������ɫ���������ɫλͼ��
// ��������ͼ�����ݻᱻǿ��ת��Ϊ��ARGB ��ʽ��ÿͨ�� 8 bit λ�ÿ������4�ֽڣ��ֱ��ǣ������̣��죬Alpha
// �������ͼ���Alphaͨ����Ϊ0����������ͼ�񲻰���Alphaͨ�������������λͼ��Alphaͨ���ᱻ����Ϊ���ֵ���� 255��
UniformBitmap_p UB_CreateFromFile(const char *FilePath, FILE *log_fp);

int UB_SaveToFile_24(UniformBitmap_p UB, const char *FilePath);
int UB_SaveToFile_32(UniformBitmap_p UB, const char *FilePath);

// �ͷ�λͼ��Դ
void UB_Free(UniformBitmap_p *pUB);

#endif
