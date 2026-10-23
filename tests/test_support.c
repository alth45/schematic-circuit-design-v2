/* ============================================================================
   tests/test_support.c — Implementasi harness + shim platform.

   Isinya:
     1. Definisi counter pass/fail global.
     2. test_reset_state() — wrapper panggil test_reset_model + test_reset_sim.
     3. Shim POSIX untuk _wfopen & _vsnwprintf (hanya aktif di non-Windows,
        lihat tests/platform_compat.h).

   File ini HANYA dikompilasi di build test (lihat Makefile SRC_TEST).
   Tidak pernah masuk build produksi.
   ============================================================================ */

#include "test_support.h"

/* Header core (test_reset_* dideklarasikan di sini saat SCHEMATIC_TEST_STUBS). */
#include "model.h"
#include "sim.h"

/* ===================== Counter global ===================== */
int test_pass_count = 0;
int test_fail_count = 0;
int test_current_section_failed = 0;

/* ===================== Reset state ===================== */
void test_reset_state(void) {
    fprintf(stderr, "[DBG] test_reset_state CALLED (before=%d)\n", comp_count);
#ifdef SCHEMATIC_TEST_STUBS
    test_reset_model();
    test_reset_sim();
    fprintf(stderr, "[DBG] test_reset_state DONE (after=%d)\n", comp_count);
#else
    fprintf(stderr, "[DBG] SCHEMATIC_TEST_STUBS NOT DEFINED — reset skipped\n");
#endif
}

/* ============================================================================
   Shim POSIX untuk _wfopen dan _vsnwprintf

   Di Windows native (MinGW/MSVC) kedua fungsi ini tersedia di CRT.
   Di POSIX, kita sediakan implementasi minimal yang memadai untuk test:

     * test_wfopen  : path UTF-16 → UTF-8 via wcstombs (cukup untuk
                      test dengan path ASCII / temp files).
     * test_vsnwprintf : wrapper vswprintf dengan jaminan null-termination
                      (perilaku glibc bisa -1 pada truncation).
   ============================================================================ */
#ifndef _WIN32

#include <stdlib.h>
#include <string.h>

FILE *test_wfopen(const wchar_t *path, const wchar_t *mode) {
    if (!path || !mode) return NULL;
    char narrow_path[1024];
    char narrow_mode[16];
    if (wcstombs(narrow_path, path, sizeof narrow_path) == (size_t)-1)
        return NULL;
    if (wcstombs(narrow_mode, mode, sizeof narrow_mode) == (size_t)-1)
        return NULL;
    return fopen(narrow_path, narrow_mode);
}

int test_vsnwprintf(wchar_t *buf, size_t count,
                    const wchar_t *fmt, va_list ap) {
    if (!buf || count == 0) return -1;
    int r = vswprintf(buf, count, fmt, ap);
    /* vswprintf glibc: return -1 pada truncation dan tidak null-terminate.
       Paksa selalu null-terminated (aman terhadap pemanggil yang mengandalkan
       buf[count-1] = 0 seperti pada set_status()). */
    buf[count - 1] = L'\0';
    return r;
}

#endif /* !_WIN32 */