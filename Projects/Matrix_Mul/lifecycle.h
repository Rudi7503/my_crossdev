#ifndef ADPCM_TEST_LIFECYCLE_H
#define ADPCM_TEST_LIFECYCLE_H
#include "uart_asm.h"
#ifndef TEST_VERSION
#define TEST_VERSION "dev"
#endif
#define TEST_START(prog) do { \
    uart_init(); \
    uart_puts("START;" prog ";" TEST_VERSION "\n"); \
    uart_flush(); \
} while (0)
#define TEST_DONE(prog) do { \
    uart_puts("DONE;" prog ";" TEST_VERSION "\n"); \
    uart_flush(); \
} while (0)
static inline void test_alive(const char *prog, unsigned n) {
    char buf[96];
    snprintf(buf, sizeof(buf), "ALIVE;%s;%u\n", prog, n);
    printf("%s", buf);
    fflush(stdout);
    uart_puts(buf);
    uart_flush();
}
#endif
