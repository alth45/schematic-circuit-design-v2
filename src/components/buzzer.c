/* src/components/buzzer.c — Simbol buzzer / piezo (2-pin, frekuensi Hz).

   Simbol buzzer standard (IEC 60617):
     - Plat datar (back of buzzer) di KIRI: garis vertikal.
     - Dome (front of buzzer) di KANAN: setengah-elips.
     - Lead kiri masuk ke plat datar, lead kanan keluar dari puncak dome.
     - Gelombang suara: 2 busur (diam) atau 3 busur + hijau (berbunyi).

   pin0 = kiri (input/+), pin1 = kanan (gnd/-). Nilai v = frekuensi (Hz).

   Dome digambar via polyline eksak (bukan Arc) karena Arc GDI
   memproyeksikan start/end yang tidak persis di elips. */
#include "ui.h"

/* Gambar satu busur lingkaran sebagai polyline; derajat dari deg_from
   ke deg_to (deg_to > deg_from; kelipatan 5° cukup mulus). */
static void draw_arc_polyline(HDC hdc, int cx, int cy, int r,
                              int deg_from, int deg_to) {
    int started = 0;
    for (int deg = deg_from; deg <= deg_to; deg += 5) {
        double rad = deg * 3.14159265358979 / 180.0;
        int px = cx + (int)lround(r * cos(rad));
        int py = cy + (int)lround(r * sin(rad));
        if (!started) { MoveToEx(hdc, px, py, NULL); started = 1; }
        else          LineTo(hdc, px, py);
    }
}

static void draw_buzzer(HDC hdc, int x, int y, BOOL sel, int on) {
    COLORREF col = (sim_running && on) ? RGB(30, 160, 60)
                 : sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* --- 1) Plat datar (back of buzzer) --- */
    MoveToEx(hdc, x - 14, y - 15, NULL);
    LineTo(hdc, x - 14, y + 15);

    /* --- 2) Dome: setengah-elips via polyline eksak ---
       Center di (x-14, y), rx=28 (puncak di x+14), ry=15.
       Loop dari deg=90 (bawah) ke -90 (atas), melewati 0 (puncak). */
    {
        const int cx = x - 14, cy = y;
        const int rx = 28, ry = 15;
        int started = 0;
        for (int deg = 90; deg >= -90; deg -= 5) {
            double rad = deg * 3.14159265358979 / 180.0;
            int px = cx + (int)lround(rx * cos(rad));
            int py = cy + (int)lround(ry * sin(rad));
            if (!started) { MoveToEx(hdc, px, py, NULL); started = 1; }
            else          LineTo(hdc, px, py);
        }
    }

    /* --- 3) Lead kiri: pin (-40, 0) -> plat datar (-14, 0) --- */
    MoveToEx(hdc, x - 40, y, NULL);
    LineTo(hdc, x - 14, y);

    /* --- 4) Lead kanan: puncak dome (x+14, 0) -> pin (x+40, 0) --- */
    MoveToEx(hdc, x + 14, y, NULL);
    LineTo(hdc, x + 40, y);

    /* --- 5) Gelombang suara: 2 (OFF) atau 3 (ON) busur konsentris
           di kuadran kanan-ATAS dome, tidak menabrak lead. --- */
    {
        int n_arcs = (sim_running && on) ? 3 : 2;
        static const int radii[3] = { 8, 13, 18 };
        for (int i = 0; i < n_arcs; i++)
            draw_arc_polyline(hdc, x + 14, y - 5, radii[i], -75, -15);
    }

    SelectObject(hdc, oldb);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

/* Wrapper status-run (signature sesuai with_rotation). */
static int buz_at_on = 0;
static void buz_at(HDC hdc, int x, int y, BOOL sel) {
    draw_buzzer(hdc, x, y, sel, buz_at_on);
}

void draw_k_buzzer(HDC hdc, Component *c) {
    buz_at_on = sim_buzzer_on(c);
    with_rotation(hdc, c, buz_at);
}