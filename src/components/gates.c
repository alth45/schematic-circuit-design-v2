/* src/components/gates.c — Gerbang logika: AND, OR, NOT, NAND, NOR, XOR.

   Simbol IEC/ANSI untuk 6 gerbang.

   Pin 3-input (AND/OR/NAND/NOR/XOR):
     pin0 = A (-40, -10)   pin1 = B (-40, +10)   pin2 = OUT (+40, 0)
   Pin NOT:
     pin0 = IN (-40, 0)    pin1 = OUT (+40, 0)

   Warna hijau saat RUN dan output HIGH. */
#include "ui.h"

/* ---- helper: parabola back curve untuk OR/NOR/XOR ----
   x(t) = base + bulge * (1 - t²),  t = y/15
   base=-20 (body edge), bulge=8 → vertex di (-12, 0), ujung di (-20, ±15). */
static int or_back_x(int y) {
    double t = (double)y / 15.0;
    return -20 + (int)lround(8.0 * (1.0 - t * t));
}
static int xor_extra_x(int y) {
    double t = (double)y / 15.0;
    return -25 + (int)lround(8.0 * (1.0 - t * t));
}

/* ---- helper: bubble (lingkaran putih dengan border) ---- */
static void draw_bubble(HDC hdc, int cx, int cy, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, br);
    Ellipse(hdc, cx - 3, cy - 3, cx + 3, cy + 3);
    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(br);
    DeleteObject(pen);
}

/* ===================== AND / NAND ===================== */
static void draw_and_impl(HDC hdc, int x, int y, BOOL sel, int high, int bubble) {
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    if (sim_running && high) col = RGB(20, 150, 60);

    /* Body: D-shape (flat left, semicircle right). */
    POINT pts[32];
    int n = 0;
    pts[n++] = (POINT){ x - 20, y - 15 };
    pts[n++] = (POINT){ x,      y - 15 };
    for (int d = -90; d <= 90; d += 15) {
        double rad = d * 3.14159265 / 180.0;
        pts[n].x = x + (int)lround(15.0 * cos(rad));
        pts[n].y = y + (int)lround(15.0 * sin(rad));
        n++;
    }
    pts[n++] = (POINT){ x - 20, y + 15 };

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polygon(hdc, pts, n);

    int tip_x = x + 15;
    if (bubble) {
        draw_bubble(hdc, tip_x + 3, y, col);
        tip_x += 6;
    }

    /* Leads */
    MoveToEx(hdc, x - 40, y - 10, NULL); LineTo(hdc, x - 20, y - 10);
    MoveToEx(hdc, x - 40, y + 10, NULL); LineTo(hdc, x - 20, y + 10);
    MoveToEx(hdc, tip_x,   y,      NULL); LineTo(hdc, x + 40, y);

    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(pen);
}

/* ===================== OR / NOR / XOR ===================== */
static void draw_or_impl(HDC hdc, int x, int y, BOOL sel, int high,
                         int bubble, int extra_curve) {
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    if (sim_running && high) col = RGB(20, 150, 60);

    /* Body polygon */
    POINT pts[64];
    int n = 0;
    pts[n++] = (POINT){ x - 20, y - 15 };
    pts[n++] = (POINT){ x,      y - 15 };
    pts[n++] = (POINT){ x + 15, y };
    pts[n++] = (POINT){ x,      y + 15 };
    pts[n++] = (POINT){ x - 20, y + 15 };
    for (int yy = 15; yy >= -15; yy -= 3) {
        int bx = or_back_x(yy);
        pts[n].x = x + bx;
        pts[n].y = y + yy;
        n++;
    }

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polygon(hdc, pts, n);

    /* XOR: extra curve di belakang */
    if (extra_curve) {
        int started = 0;
        for (int yy = -15; yy <= 15; yy += 3) {
            int bx = xor_extra_x(yy);
            if (!started) { MoveToEx(hdc, x + bx, y + yy, NULL); started = 1; }
            else          LineTo(hdc, x + bx, y + yy);
        }
    }

    int tip_x = x + 15;
    if (bubble) {
        draw_bubble(hdc, tip_x + 3, y, col);
        tip_x += 6;
    }

    /* Leads */
    {
        int back_top = extra_curve ? xor_extra_x(-10) : or_back_x(-10);
        int back_bot = extra_curve ? xor_extra_x( 10) : or_back_x( 10);
        MoveToEx(hdc, x - 40, y - 10, NULL); LineTo(hdc, x + back_top, y - 10);
        MoveToEx(hdc, x - 40, y + 10, NULL); LineTo(hdc, x + back_bot, y + 10);
    }
    MoveToEx(hdc, tip_x, y, NULL); LineTo(hdc, x + 40, y);

    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(pen);
}

/* ===================== NOT ===================== */
static int not_high = 0;

static void draw_not_do(HDC hdc, int x, int y, BOOL sel) {
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    if (sim_running && not_high) col = RGB(20, 150, 60);

    POINT tri[3] = {
        { x - 15, y - 15 },
        { x - 15, y + 15 },
        { x + 10, y }
    };

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polygon(hdc, tri, 3);
    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(pen);

    draw_bubble(hdc, x + 13, y, col);

    HPEN lpen = CreatePen(PS_SOLID, 2, col);
    HPEN olp = (HPEN)SelectObject(hdc, lpen);
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 15, y);
    MoveToEx(hdc, x + 16, y, NULL); LineTo(hdc, x + 40, y);
    SelectObject(hdc, olp);
    DeleteObject(lpen);
}

/* ===================== Wrapper status-RUN ===================== */
static int and_high  = 0, or_high  = 0, nand_high = 0;
static int nor_high  = 0, xor_high = 0;

static void and_at (HDC hdc, int x, int y, BOOL sel) { draw_and_impl(hdc, x, y, sel, and_high,  0); }
static void or_at  (HDC hdc, int x, int y, BOOL sel) { draw_or_impl (hdc, x, y, sel, or_high,   0, 0); }
static void nand_at(HDC hdc, int x, int y, BOOL sel) { draw_and_impl(hdc, x, y, sel, nand_high, 1); }
static void nor_at (HDC hdc, int x, int y, BOOL sel) { draw_or_impl (hdc, x, y, sel, nor_high,  1, 0); }
static void xor_at (HDC hdc, int x, int y, BOOL sel) { draw_or_impl (hdc, x, y, sel, xor_high,  0, 1); }
static void not_at (HDC hdc, int x, int y, BOOL sel) { draw_not_do  (hdc, x, y, sel); }

/* ===================== Public draw functions ===================== */
void draw_k_and(HDC hdc, Component *c) {
    and_high = sim_running && sim_gate_out_high(c);
    with_rotation(hdc, c, and_at);
}
void draw_k_or(HDC hdc, Component *c) {
    or_high = sim_running && sim_gate_out_high(c);
    with_rotation(hdc, c, or_at);
}
void draw_k_nand(HDC hdc, Component *c) {
    nand_high = sim_running && sim_gate_out_high(c);
    with_rotation(hdc, c, nand_at);
}
void draw_k_nor(HDC hdc, Component *c) {
    nor_high = sim_running && sim_gate_out_high(c);
    with_rotation(hdc, c, nor_at);
}
void draw_k_xor(HDC hdc, Component *c) {
    xor_high = sim_running && sim_gate_out_high(c);
    with_rotation(hdc, c, xor_at);
}
void draw_k_not(HDC hdc, Component *c) {
    not_high = sim_running && sim_gate_out_high(c);
    with_rotation(hdc, c, not_at);
}