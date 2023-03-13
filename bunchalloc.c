#include"bunchalloc.h"
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

static size_t make_padded(size_t unpadded, size_t alignment)
{
	return ((unpadded - 1) / alignment + 1) * alignment;
}

void* bunchalloc(size_t alignment, size_t headersize, ...)
{
	size_t i, count = 0;
	size_t member_offset, desired_size;;
	size_t padded_size = 0;
	size_t cur_offset = 0, esti_size, cur_size;
	void* ret = NULL;
	va_list ap;
	if (!alignment) alignment = sizeof(size_t) * 2;
	esti_size = make_padded(headersize, alignment) + alignment /* allocate extra memory for aligning body data to an aligned address */;

	va_start(ap, headersize);

	for(;;)
	{
		member_offset = va_arg(ap, size_t);
		desired_size = va_arg(ap, size_t);
		if (!desired_size) break;
		count += 1;

		padded_size = make_padded(desired_size, alignment);
		cur_offset = esti_size;
		esti_size += padded_size;
	};

	va_end(ap);

	ret = malloc(esti_size);
	if (!ret) return NULL;
	memset(ret, 0, esti_size);

	va_start(ap, headersize);

	// Align body data to an aligned address
	cur_size = make_padded((size_t)ret + headersize, alignment) - (size_t)ret;
	for (i = 0; i < count; i++)
	{
		member_offset = va_arg(ap, size_t);
		desired_size = va_arg(ap, size_t);
		padded_size = make_padded(desired_size, alignment);
		cur_offset = cur_size;
		cur_size += padded_size;

		*(void**)((size_t)ret + member_offset) = (void*)((size_t)ret + cur_offset);
	}

	va_end(ap);

	return ret;
}

void bunchfree(void* ptr)
{
	free(ptr);
}




