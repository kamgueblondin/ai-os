#ifndef AIOS_SOCKET_BOUNDS_H
#define AIOS_SOCKET_BOUNDS_H

#include <stdint.h>
#include <stdbool.h>

#define AIOS_MAX_SOCKET_DESCRIPTORS 1024
#define AIOS_DEFAULT_SOCKET_TIMEOUT_MS 5000

static inline bool aios_is_valid_socket_fd(int32_t fd) {
    return (fd >= 0 && fd < AIOS_MAX_SOCKET_DESCRIPTORS);
}

#endif /* AIOS_SOCKET_BOUNDS_H */
