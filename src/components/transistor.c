/* src/components/transistor.c — Simbol transistor NPN (3-pin).

   Simbol NPN standar (rot=0, base di kiri):
     pin0 = Kolektor -> kanan-atas  (+40, -30)
     pin1 = Emitor   -> kanan-bawah (+40, +30)  (panah menunjuk keluar)
     pin2 = Basis    -> kiri-tengah (-40,  0)

   Geometri:
     - Basis : garis horizontal dari (-40, 0) sampai bar di (-8, 0)
     - Bar   : garis vertikal (-8, -15) .. (-8, +15)
     - C & E : garis diagonal dari bar ke pin kanan-atas / kanan-bawah
     - Panah : di sepanjang garis emitor, menunjuk keluar (arah NPN). */
#include "ui.h"

static void draw_transistor(HDC hdc, int x, int y, BOOL sel, int tr_closed)
{
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    if (sim_running && tr_closed) col = RGB(20, 150, 60);

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    if (!pen) return;

    HPEN     oldPen     = (HPEN)SelectObject(hdc, pen);
    HBRUSH   oldBrush   = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    int      oldBkMode  = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldTextCol = GetTextColor(hdc);

    /* 1) Lingkaran badan */
    Ellipse(hdc, x - 21, y - 21, x + 21, y + 21);

    /* 2) Basis: kawat dari pin kiri (-40,0) ke bar di (-8,0) */
    MoveToEx(hdc, x - 40, y, NULL);
    LineTo  (hdc, x - 8,  y);

    /* 3) Bar basis vertikal */
    MoveToEx(hdc, x - 8, y - 15, NULL);
    LineTo  (hdc, x - 8, y + 15);

    /* 4) Kolektor: dari bar ke pin kanan-atas (+40,-30) */
    MoveToEx(hdc, x - 6, y - 6, NULL);
    LineTo  (hdc, x + 40, y - 30);

    /* 5) Emitor: dari bar ke pin kanan-bawah (+40,+30) */
    MoveToEx(hdc, x - 6, y + 6, NULL);
    LineTo  (hdc, x + 40, y + 30);

    /* 6) Panah NPN di jalur emitor, menunjuk keluar (NPN). */
    {
        const double ex = 46.0, ey = 24.0;              /* arah emitor */
        double len = sqrt(ex * ex + ey * ey);
        double ux = ex / len, uy = ey / len;            /* unit arah    */
        double nx = -uy, ny = ux;                       /* normal       */

        double t  = 0.62;                               /* posisi kepala */
        double hx = (x - 6) + ex * t;
        double hy = (y + 6) + ey * t;
        const double HAL = 9.0, HAW = 3.8;              /* panjang/lebar */

        POINT arrow[3] = {
            { (LONG)lround(hx - ux * HAL + nx * HAW),
              (LONG)lround(hy - uy * HAL + ny * HAW) },
            { (LONG)lround(hx - ux * HAL - nx * HAW),
              (LONG)lround(hy - uy * HAL - ny * HAW) },
            { (LONG)lround(hx),
              (LONG)lround(hy) }
        };

        HBRUSH ab = CreateSolidBrush(col);
        if (ab) {
            HBRUSH pb = (HBRUSH)SelectObject(hdc, ab);
            Polygon(hdc, arrow, 3);
            SelectObject(hdc, pb);
            DeleteObject(ab);
        }
    }

    /* 7) Label Q (kanan-atas) + status RUN (kiri-atas). */
    SetTextColor(hdc, RGB(80, 80, 80));
    TextOutW(hdc, x + 25, y - 42, L"Q", 1);

    if (sim_running) {
        const wchar_t *st = tr_closed ? L"ON" : L"OFF";
        SetTextColor(hdc, tr_closed ? RGB(20, 150, 60) : RGB(200, 40, 40));
        TextOutW(hdc, x - 40, y - 42, st, (int)wcslen(st));
    }

    /* 8) Restore state GDI */
    SetTextColor(hdc, oldTextCol);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

/* Wrapper status-run (butuh param tambahan tr_closed) via variabel statis. */
static int tr_at_closed = 0;
static void tr_at(HDC hdc, int x, int y, BOOL sel) {
    draw_transistor(hdc, x, y, sel, tr_at_closed);
}

void draw_k_transistor(HDC hdc, Component *c) {
    tr_at_closed = sim_running ? sim_comp_conducting(c) : 0;
    with_rotation(hdc, c, tr_at);
}