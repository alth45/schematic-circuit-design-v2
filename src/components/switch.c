/* src/components/switch.c — Simbol saklar (2-pin, buka/tutup). */
#include "ui.h"

static void draw_switch(HDC hdc, int x, int y, BOOL sel, int sw_on) {
    COLORREF col = sim_running
        ? (sw_on ? RGB(20,150,60) : RGB(200,40,40))
        : (sel ? RGB(50,128,255) : RGB(20,20,20));
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH br = CreateSolidBrush(col);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, br);

    MoveToEx(hdc, x-40, y, NULL); LineTo(hdc, x-15, y);
    MoveToEx(hdc, x+15, y, NULL); LineTo(hdc, x+40, y);

    // titik kontak
    Ellipse(hdc, x-18, y-3, x-12, y+3);
    Ellipse(hdc, x+12, y-3, x+18, y+3);

    SelectObject(hdc, oldb);
    DeleteObject(br);

    // tuas: tertutup = horizontal, terbuka = miring ke atas
    if (sw_on) { MoveToEx(hdc, x-15, y, NULL); LineTo(hdc, x+15, y); }
    else       { MoveToEx(hdc, x-15, y, NULL); LineTo(hdc, x+12, y-15); }

    SelectObject(hdc, old);
    DeleteObject(pen);

    SetTextColor(hdc, RGB(80,80,80));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x-10, y-28, L"SW", 2);
    if (sim_running) {
        const wchar_t *st = sw_on ? L"ON" : L"OFF";
        SetTextColor(hdc, sw_on ? RGB(20,150,60) : RGB(200,40,40));
        TextOutW(hdc, x-10, y+14, st, (int)wcslen(st));
        if (sel) {
            SetTextColor(hdc, RGB(50,128,255));
            TextOutW(hdc, x-24, y-44, L"klik: ON/OFF", 12);
        }
    }
}

/* Wrapper status-run (butuh param tambahan sw_on) via variabel statis. */
static int sw_at_on = 0;
static void sw_at(HDC hdc, int x, int y, BOOL sel) {
    draw_switch(hdc, x, y, sel, sw_at_on);
}

void draw_k_switch(HDC hdc, Component *c) {
    sw_at_on = c->on;
    with_rotation(hdc, c, sw_at);
}