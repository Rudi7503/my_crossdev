/*
 * v4_test.h -- Minimales Testgeruest. Keine Abhaengigkeiten ausser stdio,
 * damit dieselben Tests nativ (Little Endian) und unter qemu-m68k
 * (Big Endian) laufen koennen -- siehe Makefile-Ziel `test-m68k`.
 */

#ifndef V4_TEST_H
#define V4_TEST_H

#include <stdio.h>
#include <string.h>

extern int         v4_test_checks;
extern int         v4_test_failures;
extern int         v4_test_cases;
extern const char *v4_test_case_name;

void v4_test_begin(const char *suite);
void v4_test_case(const char *name);

#define CHECK(cond)                                                          \
    do {                                                                     \
        v4_test_checks++;                                                    \
        if (!(cond)) {                                                       \
            v4_test_failures++;                                              \
            printf("  FAIL %s:%d [%s]: %s\n", __FILE__, __LINE__,            \
                   v4_test_case_name, #cond);                                \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        long long v4_a = (long long)(a);                                     \
        long long v4_b = (long long)(b);                                     \
        v4_test_checks++;                                                    \
        if (v4_a != v4_b) {                                                  \
            v4_test_failures++;                                              \
            printf("  FAIL %s:%d [%s]: %s = %lld, erwartet %s = %lld\n",     \
                   __FILE__, __LINE__, v4_test_case_name, #a, v4_a, #b,      \
                   v4_b);                                                    \
        }                                                                    \
    } while (0)

#define CHECK_MEM(a, b, n)                                                   \
    do {                                                                     \
        v4_test_checks++;                                                    \
        if (memcmp((a), (b), (n)) != 0) {                                    \
            v4_test_failures++;                                              \
            printf("  FAIL %s:%d [%s]: %s != %s (%u Byte)\n", __FILE__,      \
                   __LINE__, v4_test_case_name, #a, #b, (unsigned)(n));      \
        }                                                                    \
    } while (0)

#define CHECK_STR(a, b)                                                      \
    do {                                                                     \
        v4_test_checks++;                                                    \
        if (strcmp((a), (b)) != 0) {                                         \
            v4_test_failures++;                                              \
            printf("  FAIL %s:%d [%s]: \"%s\" != \"%s\"\n", __FILE__,        \
                   __LINE__, v4_test_case_name, (a), (b));                   \
        }                                                                    \
    } while (0)

/* Von den Testdateien geliefert: jeweils Anzahl Fehler. */
int test_proto(void);
int test_master(void);
int test_session(void);
int test_fuzz(void);

#endif /* V4_TEST_H */
