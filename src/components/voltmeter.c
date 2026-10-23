/* src/components/voltmeter.c — Simbol voltmeter (2-pin, ideal).

   Voltmeter = alat ukur BEDA POTENSIAL antara dua titik. Dipasang PARALEL:
   pin0 = kutub (+) ke titik A, pin1 = kutub (-) ke titik B. Ideal: impedansi
   tak hingga → tidak menghantar arus sama sekali, sehingga memasangnya
   tidak mengubah perilaku rangkaian.

   Saat mode RUN dan kedua ujung terhubung: angka bacaan (volt) tampil
   hijau di bawah simbol; bila salah satu ujung mengambang tampil "-". */
#include "ui.h"

static void draw_voltmeter(HDC hdc, int x, int y, BOOL sel, const wchar_t *reading) {
    COLORREF col = (sim_running && reading) ? RGB(30, 160, 60)
                 : sel ? RGB(50, 128, 255) : RGB(20, 20, 20);
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    /* kawat lead kiri (pin0 = "+") & kanan (pin1 = "-") */
    MoveToEx(hdc, x - 40, y, NULL); LineTo(hdc, x - 18, y);
    MoveToEx(hdc, x + 18, y, NULL); LineTo(hdc, x + 40, y);

    /* badan lingkaran + huruf V */
    Ellipse(hdc, x - 18, y - 18, x + 18, y + 18);
    INT obm = SetBkMode(hdc, TRANSPARENT);
    HFONT hf = CreateFontW(20, 12, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    HFONT of = (HFONT)SelectObject(hdc, hf);
    SetTextColor(hdc, col);
    TextOutW(hdc, x - 5, y - 11, L"V", 1);
    SelectObject(hdc, of);
    DeleteObject(hf);
    SetBkMode(hdc, obm);

    /* tanda polaritas kecil: "+" dekat pin0, "-" dekat pin1 */
    MoveToEx(hdc, x - 26, y - 8, NULL); LineTo(hdc, x - 26, y - 2);
    MoveToEx(hdc, x - 29, y - 5, NULL); LineTo(hdc, x - 23, y - 5);
    MoveToEx(hdc, x + 23, y - 5, NULL); LineTo(hdc, x + 29, y - 5);

    SelectObject(hdc, oldb);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

/* Bacaan disimpan sementara agar wrapper with_rotation (signature tetap)
   bisa menampilkan teks tegak di posisi dunia tanpa rotasi. */
static Component *vm_cur = NULL;

static void vm_at(HDC hdc, int x, int y, BOOL sel) {
    wchar_t rd[32] = L"";
    const wchar_t *readings = NULL;
    if (sim_running && vm_cur) {
        double v = sim_voltmeter_reading(vm_cur);
        if (v < 0.0) wcscpy_s(rd, 32, L"-");           /* mengambang / tak terhubung */
        else {
            swprintf(rd, 32, L"%.3g V", v);
            readings = rd;
        }
    }
    draw_voltmeter(hdc, x, y, sel, readings);

    /* angka bacaan digambar tegak (di luar transformasi rotasi) */
    if (readings && !g_minimap_mode) {
        int px, py;               /* pusat badan voltmeter pada koordinat dunia */
        px = vm_cur->x; py = vm_cur->y;
        INT obm = SetBkMode(hdc, TRANSPARENT);
        HFONT hf = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
        HFONT of = (HFONT)SelectObject(hdc, hf);
        SIZE sz; GetTextExtentPoint32W(hdc, rd, (int)wcslen(rd), &sz);
        /* kotak putih tipis agar terbaca di atas grid/kabel */
        HPEN bp = CreatePen(PS_SOLID, 1, RGB(220, 220, 220));
        HPEN obp = (HPEN)SelectObject(hdc, bp);
        HBRUSH bb = CreateSolidBrush(RGB(255, 255, 255));
        HBRUSH obb = (HBRUSH)SelectObject(hdc, bb);
        Rectangle(hdc, px - sz.cx / 2 - 4, py + 22, px + sz.cx / 2 + 4, py + 22 + sz.cy + 2);
        SelectObject(hdc, obb); SelectObject(hdc, obp);
        DeleteObject(bb); DeleteObject(bp);
        SetTextColor(hdc, RGB(30, 130, 50));
        TextOutW(hdc, px - sz.cx / 2, py + 23, rd, (int)wcslen(rd));
        SelectObject(hdc, of);
        DeleteObject(hf);
        SetBkMode(hdc, obm);
    }
}

void draw_k_voltmeter(HDC hdc, Component *c) {
    vm_cur = c;
    with_rotation(hdc, c, vm_at);
    vm_cur = NULL;
}