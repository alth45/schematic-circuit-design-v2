/* ============================================================================
   src/io/png_writer.c — Encoder PNG mandiri (tanpa libpng/zlib/GDI+/WIC).

   Modul ini PURE: nol <windows.h>. Semua dependensi platform lewat CRT
   standar (stdio, stdlib, string) — bisa dikompilasi di MinGW / MSVC /
   glibc. Satu-satunya fungsi non-ANSI yang dipakai adalah _wfopen()
   (ekstensi Microsoft, tersedia di MSVCRT/ucrt via <stdio.h>) untuk
   mendukung path UTF-16 di Windows.

   Untuk build Linux (unit test), _wfopen dipetakan ke fopen biasa via
   shim di tests/test_support.c — TIDAK di file ini. Strategi ini menjaga
   modul tetap "pure" dari sudut pandang Windows API.

   -----------------------------------------------------------------------
   Strategi kompresi: "deflate stored blocks" (kompresi nol). File jadi
   lebih besar (~3 MB untuk 1100×650) tapi 100% valid dan portabel.

   Format PNG:
     [8-byte signature]
     [IHDR] → width, height, depth=8, color=2, comp=0, filter=0, interlace=0
     [IDAT] → zlib stream (2-byte header + stored deflate blocks + Adler32)
     [IEND] → penutup (CRC tetap 0xAE426082)

   Blok "stored" deflate:
     1 byte  : BFINAL (1) + BTYPE=00 + 5 bit padding
     2 bytes : LEN  (little-endian, ≤ 65535)
     2 bytes : NLEN (one's complement LEN)
     N bytes : data mentah
   ============================================================================ */
#include <stdio.h>   /* FILE, _wfopen, fwrite, fclose */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memcpy */

#include "io.h"      /* png_write_rgb + transitive: types.h, config.h */

/* ---- CRC32 (polinomial PNG) — tabel diinisialisasi lazy. ---- */
static unsigned long crc_table[256];
static int crc_table_ready = 0;

static void crc_init(void) {
    if (crc_table_ready) return;
    for (unsigned long n = 0; n < 256; n++) {
        unsigned long c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xedb88320UL ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
    crc_table_ready = 1;
}

/* ---- Adler32 (untuk stream zlib) ---- */
static unsigned long adler32_buf(const unsigned char *buf, size_t len) {
    unsigned long a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + buf[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

/* ---- Byte put helpers (network order untuk PNG) ---- */
static void put_be32(unsigned char *p, unsigned long v) {
    p[0] = (v >> 24) & 0xff;
    p[1] = (v >> 16) & 0xff;
    p[2] = (v >>  8) & 0xff;
    p[3] =  v        & 0xff;
}

static void put_le16(unsigned char *p, unsigned v) {
    p[0] =  v        & 0xff;
    p[1] = (v >>  8) & 0xff;
}

static int write_bytes(FILE *f, const void *buf, size_t n) {
    return fwrite(buf, 1, n, f) == n;
}

/* Tulis satu chunk PNG (length + type + data + CRC). */
static int write_chunk(FILE *f, const char *type,
                       const unsigned char *data, size_t len) {
    unsigned char hdr[8];
    put_be32(hdr, (unsigned long)len);
    memcpy(hdr + 4, type, 4);
    if (!write_bytes(f, hdr, 8)) return 0;
    if (len && !write_bytes(f, data, len)) return 0;

    /* CRC atas type + data — hitung langsung tanpa alokasi. */
    crc_init();
    unsigned long c = 0xffffffffUL;
    for (int i = 0; i < 4; i++)
        c = crc_table[(c ^ (unsigned char)type[i]) & 0xff] ^ (c >> 8);
    for (size_t i = 0; i < len; i++)
        c = crc_table[(c ^ data[i]) & 0xff] ^ (c >> 8);
    c ^= 0xffffffffUL;

    unsigned char cbytes[4];
    put_be32(cbytes, c);
    return write_bytes(f, cbytes, 4);
}

int png_write_rgb(const wchar_t *path, int width, int height,
                  const unsigned char *rgb) {
    if (!path || width <= 0 || height <= 0 || !rgb) return 0;

    /* 1) Data mentah: tiap baris = 1 byte filter (0) + W*3 byte RGB. */
    size_t row = (size_t)width * 3;
    size_t raw_len = (row + 1) * (size_t)height;
    unsigned char *raw = (unsigned char *)malloc(raw_len);
    if (!raw) return 0;
    for (int y = 0; y < height; y++) {
        unsigned char *dst = raw + (row + 1) * y;
        dst[0] = 0;
        memcpy(dst + 1, rgb + row * y, row);
    }

    /* 2) zlib stream dengan stored deflate blocks. */
    size_t nblocks = (raw_len + 65534) / 65535;
    if (nblocks == 0) nblocks = 1;
    size_t zlib_len = 2 + nblocks * 5 + raw_len + 4;
    unsigned char *zlib = (unsigned char *)malloc(zlib_len);
    if (!zlib) { free(raw); return 0; }

    size_t zp = 0;
    zlib[zp++] = 0x78;   /* zlib header CMF */
    zlib[zp++] = 0x01;   /* zlib header FLG (no dict, no compression) */

    size_t src_off = 0;
    size_t remaining = raw_len;
    for (size_t b = 0; b < nblocks; b++) {
        size_t blk = remaining > 65535 ? 65535 : remaining;
        int final = (b == nblocks - 1);
        zlib[zp++] = final ? 0x01 : 0x00;              /* BFINAL|BTYPE=00 */
        put_le16(zlib + zp, (unsigned)blk);            zp += 2;
        put_le16(zlib + zp, (unsigned)(~blk) & 0xffff);zp += 2;
        memcpy(zlib + zp, raw + src_off, blk);         zp += blk;
        src_off += blk;
        remaining -= blk;
    }
    put_be32(zlib + zp, adler32_buf(raw, raw_len));
    zp += 4;

    /* 3) Tulis file. */
    FILE *f = _wfopen(path, L"wb");
    if (!f) { free(zlib); free(raw); return 0; }

    static const unsigned char sig[8] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A
    };
    int ok = write_bytes(f, sig, 8);

    if (ok) {
        unsigned char ihdr[13];
        put_be32(ihdr + 0, (unsigned long)width);
        put_be32(ihdr + 4, (unsigned long)height);
        ihdr[8]  = 8;   /* bit depth */
        ihdr[9]  = 2;   /* color type = RGB */
        ihdr[10] = 0;   /* compression = deflate */
        ihdr[11] = 0;   /* filter = adaptive */
        ihdr[12] = 0;   /* interlace = none */
        ok = write_chunk(f, "IHDR", ihdr, 13);
    }

    if (ok) ok = write_chunk(f, "IDAT", zlib, zp);
    if (ok) ok = write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(zlib);
    free(raw);
    return ok;
}