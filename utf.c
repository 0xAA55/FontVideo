#include"utf.h"

// Convert 32-bit unicode into UTF-8 form one by one.
// The UTF-8 output string pointer will be moved to the next position.
void u32toutf8
(
    int8_t **ppUTF8,
    const uint32_t CharCode
)
{
    int8_t *pUTF8 = ppUTF8[0];

    if (CharCode >= 0x4000000)
    {
        *pUTF8++ = 0xFC | ((CharCode >> 30) & 0x01);
        *pUTF8++ = 0x80 | ((CharCode >> 24) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 18) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 12) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 6) & 0x3F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else if (CharCode >= 0x200000)
    {
        *pUTF8++ = 0xF8 | ((CharCode >> 24) & 0x03);
        *pUTF8++ = 0x80 | ((CharCode >> 18) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 12) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 6) & 0x3F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else if (CharCode >= 0x10000)
    {
        *pUTF8++ = 0xF0 | ((CharCode >> 18) & 0x07);
        *pUTF8++ = 0x80 | ((CharCode >> 12) & 0x3F);
        *pUTF8++ = 0x80 | ((CharCode >> 6) & 0x3F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else if (CharCode >= 0x0800)
    {
        *pUTF8++ = 0xE0 | ((CharCode >> 12) & 0x0F);
        *pUTF8++ = 0x80 | ((CharCode >> 6) & 0x3F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else if (CharCode >= 0x0080)
    {
        *pUTF8++ = 0xC0 | ((CharCode >> 6) & 0x1F);
        *pUTF8++ = 0x80 | (CharCode & 0x3F);
    }
    else
    {
        *pUTF8++ = (char)CharCode;
    }

    // Move the pointer
    ppUTF8[0] = pUTF8;
}

// Convert UTF-8 character into 32-bit unicode.
// The UTF-8 string pointer will be moved to the next position.
uint32_t utf8tou32char
(
    int8_t **ppUTF8
)
{
    size_t cb = 0;
    uint32_t ret = 0;
    int8_t *pUTF8 = ppUTF8[0];

    // Detect the available room size of the UTF-8 buffer
    while (cb < 6 && pUTF8[cb]) cb++;

    if ((pUTF8[0] & 0xFE) == 0xFC) // 1111110x
    {
        if (6 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x01) << 30) |
                (((uint32_t)pUTF8[1] & 0x3F) << 24) |
                (((uint32_t)pUTF8[2] & 0x3F) << 18) |
                (((uint32_t)pUTF8[3] & 0x3F) << 12) |
                (((uint32_t)pUTF8[4] & 0x3F) << 6) |
                (((uint32_t)pUTF8[5] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[6];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xFC) == 0xF8) // 111110xx
    {
        if (5 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x03) << 24) |
                (((uint32_t)pUTF8[1] & 0x3F) << 18) |
                (((uint32_t)pUTF8[2] & 0x3F) << 12) |
                (((uint32_t)pUTF8[3] & 0x3F) << 6) |
                (((uint32_t)pUTF8[4] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[5];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xF8) == 0xF0) // 11110xxx
    {
        if (4 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x07) << 18) |
                (((uint32_t)pUTF8[1] & 0x3F) << 12) |
                (((uint32_t)pUTF8[2] & 0x3F) << 6) |
                (((uint32_t)pUTF8[3] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[4];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xF0) == 0xE0) // 1110xxxx
    {
        if (3 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x0F) << 12) |
                (((uint32_t)pUTF8[1] & 0x3F) << 6) |
                (((uint32_t)pUTF8[2] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[3];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xE0) == 0xC0) // 110xxxxx
    {
        if (2 <= cb)
        {
            ret =
                (((uint32_t)pUTF8[0] & 0x1F) << 6) |
                (((uint32_t)pUTF8[1] & 0x3F) << 0);
            ppUTF8[0] = &pUTF8[2];
            return ret;
        }
        else
            goto FailExit;
    }
    else if ((pUTF8[0] & 0xC0) == 0x80) // 10xxxxxx
    {
        // Wrong encode
        goto FailExit;
    }
    else if ((pUTF8[0] & 0x80) == 0x00) // 0xxxxxxx
    {
        ret = pUTF8[0] & 0x7F;
        ppUTF8[0] = &pUTF8[1];
        return ret;
    }
    return ret;
FailExit:
    // If convert failed, null-char will be returned.
    return ret;
}

// Convert 32-bit unicode into UTF-16 form one by one.
// The UTF-16 output string pointer will be moved to the next position.
void u32toutf16
(
    int16_t **ppUTF16,
    const uint32_t CharCode
)
{
    int16_t *pUTF16 = ppUTF16[0];
    if (CharCode <= 0xffff)
    {
        *pUTF16++ = (uint16_t)CharCode;
    }
    else if (CharCode <= 0x10ffff)
    {
        uint32_t U20 = CharCode - 0x10000;
        uint16_t HighSurrogate = 0xD800 | (uint16_t)(U20 >> 10);
        uint16_t LowSurrogate = 0xDC00 | (uint16_t)(U20 & 0x3FF);
        *pUTF16++ = HighSurrogate;
        *pUTF16++ = LowSurrogate;
    }
    else
    {
        *pUTF16++ = 0xFFFD;
    }
    ppUTF16[0] = pUTF16;
}

// Convert UTF-16 character into 32-bit unicode.
// The UTF-16 string pointer will be moved to the next position.
uint32_t u16tou32char
(
    int16_t **ppUTF16
)
{
    int16_t *pUTF16 = ppUTF16[0];
    uint32_t CharCode = 0xFFFD;

    switch (pUTF16[0] & 0xFC00)
    {
    case 0xD800:
        if ((pUTF16[1] & 0xFC00) == 0xDC00)
        {
            CharCode = (pUTF16[1] & 0x3FF) | ((uint32_t)(pUTF16[0] & 0x3FF) << 10);
            CharCode += 0x10000;
            pUTF16 += 2;
        }
        break;
    case 0xDC00:
        if ((pUTF16[1] & 0xFC00) == 0xD800)
        {
            CharCode = (pUTF16[0] & 0x3FF) | ((uint32_t)(pUTF16[1] & 0x3FF) << 10);
            CharCode += 0x10000;
            pUTF16 += 2;
        }
        break;
    default:
        CharCode = *pUTF16++;
    }

    ppUTF16[0] = pUTF16;
    return CharCode;
}
