#ifndef _SATISALLOC_H_
#define _SATISALLOC_H_ 1

#include <stdlib.h>

void* bunchalloc(size_t alignment, size_t headersize, ...);

#endif