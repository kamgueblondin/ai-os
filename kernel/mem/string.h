#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// Define __user for address space annotation (compiler hint)
#ifndef __user
#define __user
#endif

void* memset(void* bufptr, int value, size_t size);
void* memcpy(void* dstptr, const void* srcptr, size_t size);
int strcmp(const char *s1, const char *s2);

// Copies data from kernel space to user space.
// Returns 0 on success.
// NOTE: A real implementation needs to perform security checks.
int copy_to_user(void __user *dst, const void *src, size_t size);

#endif
