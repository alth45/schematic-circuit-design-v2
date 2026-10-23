#include "toolbar_intern.h"

/* Tombol aksi Clear / Save / Open / Run (dipindah dari toolbar.c). */

void palette_draw_clear(HDC hdc, int bx, int by) {
    HPEN xp = CreatePen(PS_SOLID, 3, RGB(200,40,40));
    HPEN oxp = (HPEN)SelectObject(hdc, xp);
    int cx = bx + TOOLBAR_BTN_W / 2, cy = by + 15;
    MoveToEx(hdc, cx - 9, cy - 7, NULL); LineTo(hdc, cx + 9, cy + 7);
    MoveToEx(hdc, cx + 9, cy - 7, NULL); LineTo(hdc, cx - 9, cy + 7);
    SelectObject(hdc, oxp);
    DeleteObject(xp);
    SetTextColor(hdc, RGB(90,90,90));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, bx + 13, by + 28, L"Clear", 5);
}

void palette_draw_save(HDC hdc, int bx, int by) {
    HPEN sp = CreatePen(PS_SOLID, 2, RGB(30,120,60));
    HPEN osp2 = (HPEN)SelectObject(hdc, sp);
    HBRUSH sob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    int cx = bx + TOOLBAR_BTN_W / 2, cy = by + 14;
    Rectangle(hdc, cx - 11, cy - 9, cx + 11, cy + 9);
    MoveToEx(hdc, cx - 11, cy - 9, NULL); LineTo(hdc, cx - 4, cy - 9);
    LineTo(hdc, cx - 4, cy - 2); LineTo(hdc, cx - 11, cy - 2);
    MoveToEx(hdc, cx - 3, cy + 5, NULL); LineTo(hdc, cx + 1, cy + 5);
    LineTo(hdc, cx + 5, cy + 1);
    SelectObject(hdc, sob);
    SelectObject(hdc, osp2);
    DeleteObject(sp);
    SetTextColor(hdc, RGB(90,90,90));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, bx + 14, by + 28, L"Save", 4);
}

void palette_draw_open(HDC hdc, int bx, int by) {
    HPEN op2 = CreatePen(PS_SOLID, 2, RGB(30,90,200));
    HPEN osp3 = (HPEN)SelectObject(hdc, op2);
    HBRUSH oob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    int cx = bx + TOOLBAR_BTN_W / 2, cy = by + 14;
    Rectangle(hdc, cx - 11, cy - 6, cx + 11, cy + 9);
    MoveToEx(hdc, cx - 6, cy - 6, NULL); LineTo(hdc, cx - 2, cy - 11);
    LineTo(hdc, cx + 6, cy - 11); LineTo(hdc, cx + 6, cy - 6);
    MoveToEx(hdc, cx, cy - 3, NULL); LineTo(hdc, cx, cy + 5);
    MoveToEx(hdc, cx - 4, cy + 1, NULL); LineTo(hdc, cx, cy + 5);
    LineTo(hdc, cx + 4, cy + 1);
    SelectObject(hdc, oob);
    SelectObject(hdc, osp3);
    DeleteObject(op2);
    SetTextColor(hdc, RGB(90,90,90));
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, bx + 14, by + 28, L"Open", 4);
}

void palette_draw_run(HDC hdc, int bx, int by) {
    int cx = bx + TOOLBAR_BTN_W / 2, cy = by + 14;

    /* Disabled: canvas kosong & belum RUN. */
    int disabled = (!sim_running && comp_count == 0);

    if (sim_running) {
        /* Tombol Stop — merah (selalu enabled). */
        HBRUSH sb2 = CreateSolidBrush(RGB(200,40,40));
        HBRUSH osb = (HBRUSH)SelectObject(hdc, sb2);
        HPEN sp3 = CreatePen(PS_SOLID, 1, RGB(200,40,40));
        HPEN osp5 = (HPEN)SelectObject(hdc, sp3);
        Rectangle(hdc, cx - 8, cy - 8, cx + 8, cy + 8);
        SelectObject(hdc, osp5);
        SelectObject(hdc, osb);
        DeleteObject(sp3);
        DeleteObject(sb2);
        SetTextColor(hdc, RGB(200,40,40));
    } else if (disabled) {
        /* Tombol Run disabled — abu-abu pucat. */
        COLORREF col = RGB(190, 190, 190);
        POINT tri[3] = { {cx - 8, cy - 9}, {cx - 8, cy + 9}, {cx + 9, cy} };
        HBRUSH rb = CreateSolidBrush(col);
        HBRUSH orb = (HBRUSH)SelectObject(hdc, rb);
        HPEN rp = CreatePen(PS_SOLID, 1, col);
        HPEN orp = (HPEN)SelectObject(hdc, rp);
        Polygon(hdc, tri, 3);
        SelectObject(hdc, orp);
        SelectObject(hdc, orb);
        DeleteObject(rp);
        DeleteObject(rb);
        SetTextColor(hdc, RGB(185, 185, 185));
    } else {
        /* Tombol Run enabled — hijau. */
        POINT tri[3] = { {cx - 8, cy - 9}, {cx - 8, cy + 9}, {cx + 9, cy} };
        HBRUSH rb = CreateSolidBrush(RGB(20,150,60));
        HBRUSH orb = (HBRUSH)SelectObject(hdc, rb);
        HPEN rp = CreatePen(PS_SOLID, 1, RGB(20,150,60));
        HPEN orp = (HPEN)SelectObject(hdc, rp);
        Polygon(hdc, tri, 3);
        SelectObject(hdc, orp);
        SelectObject(hdc, orb);
        DeleteObject(rp);
        DeleteObject(rb);
        SetTextColor(hdc, RGB(20,150,60));
    }

    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, bx + (sim_running ? 13 : 16), by + 28,
             sim_running ? L"Stop" : L"Run", sim_running ? 4 : 3);
}
