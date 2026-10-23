#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

/* ============================================================================
   tests/test_support.h — Test harness minimal (C-only, tanpa framework).

   Fitur:
     * Counter pass/fail global (didefinisikan di test_support.c).
     * Macro assertion soft (test lanjut setelah fail, untuk laporan lengkap).
     * Macro TEST_RUN untuk membungkus satu test-case dengan output jelas.
     * Helper test_reset_state() = reset model + sim (dipanggil di awal
       setiap test-case agar tidak ada state bocor antar test).

   Desain:
     * Tidak butuh dependency luar (no gtest, no assert.h).
     * Assertion dicetak ke stdout dengan file:line supaya mudah debug.
     * Exit code akhir = (test_fail_count > 0) ? 1 : 0 (di test_main.c).

   Contoh penggunaan:
       #include "test_support.h"
       #include "model.h"

       static void test_snap(void) {
           test_reset_state();
           TEST_ASSERT_INT_EQ(snap(0),  0);
           TEST_ASSERT_INT_EQ(snap(9),  0);
           TEST_ASSERT_INT_EQ(snap(11), 20);
       }

       int main(void) {
           TEST_RUN(test_snap);
           return (test_fail_count > 0) ? 1 : 0;
       }
   ============================================================================ */

#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* ===================== Counter global ===================== */
extern int test_pass_count;             /* total assertion sukses         */
extern int test_fail_count;             /* total assertion gagal          */
extern int test_current_section_failed; /* 1 kalau test-case ini ada fail */

/* ===================== Assertion macros (soft) ===================== */

/* Kondisi boolean. */
#define TEST_ASSERT(cond) do { \
    if (cond) { \
        test_pass_count++; \
    } else { \
        test_fail_count++; \
        test_current_section_failed = 1; \
        printf("         FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

/* Kondisi boolean + pesan kustom. */
#define TEST_ASSERT_MSG(cond, msg) do { \
    if (cond) { \
        test_pass_count++; \
    } else { \
        test_fail_count++; \
        test_current_section_failed = 1; \
        printf("         FAIL: %s:%d: %s — %s\n", \
               __FILE__, __LINE__, #cond, (msg)); \
    } \
} while (0)

/* Integer equality (long cast — cocok untuk int, enum, pointer delta kecil). */
#define TEST_ASSERT_INT_EQ(a, b) do { \
    long __a = (long)(a), __b = (long)(b); \
    if (__a == __b) { \
        test_pass_count++; \
    } else { \
        test_fail_count++; \
        test_current_section_failed = 1; \
        printf("         FAIL: %s:%d: %s == %s " \
               "(expected %ld, got %ld)\n", \
               __FILE__, __LINE__, #a, #b, __b, __a); \
    } \
} while (0)

/* Integer inequality. */
#define TEST_ASSERT_INT_NE(a, b) do { \
    long __a = (long)(a), __b = (long)(b); \
    if (__a != __b) { \
        test_pass_count++; \
    } else { \
        test_fail_count++; \
        test_current_section_failed = 1; \
        printf("         FAIL: %s:%d: %s != %s (both %ld)\n", \
               __FILE__, __LINE__, #a, #b, __a); \
    } \
} while (0)

/* Double equality dengan toleransi absolut (eps). */
#define TEST_ASSERT_DOUBLE_EQ(a, b, eps) do { \
    double __a = (double)(a), __b = (double)(b); \
    double __diff = __a - __b; if (__diff < 0) __diff = -__diff; \
    if (__diff <= (double)(eps)) { \
        test_pass_count++; \
    } else { \
        test_fail_count++; \
        test_current_section_failed = 1; \
        printf("         FAIL: %s:%d: |%s - %s| <= %g " \
               "(%.9g vs %.9g, diff %.9g)\n", \
               __FILE__, __LINE__, #a, #b, (double)(eps), __a, __b, __diff); \
    } \
} while (0)

/* String wide equality (null-safe). */
#define TEST_ASSERT_WSTREQ(a, b) do { \
    const wchar_t *__a = (a), *__b = (b); \
    int __ok = (__a && __b) ? (wcscmp(__a, __b) == 0) : (__a == __b); \
    if (__ok) { \
        test_pass_count++; \
    } else { \
        test_fail_count++; \
        test_current_section_failed = 1; \
        printf("         FAIL: %s:%d: wcscmp(%s, %s) == 0\n", \
               __FILE__, __LINE__, #a, #b); \
    } \
} while (0)

/* String ASCII equality. */
#define TEST_ASSERT_STREQ(a, b) do { \
    const char *__a = (a), *__b = (b); \
    int __ok = (__a && __b) ? (strcmp(__a, __b) == 0) : (__a == __b); \
    if (__ok) { \
        test_pass_count++; \
    } else { \
        test_fail_count++; \
        test_current_section_failed = 1; \
        printf("         FAIL: %s:%d: strcmp(%s, %s) == 0\n", \
               __FILE__, __LINE__, #a, #b); \
    } \
} while (0)

/* Null pointer check. */
#define TEST_ASSERT_NULL(p)  TEST_ASSERT_MSG((p) == NULL,  "expected NULL")
#define TEST_ASSERT_NOTNULL(p) TEST_ASSERT_MSG((p) != NULL, "expected non-NULL")

/* ===================== Test runner ===================== */

/* Jalankan satu test-case, cetak ringkasan per-case. */
#define TEST_RUN(fn) do { \
    int __before_pass = test_pass_count; \
    int __before_fail = test_fail_count; \
    test_current_section_failed = 0; \
    printf("[ RUN      ] %s\n", #fn); \
    fn(); \
    if (test_fail_count > __before_fail) { \
        printf("[  FAILED  ] %s  (%d assertion(s), %d failed)\n", \
               #fn, \
               (test_pass_count - __before_pass) + (test_fail_count - __before_fail), \
               test_fail_count - __before_fail); \
    } else { \
        printf("[       OK ] %s  (%d assertion(s))\n", \
               #fn, test_pass_count - __before_pass); \
    } \
} while (0)

/* ===================== State reset helper ===================== */
/* Reset model + sim ke kondisi kosong. WAJIB dipanggil di awal setiap
   test-case yang memanipulasi state global (comps, wires, junction,
   selection, undo stack, dll) untuk mencegah bocoran antar test. */
void test_reset_state(void);

#endif /* TEST_SUPPORT_H */