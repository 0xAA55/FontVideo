#ifndef _UNIBMP_H_
#define _UNIBMP_H_ 1

#include <stdint.h>
#include <stdio.h>

typedef struct UniformBitmap_struct
{
	// Size in pixels
	uint32_t Width;
	uint32_t Height;

	// DPI info
	uint32_t XPelsPerMeter;
	uint32_t YPelsPerMeter;

	// Bitmap in top-to-bottom row order
	uint32_t *BitmapData;

	// Row pointers of the bitmap
	uint32_t **RowPointers;
}UniformBitmap_t, *UniformBitmap_p;

// The alpha channel will be set to 255 if the bitmap file don't contains it.
UniformBitmap_p UB_CreateFromFile(const char *FilePath, FILE *log_fp);

// Copy
UniformBitmap_p UB_CreateFromCopy(UniformBitmap_p Source);

int UB_SaveToFile_24(UniformBitmap_p UB, const char *FilePath);
int UB_SaveToFile_32(UniformBitmap_p UB, const char *FilePath);

void UB_Free(UniformBitmap_p *pUB);

#endif
