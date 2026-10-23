/* src/components/ground.c — Simbol ground (1-pin). */
#include "ui.h"

static void draw_ground(HDC hdc, int x, int y, BOOL sel) {
    HPEN pen = CreatePen(PS_SOLID, 2, sel ? RGB(50,128,255) : RGB(20,20,20));
    HPEN old = (HPEN)SelectObject(hdc, pen);

    MoveToEx(hdc, x, y-30, NULL); LineTo(hdc, x, y);
    MoveToEx(hdc, x-20, y, NULL); LineTo(hdc, x+20, y);
    MoveToEx(hdc, x-12, y+8, NULL); LineTo(hdc, x+12, y+8);
    MoveToEx(hdc, x-5, y+16, NULL); LineTo(hdc, x+5, y+16);

    SelectObject(hdc, old);
    DeleteObject(pen);
}

void draw_k_ground(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_ground);
}