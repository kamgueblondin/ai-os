#ifndef AIOS_LFN_UTF8_H
#define AIOS_LFN_UTF8_H

#include <stdint.h>

/* Conversion LFN bornée : BMP uniquement, sans surrogates ni allocation. */
static int lfn_utf8_to_utf16_bmp(const char* input, uint16_t* units,
                                 uint32_t capacity, uint32_t* unit_count) {
    uint32_t in = 0U, out = 0U;
    uint32_t max_bytes;
    if (!input || !units || !unit_count || capacity == 0U || capacity > 255U) return -1;
    max_bytes = capacity * 3U;
    while (in < max_bytes && input[in] != '\0') {
        uint8_t a = (uint8_t)input[in++];
        uint32_t codepoint;
        if (a < 0x80U) {
            codepoint = a;
        } else if (a >= 0xC2U && a <= 0xDFU) {
            uint8_t b;
            if (in >= max_bytes || input[in] == '\0') return -1;
            b = (uint8_t)input[in++];
            if ((b & 0xC0U) != 0x80U) return -1;
            codepoint = ((uint32_t)(a & 0x1FU) << 6U) | (uint32_t)(b & 0x3FU);
        } else if (a >= 0xE0U && a <= 0xEFU) {
            uint8_t b, c;
            if (in + 1U >= max_bytes || input[in] == '\0' || input[in + 1U] == '\0') return -1;
            b = (uint8_t)input[in++]; c = (uint8_t)input[in++];
            if ((b & 0xC0U) != 0x80U || (c & 0xC0U) != 0x80U ||
                (a == 0xE0U && b < 0xA0U) || (a == 0xEDU && b >= 0xA0U)) return -1;
            codepoint = ((uint32_t)(a & 0x0FU) << 12U) |
                        ((uint32_t)(b & 0x3FU) << 6U) | (uint32_t)(c & 0x3FU);
        } else {
            return -1;
        }
        if (codepoint < 0x20U || codepoint == '/' || codepoint == '\\' ||
            codepoint == 0x7FU || out >= capacity) return -1;
        units[out++] = (uint16_t)codepoint;
    }
    if (in == max_bytes && input[in] != '\0') return -1;
    if (out == 0U) return -1;
    *unit_count = out;
    return 0;
}

static int lfn_utf16_bmp_to_utf8(const uint16_t* units, uint32_t unit_count,
                                  char* output, uint32_t capacity) {
    uint32_t in, out = 0U;
    if (!units || !output || capacity == 0U) return -1;
    for (in = 0U; in < unit_count && units[in] != 0U && units[in] != 0xFFFFU; in++) {
        uint16_t unit = units[in];
        if (unit >= 0xD800U && unit <= 0xDFFFU) return -1;
        if (unit < 0x80U) {
            if (out + 1U >= capacity) return -1;
            output[out++] = (char)unit;
        } else if (unit < 0x800U) {
            if (out + 2U >= capacity) return -1;
            output[out++] = (char)(0xC0U | (unit >> 6U));
            output[out++] = (char)(0x80U | (unit & 0x3FU));
        } else {
            if (out + 3U >= capacity) return -1;
            output[out++] = (char)(0xE0U | (unit >> 12U));
            output[out++] = (char)(0x80U | ((unit >> 6U) & 0x3FU));
            output[out++] = (char)(0x80U | (unit & 0x3FU));
        }
    }
    output[out] = '\0';
    return (int)out;
}

#endif
