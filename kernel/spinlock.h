#pragma once
#include <stdint.h>

typedef volatile int spinlock_t;

static inline void spin_lock(spinlock_t *lock) {
    while (__atomic_test_and_set(lock, __ATOMIC_ACQUIRE));
}

static inline void spin_unlock(spinlock_t *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}
