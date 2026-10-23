/* src/components/ldr.c — Simbol LDR / fotoresistor (2-pin).

   Bentuk: kotak resistitor dengan dua panah CAHAYA masuk (menunjukkan
   sensor bergantung-cahaya). Nilai v = resistansi saat GELAP (Ohm);
   saat TERANG resistansi efektif ~ 1% dari nilai gelap (LDR_LIGHT_RATIO).
   pin0 = kiri, pin1 = kanan (sama seperti resistor). */
#include "ui.h"

static void draw_ldr(HDC hdc, int x, int y, BOOL sel) {
    COLORREF col = sel ? RGB(50,128,255) : RGB(20,20,20);
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* kawat lead kiri & kanan */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 22, y);
    MoveToEx(hdc, x + 22, y, NULL); LineTo(hdc, x + 40, y);

    /* badan fotoresistor: kotak bergaris tengah khas LDR */
    Rectangle(hdc, x - 22, y - 12, x + 22, y + 12);
    MoveToEx(hdc, x - 22, y - 6, NULL); LineTo(hdc, x + 22, y - 6);
    MoveToEx(hdc, x - 22, y + 6, NULL); LineTo(hdc, x + 22, y + 6);

    /* dua panah cahaya MASUK (arah ke badan, dari kiri-atas) */
    MoveToEx(hdc, x - 30, y - 30, NULL); LineTo(hdc, x - 14, y - 16);
    MoveToEx(hdc, x - 14, y - 16, NULL); LineTo(hdc, x - 22, y - 17);
    MoveToEx(hdc, x - 14, y - 16, NULL); LineTo(hdc, x - 15, y - 24);
    MoveToEx(hdc, x - 18, y - 34, NULL); LineTo(hdc,  x - 2, y - 20);
    MoveToEx(hdc,  x - 2, y - 20, NULL); LineTo(hdc, x - 10, y - 21);
    MoveToEx(hdc,  x - 2, y - 20, NULL); LineTo(hdc,  x - 3, y - 28);

    SelectObject(hdc, oldb);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void ldr_at(HDC hdc, int x, int y, BOOL sel) {
    draw_ldr(hdc, x, y, sel);
}

void draw_k_ldr(HDC hdc, Component *c) {
    with_rotation(hdc, c, ldr_at);
}