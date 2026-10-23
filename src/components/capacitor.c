/* src/components/capacitor.c — Simbol kapasitor non-polar (2-pin). */
#include "ui.h"

static void draw_capacitor(HDC hdc, int x, int y, BOOL sel) {
    HPEN pen = CreatePen(PS_SOLID, 2, sel ? RGB(50,128,255) : RGB(20,20,20));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    MoveToEx(hdc, x-40, y, NULL); LineTo(hdc, x-6, y);
    MoveToEx(hdc, x+6, y, NULL); LineTo(hdc, x+40, y);
    MoveToEx(hdc, x-6, y-15, NULL); LineTo(hdc, x-6, y+15);
    MoveToEx(hdc, x+6, y-15, NULL); LineTo(hdc, x+6, y+15);
    SelectObject(hdc, ob);
    SelectObject(hdc, old);
    DeleteObject(pen);
    SetTextColor(hdc, RGB(80,80,80));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x-4, y-28, L"C", 1);
}

void draw_k_capacitor(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_capacitor);
}