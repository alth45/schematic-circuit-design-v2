#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

/* ============================================================================
   tests/platform_compat.h — Shim fungsi MSVCRT yang dipakai modul pure.

   Modul pure core/io memakai dua fungsi MSVCRT (Microsoft-specific) yang
   TIDAK tersedia di glibc/musl:

       FILE *_wfopen(const wchar_t *path, const wchar_t *mode);
       int   _vsnwprintf(wchar_t *buf, size_t count, const wchar_t *fmt,
                         va_list ap);

   Di Windows (MinGW/MSVC), keduanya tersedia native di <stdio.h> dan
   <wchar.h>.

   Di Linux, header ini me-redirect keduanya ke implementasi POSIX shim
   yang disediakan oleh tests/test_support.c. Redirection dilakukan via
   #define preprocessor — jadi file sumber yang memanggil _wfopen() atau
   _vsnwprintf() otomatis memanggil test_wfopen() / test_vsnwprintf()
   tanpa perlu diubah sama sekali.

   CARA PAKAI
   ----------
   File ini TIDAK di-include secara eksplisit dari source files.
   Sebaliknya, di-build system tambahkan flag:

       gcc ... -include tests/platform_compat.h ...

   sehingga otomatis di-pre-include ke setiap TU (translation unit)
   sebelum file-nya sendiri. Flag ini aman di Windows juga — di sana
   header ini hanya berisi deklarasi kosong (native MSVCRT dipakai).

   KAPAN DIPAKAI
   -------------
   * Build produksi Windows : TIDAK perlu (native Win32 dipakai).
   * Build test Windows     : OPSIONAL (native Win32 tetap jalan,
                              shim tidak dipicu).
   * Build test Linux       : WAJIB (shim dipakai untuk _wfopen & _vsnwprintf).
   ============================================================================ */

#ifdef _WIN32
/* ---------------------------------------------------------------- Windows */
/* Native MSVCRT/ucrt: _wfopen di <stdio.h>, _vsnwprintf di <wchar.h>.
   Tidak ada redirect. Header ini intentionally kosong di Windows. */

#else
/* ---------------------------------------------------------------- POSIX  */
#include <stdio.h>    /* FILE, fopen */
#include <stdarg.h>   /* va_list */
#include <wchar.h>    /* wchar_t, vswprintf */
#include <stddef.h>   /* size_t */

/* Implementasi di tests/test_support.c. */
FILE *test_wfopen(const wchar_t *path, const wchar_t *mode);
int   test_vsnwprintf(wchar_t *buf, size_t count,
                      const wchar_t *fmt, va_list ap);

/* Redirect nama simbol MSVCRT ke shim. Semua file sumber yang
   di-compile dengan header ini otomatis melihat shim. */
#define _wfopen     test_wfopen
#define _vsnwprintf test_vsnwprintf

#endif /* _WIN32 */

#endif /* PLATFORM_COMPAT_H */