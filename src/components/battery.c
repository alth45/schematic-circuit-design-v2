/* src/components/battery.c — Simbol baterai (DC, 2-pin). */
#include "ui.h"

static void draw_battery(HDC hdc, int x, int y, BOOL sel) {
    HPEN pen = CreatePen(PS_SOLID, 2, sel ? RGB(50,128,255) : RGB(20,20,20));
    HPEN old = (HPEN)SelectObject(hdc, pen);

    MoveToEx(hdc, x-40, y, NULL); LineTo(hdc, x-10, y);
    MoveToEx(hdc, x+10, y, NULL); LineTo(hdc, x+40, y);

    // plat panjang (+)
    MoveToEx(hdc, x-10, y-15, NULL); LineTo(hdc, x-10, y+15);
    // plat pendek (-)
    MoveToEx(hdc, x+10, y-8, NULL);  LineTo(hdc, x+10, y+8);

    SelectObject(hdc, old);
    DeleteObject(pen);

    SetTextColor(hdc, RGB(80,80,80));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x-10, y-30, L"+  -", 4);
    TextOutW(hdc, x-14, y+22, L"BAT", 3);
}

void draw_k_battery(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_battery);
}