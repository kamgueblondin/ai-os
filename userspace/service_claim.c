#include "os_syscalls.h"

static void putc(char c) {
    asm volatile("int $0x80" : : "a"(SYS_PUTC), "b"(c));
}

static void puts(const char* text) {
    int i = 0;
    while (text[i] != '\0') putc(text[i++]);
}

static void print_int(int value) {
    char digits[12];
    int n = 0;
    unsigned int number;
    if (value < 0) {
        putc('-');
        number = (unsigned int)(-value);
    } else {
        number = (unsigned int)value;
    }
    if (number == 0U) {
        putc('0');
        return;
    }
    while (number > 0U && n < 11) {
        digits[n++] = (char)('0' + (number % 10U));
        number /= 10U;
    }
    while (n > 0) putc(digits[--n]);
}

static int service_register(const char* name) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_REGISTER), "b"(name));
    return result;
}

static void yield(void) {
    asm volatile("int $0x80" : : "a"(SYS_YIELD));
}

void main(void) {
    int announced = 0;
    for (;;) {
        int rc = service_register("demo");
        if (rc == 0) {
            puts("serviceclaim claimed demo\n");
            for (;;) yield();
        }
        if (!announced) {
            if (rc == OS_SERVICE_TAKEN) {
                puts("serviceclaim waiting demo\n");
            } else {
                puts("serviceclaim register rc ");
                print_int(rc);
                puts("\n");
            }
            announced = 1;
        }
        yield();
    }
}
