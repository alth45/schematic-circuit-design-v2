/* ============================================================================
   src/io/export_png.c — Render canvas ke bitmap, lalu tulis sebagai PNG.

   Fungsi ini adalah JEMBATAN antara dua layer:
     * UI  — render_canvas_scene() (menggambar seluruh scene ke HDC)
     * IO  — png_write_rgb()      (encoder PNG murni)

   Kenapa tidak PURE?
     Membutuhkan GetDC/CreateCompatibleDC/GetDIBits (Win32 GDI) untuk
     menangkap hasil render. Karena itu file ini TIDAK masuk build test
     Linux — hanya build produksi Windows. Untuk unit test PNG encoding,
     gunakan png_writer.c langsung (yang pure dan tidak butuh GDI).

   Include:
     * "ui.h"     — HWND, render_canvas_scene, TOOLBAR_H/STATUS_H
                    (via model.h → config.h), g_show_status.
     * "io.h"     — png_write_rgb.
     * <stdlib.h> — malloc, free.
     * <string.h> — memset (via ZeroMemory macro), memcpy.
     * <wchar.h>  — swprintf untuk pesan error.
   ============================================================================ */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "ui.h"
#include "io.h"

int export_canvas_png(HWND hwnd, const wchar_t *path,
                      wchar_t *err, int err_cap) {
    if (err && err_cap > 0) err[0] = L'\0';

    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right;
    int H = rc.bottom - TOOLBAR_H - (g_show_status ? STATUS_H : 0);
    if (W <= 0 || H <= 0) {
        if (err) swprintf(err, err_cap, L"Area canvas tidak valid.");
        return 0;
    }

    HDC screen = GetDC(hwnd);
    if (!screen) {
        if (err) swprintf(err, err_cap, L"Gagal mendapatkan DC.");
        return 0;
    }

    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
    if (!mem || !bmp) {
        if (bmp) DeleteObject(bmp);
        if (mem) DeleteDC(mem);
        ReleaseDC(hwnd, screen);
        if (err) swprintf(err, err_cap, L"Gagal membuat bitmap sementara.");
        return 0;
    }

    HBITMAP oldb = (HBITMAP)SelectObject(mem, bmp);

    /* Latar putih (sama dengan yang dipakai WM_PAINT). */
    RECT fill = { 0, 0, W, H };
    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(mem, &fill, bg);
    DeleteObject(bg);

    /* Render tanpa UI transient (seleksi & marquee). */
    render_canvas_scene(mem, W, H, 0);

    /* PENTING: bitmap harus dilepas dari DC sebelum GetDIBits. */
    SelectObject(mem, oldb);

    /* Ambil piksel sebagai BGRA top-down. */
    int row_bytes = W * 4;
    unsigned char *rowbuf = (unsigned char *)malloc((size_t)row_bytes * H);
    if (!rowbuf) {
        DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(hwnd, screen);
        if (err) swprintf(err, err_cap, L"Memori tidak cukup.");
        return 0;
    }

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = W;
    bi.bmiHeader.biHeight      = -H;   /* top-down */
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    int got = GetDIBits(mem, bmp, 0, H, rowbuf, &bi, DIB_RGB_COLORS);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, screen);

    if (!got) {
        free(rowbuf);
        if (err) swprintf(err, err_cap, L"GetDIBits gagal.");
        return 0;
    }

    /* Konversi BGRA -> RGB. */
    unsigned char *rgb = (unsigned char *)malloc((size_t)W * H * 3);
    if (!rgb) {
        free(rowbuf);
        if (err) swprintf(err, err_cap, L"Memori tidak cukup.");
        return 0;
    }
    for (int i = 0; i < W * H; i++) {
        rgb[i * 3 + 0] = rowbuf[i * 4 + 2];   /* R */
        rgb[i * 3 + 1] = rowbuf[i * 4 + 1];   /* G */
        rgb[i * 3 + 2] = rowbuf[i * 4 + 0];   /* B */
    }
    free(rowbuf);

    int ok = png_write_rgb(path, W, H, rgb);
    free(rgb);

    if (!ok && err) swprintf(err, err_cap, L"Gagal menulis file PNG.");
    return ok;
}