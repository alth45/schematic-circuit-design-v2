/* src/components/acsource.c — Simbol sumber AC (2-pin, VAC RMS).

   Lingkaran + gelombang sinus di dalam.
   pin0 = Live (kiri), pin1 = Netral (kanan). Nilai v = VAC RMS (default 220).
   Saat mode RUN, lingkaran digambar biru sebagai penanda sumber aktif. */
#include "ui.h"

static void draw_acsource(HDC hdc, int x, int y, BOOL sel) {
    HPEN pen = CreatePen(PS_SOLID, 2, sel ? RGB(50,128,255) : RGB(20,20,20));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* kaki horizontal */
    MoveToEx(hdc, x-40, y, NULL); LineTo(hdc, x-22, y);
    MoveToEx(hdc, x+22, y, NULL); LineTo(hdc, x+40, y);

    /* lingkaran sumber */
    Ellipse(hdc, x-22, y-22, x+22, y+22);

    /* gelombang sinus di dalam lingkaran */
    {
        int started = 0;
        for (int px = -14; px <= 14; px++) {
            double ph = (double)px / 14.0 * 3.14159265358979; /* -pi..pi */
            int py = (int)(-10.0 * sin(ph));
            if (!started) { MoveToEx(hdc, x+px, y+py, NULL); started = 1; }
            else LineTo(hdc, x+px, y+py);
        }
    }

    SelectObject(hdc, ob);
    SelectObject(hdc, old);
    DeleteObject(pen);

    SetTextColor(hdc, RGB(80,80,80));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x-10, y-38, L"~", 1);
    TextOutW(hdc, x-8, y+24, L"AC", 2);
}

void draw_k_acsource(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_acsource);
}