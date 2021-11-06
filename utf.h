#ifndef _UTF_H_
#define _UTF_H_ 1

#include<stdint.h>
#include<stddef.h>

// Convert 32-bit unicode into UTF-8 form one by one.
// The UTF-8 output string pointer will be moved to the next position.
void u32toutf8
(
    char **ppUTF8,
    const uint32_t CharCode
);

// Convert UTF-8 character into 32-bit unicode.
// The UTF-8 string pointer will be moved to the next position.
uint32_t utf8tou32char
(
    char **ppUTF8
);

// Convert 32-bit unicode into UTF-16 form one by one.
// The UTF-16 output string pointer will be moved to the next position.
void u32toutf16
(
    uint16_t **ppUTF16,
    const uint32_t CharCode
);

// Convert UTF-16 character into 32-bit unicode.
// The UTF-16 string pointer will be moved to the next position.
uint32_t u16tou32char
(
    uint16_t **ppUTF16
);

#endif // !_UTF_H_
