/* src/components/mosfet.c — Simbol MOSFET N-channel enhancement (3-pin).

   pin0 = D (Drain,  kanan-atas)
   pin1 = S (Source, kanan-bawah)
   pin2 = G (Gate,   kiri-tengah)

   Simbol IEC standar:
     - Gate lead horizontal + gate bar vertikal di kiri.
     - Channel dengan 3 dashes (enhancement mode).
     - Drain diagonal dari channel-atas ke pin D.
     - Source diagonal dari channel-bawah ke pin S.
     - Panah N-channel pada jalur source, menunjuk KE ARAH channel.

   Saat RUN dan MOSFET ON: seluruh simbol berwarna hijau. */
#include "ui.h"

static int mosfet_at_on = 0;

static void draw_mosfet(HDC hdc, int x, int y, BOOL sel) {
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    if (sim_running && mosfet_at_on) col = RGB(20, 150, 60);

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    if (!pen) return;
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    int oldBk = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldTc = GetTextColor(hdc);

    /* Gate lead: pin kiri (-40, 0) ke gate bar (-14, 0) */
    MoveToEx(hdc, x - 40, y, NULL);
    LineTo(hdc, x - 14, y);

    /* Gate bar vertikal */
    MoveToEx(hdc, x - 14, y - 15, NULL);
    LineTo(hdc, x - 14, y + 15);

    /* Channel: 3 dashes (enhancement mode) */
    MoveToEx(hdc, x - 10, y - 15, NULL); LineTo(hdc, x - 10, y - 8);
    MoveToEx(hdc, x - 10, y - 4,  NULL); LineTo(hdc, x - 10, y + 4);
    MoveToEx(hdc, x - 10, y + 8,  NULL); LineTo(hdc, x - 10, y + 15);

    /* Drain: diagonal dari channel-atas ke pin D */
    MoveToEx(hdc, x - 10, y - 10, NULL);
    LineTo(hdc, x + 40, y - 30);

    /* Source: diagonal dari channel-bawah ke pin S */
    MoveToEx(hdc, x - 10, y + 10, NULL);
    LineTo(hdc, x + 40, y + 30);

    /* Panah N-channel pada jalur source, menunjuk KE KIRI (toward channel). */
    {
        const double sx = (double)(x - 10), sy = (double)(y + 10);
        const double ex = (double)(x + 40), ey = (double)(y + 30);
        double dx = ex - sx, dy = ey - sy;
        double len = sqrt(dx * dx + dy * dy);
        if (len > 1.0) {
            double ux = dx / len, uy = dy / len;
            double nx = -uy, ny = ux;
            double t = 0.35;                       /* posisi kepala panah */
            double hx = sx + dx * t;
            double hy = sy + dy * t;
            const double HAL = 8.0, HAW = 3.5;
            /* tip = ujung panah (ke arah channel = -u), base = ke arah pin (+u). */
            POINT arrow[3] = {
                { (LONG)lround(hx),                        (LONG)lround(hy) },
                { (LONG)lround(hx + ux * HAL + nx * HAW),  (LONG)lround(hy + uy * HAL + ny * HAW) },
                { (LONG)lround(hx + ux * HAL - nx * HAW),  (LONG)lround(hy + uy * HAL - ny * HAW) }
            };
            HBRUSH ab = CreateSolidBrush(col);
            if (ab) {
                HBRUSH pb = (HBRUSH)SelectObject(hdc, ab);
                Polygon(hdc, arrow, 3);
                SelectObject(hdc, pb);
                DeleteObject(ab);
            }
        }
    }

    SetTextColor(hdc, RGB(80, 80, 80));
    TextOutW(hdc, x + 22, y - 44, L"Q", 1);           /* label di kanan-atas */

    if (sim_running) {
        const wchar_t *st = mosfet_at_on ? L"ON" : L"OFF";
        SetTextColor(hdc, mosfet_at_on ? RGB(20, 150, 60) : RGB(200, 40, 40));
        TextOutW(hdc, x - 40, y - 44, st, (int)wcslen(st));
    }

    SetTextColor(hdc, oldTc);
    SetBkMode(hdc, oldBk);
    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(pen);
}

void draw_k_mosfet(HDC hdc, Component *c) {
    mosfet_at_on = (sim_running && sim_comp_conducting(c)) ? 1 : 0;
    with_rotation(hdc, c, draw_mosfet);
}