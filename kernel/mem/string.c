#include "string.h"

void* memset(void* bufptr, int value, size_t size) {
    unsigned char* buf = (unsigned char*) bufptr;
    for (size_t i = 0; i < size; i++) {
        buf[i] = (unsigned char) value;
    }
    return bufptr;
}

void* memcpy(void* dstptr, const void* srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	for (size_t i = 0; i < size; i++)
		dst[i] = src[i];
	return dstptr;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// NOTE: This is a placeholder implementation. A real implementation must validate
// that the user pointer 'dst' points to a valid, writable user-space address
// before copying to prevent kernel panics or security vulnerabilities.
int copy_to_user(void __user *dst, const void *src, size_t size) {
    memcpy(dst, src, size);
    return 0; // Assume success for now. On failure, should return an error code.
}
