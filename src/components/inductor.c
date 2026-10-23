/* src/components/inductor.c — Simbol induktor (2-pin, DC short / AC XL).

   Simbol standar IEC: 4 lengkung (hump) di atas garis kabel.
   pin0 = kiri, pin1 = kanan. Nilai v = induktansi (Henry). Default 100 µH.
   Saat mode RUN dengan sumber AC, induktor memberi reaktansi XL = 2πfL. */
#include "ui.h"

static void draw_inductor(HDC hdc, int x, int y, BOOL sel) {
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* Lead kiri & kanan */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 20, y);
    MoveToEx(hdc, x + 20, y, NULL); LineTo(hdc, x + 40, y);

    /* 4 lengkung (hump) dari x-20 s.d. x+20, amplitude 7 px ke atas.
       abs(sin(phase)) dengan phase 0..4π → 4 hump sejenis. */
    {
        int started = 0;
        for (int px = -20; px <= 20; px++) {
            double t = (double)(px + 20) / 40.0;   /* 0..1 */
            double phase = t * 4.0 * 3.14159265358979;
            int py = (int)(-7.0 * fabs(sin(phase)));
            if (!started) { MoveToEx(hdc, x + px, y + py, NULL); started = 1; }
            else          LineTo(hdc, x + px, y + py);
        }
    }

    SelectObject(hdc, oldb);
    SelectObject(hdc, old);
    DeleteObject(pen);

    SetTextColor(hdc, RGB(80, 80, 80));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x - 4, y - 26, L"L", 1);
}

void draw_k_inductor(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_inductor);
}