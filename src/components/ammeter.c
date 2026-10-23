/* src/components/ammeter.c — Simbol ammeter (2-pin, ideal ~0Ω).

   Simbol: lingkaran dengan huruf "A" + lead kiri/kanan.
   Pin0 = (+) masuk jalur, pin1 = (-) keluar jalur (dipasang SERI).

   Rotasi: badan simbol mengikuti with_rotation (world transform GDI);
   angka bacaan arus digambar SETELAH with_rotation kembali, jadi selalu
   tegak (tidak terpengaruh rotasi komponen). Huruf "A" digambar sebagai
   vektor (bukan font) agar ikut berputar bersama simbol dan konsisten
   dengan ikon toolbar. */
#include "ui.h"

static void draw_ammeter_body(HDC hdc, int x, int y, BOOL sel) {
    COLORREF col = sel ? RGB(50, 128, 255) : RGB(20, 20, 20);

    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);

    /* lead kiri & kanan */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 20, y);
    MoveToEx(hdc, x + 20, y, NULL); LineTo(hdc, x + 40, y);

    /* lingkaran putih + border (border = pen col di atas) */
    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, bg);
    Ellipse(hdc, x - 20, y - 20, x + 20, y + 20);
    SelectObject(hdc, oldB);
    DeleteObject(bg);

    /* huruf "A" vektor: dua diagonal + crossbar, warna biru */
    HPEN ap = CreatePen(PS_SOLID, 2, RGB(20, 20, 180));
    HPEN oap = (HPEN)SelectObject(hdc, ap);
    MoveToEx(hdc, x - 7, y + 8, NULL); LineTo(hdc, x,     y - 8);
    MoveToEx(hdc, x,     y - 8, NULL); LineTo(hdc, x + 7, y + 8);
    MoveToEx(hdc, x - 4, y + 2, NULL); LineTo(hdc, x + 4, y + 2);
    SelectObject(hdc, oap);
    DeleteObject(ap);

    SelectObject(hdc, old);
    DeleteObject(pen);
}

void draw_k_ammeter(HDC hdc, Component *c) {
    /* 1) Badan simbol mengikuti rotasi komponen. */
    with_rotation(hdc, c, draw_ammeter_body);

    /* 2) Bacaan arus: hanya saat RUN, digambar tegak (world coords). */
    if (sim_running && !g_minimap_mode) {
        double current = sim_ammeter_reading(c);
        int has_val = (current >= 0.0);
        wchar_t val[32];
        if (has_val) sim_format_si(current, L"A", val, 32);
        else         wcscpy_s(val, 32, L"-");

        HFONT hSmall = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HFONT oSmall = (HFONT)SelectObject(hdc, hSmall);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, has_val ? RGB(0, 150, 0) : RGB(150, 150, 150));

        SIZE sz;
        GetTextExtentPoint32W(hdc, val, (int)wcslen(val), &sz);
        TextOutW(hdc, c->x - sz.cx / 2, c->y + 22, val, (int)wcslen(val));

        SelectObject(hdc, oSmall);
        DeleteObject(hSmall);
    }
}