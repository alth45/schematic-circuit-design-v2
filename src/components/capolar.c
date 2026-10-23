/* src/components/capolar.c — Simbol kapasitor polar / elektrolit (2-pin).

   Simbol standar IEC 60617 untuk kapasitor terpolarisasi:
     - Plat (+) : garis vertikal LURUS, di kiri.
     - Plat (−) : busur setengah-elips CEMBUNG KE KANAN.
     - Lead kanan keluar dari PUNCAK busur.

   pin0 = (+) kiri, pin1 = (−) kanan. Nilai v = kapasitansi (F).
   DC: blokir. AC: Xc = 1/(2πfC).

   Polyline (bukan Arc) karena Arc() GDI memproyeksikan start/end yang
   tidak persis di elips, sehingga bentuk menjadi tidak terduga. */
#include "ui.h"

static void draw_capolar(HDC hdc, int x, int y, BOOL sel) {
    COLORREF col = sel ? RGB(50,128,255) : RGB(20,20,20);
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* Lead kiri -> plat (+) */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 6, y);

    /* Plat (+) : garis lurus vertikal, tinggi 30 px */
    MoveToEx(hdc, x - 6, y - 15, NULL); LineTo(hdc, x - 6, y + 15);

    /* Plat (−) : busur setengah-elips, dari (x+6, y-15) [atas]
       melewati (x+15, y) [puncak cembung] ke (x+6, y+15) [bawah]. */
    {
        const int cx = x + 6,  cy = y;
        const int rx = 9,      ry = 15;
        int started = 0;
        for (int deg = -90; deg <= 90; deg += 5) {
            double rad = deg * 3.14159265358979 / 180.0;
            int px = cx + (int)lround(rx * cos(rad));
            int py = cy + (int)lround(ry * sin(rad));
            if (!started) { MoveToEx(hdc, px, py, NULL); started = 1; }
            else          LineTo(hdc, px, py);
        }
    }

    /* Lead kanan : dari PUNCAK busur (x+15, y) ke pin (x+40, y). */
    MoveToEx(hdc, x + 15, y, NULL); LineTo(hdc, x + 40, y);

    SelectObject(hdc, oldb);
    SelectObject(hdc, old);
    DeleteObject(pen);

    /* Tanda polaritas: '+' di dekat plat lurus, '−' di dekat plat lengkung */
    SetTextColor(hdc, sel ? RGB(50,128,255) : RGB(20,20,20));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x - 22, y - 14, L"+", 1);
    TextOutW(hdc, x + 19, y - 14, L"-", 1);

    /* Label tipe */
    SetTextColor(hdc, RGB(80,80,80));
    TextOutW(hdc, x - 8, y - 30, L"CP", 2);
}

void draw_k_capolar(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_capolar);
}