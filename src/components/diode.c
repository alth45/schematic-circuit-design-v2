/* src/components/diode.c — Simbol dioda (2-pin, A→K).

   Arah sama dgn LED: pin0 = Anoda (kiri), pin1 = Katoda (kanan).
   Saat RUN & forward-biased: garis hijau + panah arah alir di bawah. */
#include "ui.h"

static void draw_diode(HDC hdc, int x, int y, BOOL sel, int fwd) {
    COLORREF col = sel ? RGB(50,128,255) : RGB(20,20,20);
    if (sim_running && fwd) col = RGB(20,150,60);   /* menghantar = hijau */
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* kawat lead ke titik pin (-40 / +40) */
    MoveToEx(hdc, x-40, y, NULL); LineTo(hdc, x-12, y);
    MoveToEx(hdc, x+12, y, NULL); LineTo(hdc, x+40, y);

    /* segitiga anoda */
    POINT tri[3] = { {x-12, y-15}, {x-12, y+15}, {x+12, y} };
    Polygon(hdc, tri, 3);

    /* garis katoda (menempel persis di ujung kanan segitiga) */
    MoveToEx(hdc, x+12, y-15, NULL); LineTo(hdc, x+12, y+15);

    SelectObject(hdc, ob);
    SelectObject(hdc, old);
    DeleteObject(pen);

    SetTextColor(hdc, RGB(80,80,80));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x-4, y-30, L"D", 1);

    /* saat RUN & forward: panah arah alir A->K di bawah simbol */
    if (sim_running && fwd) {
        HPEN ap = CreatePen(PS_SOLID, 2, RGB(20,150,60));
        HPEN oap = (HPEN)SelectObject(hdc, ap);
        MoveToEx(hdc, x-14, y+22, NULL); LineTo(hdc, x+10, y+22);
        MoveToEx(hdc, x+10, y+22, NULL); LineTo(hdc, x+4,  y+18);
        MoveToEx(hdc, x+10, y+22, NULL); LineTo(hdc, x+4,  y+26);
        SelectObject(hdc, oap);
        DeleteObject(ap);
    }
}

/* Wrapper status-run (butuh param tambahan fwd) via variabel statis. */
static int diode_at_fwd = 0;
static void diode_at(HDC hdc, int x, int y, BOOL sel) {
    draw_diode(hdc, x, y, sel, diode_at_fwd);
}

void draw_k_diode(HDC hdc, Component *c) {
    diode_at_fwd = sim_running ? sim_diode_fwd(c) : 0;
    with_rotation(hdc, c, diode_at);
}