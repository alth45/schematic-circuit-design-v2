/* src/components/resistor.c — Simbol resistor (2-pin standar). */
#include "ui.h"

static void draw_resistor(HDC hdc, int x, int y, BOOL sel) {
    HPEN pen = CreatePen(PS_SOLID, 2, sel ? RGB(50,128,255) : RGB(20,20,20));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HPEN oldBrush = (HPEN)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // lead kiri & kanan
    MoveToEx(hdc, x-40, y, NULL); LineTo(hdc, x-20, y);
    MoveToEx(hdc, x+20, y, NULL); LineTo(hdc, x+40, y);
    // kotak
    Rectangle(hdc, x-20, y-10, x+20, y+10);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, old);
    DeleteObject(pen);

    SetTextColor(hdc, RGB(80,80,80));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x-6, y-22, L"R", 1);
}

void draw_k_resistor(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_resistor);
}