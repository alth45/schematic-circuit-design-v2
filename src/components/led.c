/* src/components/led.c — Simbol LED (2-pin, warna + glow saat menyala).

   pin0 = anoda (kiri), pin1 = katoda (kanan).
   Nilai v = Vf (volt). c->led_color memilih warna: 0=merah, 1=hijau,
   2=kuning, 3=biru. Warna berlaku untuk badan LED (saat OFF maupun ON)
   dan halo glow saat menyala. */
#include "ui.h"

/* Palet warna per LED color index. */
typedef struct {
    COLORREF body;       /* warna badan saat OFF (tinted glass) */
    COLORREF lit;        /* warna badan saat ON */
    COLORREF glow;       /* isi halo */
    COLORREF glow_edge;  /* border halo */
} LedPalette;

static const LedPalette led_pal[4] = {
    /* 0 = merah */
    { RGB(200, 30, 30), RGB(255, 80, 40), RGB(255, 200, 140), RGB(255, 140, 60) },
    /* 1 = hijau */
    { RGB(30, 160, 50), RGB(100, 255, 100), RGB(180, 255, 180), RGB(80, 220, 80) },
    /* 2 = kuning */
    { RGB(220, 180, 20), RGB(255, 240, 60), RGB(255, 255, 180), RGB(255, 220, 60) },
    /* 3 = biru */
    { RGB(50, 100, 230), RGB(120, 180, 255), RGB(180, 220, 255), RGB(100, 160, 255) }
};

static void draw_led(HDC hdc, int x, int y, BOOL sel, int lit, int color) {
    if (color < 0 || color > 3) color = 0;
    const LedPalette *pal = &led_pal[color];

    COLORREF col;
    if (sim_running && lit)       col = pal->lit;
    else if (sel)                 col = RGB(50, 128, 255);
    else                          col = pal->body;

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);

    /* Glow saat menyala — warna mengikuti palet. */
    if (sim_running && lit) {
        HBRUSH glow = CreateSolidBrush(pal->glow);
        HBRUSH og = (HBRUSH)SelectObject(hdc, glow);
        HPEN gp = CreatePen(PS_SOLID, 1, pal->glow_edge);
        HPEN og2 = (HPEN)SelectObject(hdc, gp);
        Ellipse(hdc, x - 22, y - 25, x + 22, y + 25);
        SelectObject(hdc, og2);
        SelectObject(hdc, og);
        DeleteObject(gp);
        DeleteObject(glow);
        SelectObject(hdc, pen);
    }

    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* Lead kiri & kanan */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 12, y);
    MoveToEx(hdc, x + 12, y, NULL); LineTo(hdc, x + 40, y);

    /* Segitiga dioda (anoda -> katoda) */
    POINT tri[3] = { {x-12, y-15}, {x-12, y+15}, {x+12, y} };
    Polygon(hdc, tri, 3);

    /* Garis katoda */
    MoveToEx(hdc, x + 12, y - 15, NULL); LineTo(hdc, x + 12, y + 15);

    /* Dua panah cahaya keluar */
    MoveToEx(hdc, x - 5, y - 18, NULL); LineTo(hdc, x - 12, y - 30);
    MoveToEx(hdc, x + 3, y - 18, NULL); LineTo(hdc, x - 4, y - 30);

    SelectObject(hdc, oldb);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

/* Wrapper status-RUN: lit-state + warna disimpan statis karena signature
   with_rotation hanya menerima (HDC, int, int, BOOL). */
static int led_at_lit   = 0;
static int led_at_color = 0;

static void led_at(HDC hdc, int x, int y, BOOL sel) {
    draw_led(hdc, x, y, sel, led_at_lit, led_at_color);
}

void draw_k_led(HDC hdc, Component *c) {
    led_at_lit   = sim_comp_lit(c);
    led_at_color = c->led_color;
    with_rotation(hdc, c, led_at);
}