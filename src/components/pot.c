/* src/components/pot.c — Simbol potensiometer (3-pin: A, B, W).

   pin0 = A (kiri), pin1 = B (kanan), pin2 = W (wiper, atas-tengah).

   Posisi panah wiper MENGIKUTI c->on (0% di kiri, 100% di kanan),
   memberi feedback visual posisi wiper tanpa perlu info dialog. */
#include "ui.h"

static void draw_pot_body(HDC hdc, int x, int y, BOOL sel, int pos) {
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* Lead A -> badan (kiri) */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 20, y);
    /* Lead B -> badan (kanan) */
    MoveToEx(hdc, x + 20, y, NULL); LineTo(hdc, x + 40, y);

    /* Badan resistor */
    Rectangle(hdc, x - 20, y - 8, x + 20, y + 8);

    /* Posisi panah wiper: -14 .. +14 (dalam badan) sesuai pos 0..100. */
    int arrow_x = -14 + (pos * 28) / 100;
    if (arrow_x < -14) arrow_x = -14;
    if (arrow_x >  14) arrow_x =  14;

    /* Wire wiper: dari pin2 (0,-35) turun ke (arrow_x, -18). */
    MoveToEx(hdc, x, y - 35, NULL);
    LineTo(hdc, x + arrow_x, y - 18);

    /* Panah wiper: dari (arrow_x, -18) turun ke (arrow_x, -8) → menyentuh
       badan resistor di posisi wiper. */
    MoveToEx(hdc, x + arrow_x, y - 18, NULL);
    LineTo(hdc, x + arrow_x, y - 8);

    /* Kepala panah segitiga kecil (menunjuk ke bawah). */
    {
        int ax = x + arrow_x;
        POINT tip[3] = {
            { ax,     y - 8  },
            { ax - 4, y - 13 },
            { ax + 4, y - 13 }
        };
        HBRUSH ab = CreateSolidBrush(col);
        if (ab) {
            HBRUSH pb = (HBRUSH)SelectObject(hdc, ab);
            Polygon(hdc, tip, 3);
            SelectObject(hdc, pb);
            DeleteObject(ab);
        }
    }

    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(pen);
}

static int pot_cur_pos = 50;

static void pot_at(HDC hdc, int x, int y, BOOL sel) {
    draw_pot_body(hdc, x, y, sel, pot_cur_pos);
}

void draw_k_pot(HDC hdc, Component *c) {
    pot_cur_pos = c->on;
    if (pot_cur_pos < 0) pot_cur_pos = 0;
    if (pot_cur_pos > 100) pot_cur_pos = 100;
    with_rotation(hdc, c, pot_at);
}