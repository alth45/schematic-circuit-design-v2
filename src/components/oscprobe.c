/* src/components/oscprobe.c — Simbol probe osiloskop (2-pin).

   pin0 = sinyal (probe tip, ujung kiri),
   pin1 = referensi (ground clip, ujung kanan).
   Bentuk: probe penusuk (segitiga) + badan bulat + klip referensi.
   Tidak punya nilai editable; bacaan gelombang via oscilloscope.c. */
#include "ui.h"

static void draw_oscprobe(HDC hdc, int x, int y, BOOL sel) {
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* lead kiri (sinyal) & kanan (referensi) */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 20, y);
    MoveToEx(hdc, x + 20, y, NULL); LineTo(hdc, x + 40, y);

    /* tip probe: segitiga runcing ke kiri */
    POINT tip[3] = { { x - 20, y }, { x - 8, y - 10 }, { x - 8, y + 10 } };
    Polygon(hdc, tip, 3);

    /* badan probe: lingkaran memanjang */
    Ellipse(hdc, x - 8, y - 10, x + 20, y + 10);

    /* klip referensi: titik terisi */
    HBRUSH dot = CreateSolidBrush(col);
    HBRUSH od  = (HBRUSH)SelectObject(hdc, dot);
    Ellipse(hdc, x + 15, y - 4, x + 23, y + 4);
    SelectObject(hdc, od);
    DeleteObject(dot);

    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(80, 80, 80));
    TextOutW(hdc, x - 12, y - 30, L"OSC", 3);
}

void draw_k_oscprobe(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_oscprobe);
}