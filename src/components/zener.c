/* src/components/zener.c — Simbol dioda Zener (2-pin, A→K dengan ekor "Z").

   pin0 = A (anode, kiri), pin1 = K (katode, kanan).
   Nilai v = Vz (tegangan breakdown), default 5.1 V.

   Saat RUN:
   - Forward (menghantar)  : garis + panah arah alir HIJAU.
   - Reverse breakdown     : garis + sayap katoda ORANGE (meng-clamp).
   - Terbuka / default     : hitam. */
#include "ui.h"

static int zener_state = 0;  /* 0 = off, 1 = forward, 2 = breakdown */

static void draw_zener_body(HDC hdc, int x, int y, BOOL sel) {
    COLORREF col;
    if (sel) col = RGB(50, 128, 255);
    else if (sim_running && zener_state == 2) col = RGB(220, 130, 20);
    else if (sim_running && zener_state == 1) col = RGB(20, 150, 60);
    else col = RGB(20, 20, 20);

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* lead kiri & kanan */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 12, y);
    MoveToEx(hdc, x + 12, y, NULL); LineTo(hdc, x + 40, y);

    /* segitiga anoda -> katoda */
    POINT tri[3] = { { x - 12, y - 15 }, { x - 12, y + 15 }, { x + 12, y } };
    Polygon(hdc, tri, 3);

    /* garis katoda vertikal */
    MoveToEx(hdc, x + 12, y - 15, NULL); LineTo(hdc, x + 12, y + 15);

    /* sayap Zener: atas -> kiri, bawah -> kanan (bentuk "Z"). */
    MoveToEx(hdc, x + 12, y - 15, NULL); LineTo(hdc, x + 5,  y - 15);
    MoveToEx(hdc, x + 12, y + 15, NULL); LineTo(hdc, x + 19, y + 15);

    SelectObject(hdc, oldb);
    SelectObject(hdc, old);
    DeleteObject(pen);

    SetTextColor(hdc, RGB(80, 80, 80));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x - 4, y - 30, L"Z", 1);
}

void draw_k_zener(HDC hdc, Component *c) {
    if (sim_running) {
        if (sim_zener_breakdown(c)) zener_state = 2;
        else if (sim_zener_fwd(c))  zener_state = 1;
        else                        zener_state = 0;
    } else {
        zener_state = 0;
    }
    with_rotation(hdc, c, draw_zener_body);
}