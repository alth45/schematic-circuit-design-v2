#include "toolbar_intern.h"

/* State scroll horizontal untuk area komponen. */
int g_toolbar_scroll_x = 0;
/* State drag scrollbar. */
static int sb_drag_active        = 0;
static int sb_drag_start_mx      = 0;
static int sb_drag_start_scroll  = 0;

static HFONT font_btn_label(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(12, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        0, 0, 0, 0, 0, L"Segoe UI");
    return h;
}

/* ===================== Layout helpers ===================== */

int toolbar_scroll_start_x(void) {
    return 8 + TOOLBAR_FIXED_LEFT_COUNT * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP);
}

int toolbar_scroll_end_x(int W) {
    int end   = W - 8 - TOOLBAR_FIXED_RIGHT_COUNT * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP);
    int start = toolbar_scroll_start_x();
    return (end > start) ? end : start;
}

int toolbar_max_scroll(int W) {
    int region_w  = toolbar_scroll_end_x(W) - toolbar_scroll_start_x();
    int content_w = TOOLBAR_SCROLL_COUNT * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP) - TOOLBAR_BTN_GAP;
    if (region_w <= 0) return 0;
    if (region_w >= content_w) return 0;
    return content_w - region_w;
}

int toolbar_get_scroll_x(int W) {
    int max = toolbar_max_scroll(W);
    if (g_toolbar_scroll_x < 0)   g_toolbar_scroll_x = 0;
    if (g_toolbar_scroll_x > max) g_toolbar_scroll_x = max;
    return g_toolbar_scroll_x;
}

int toolbar_action_x(int action_idx, int W) {
    /* action 0=Clear ... 3=Run; Run paling kanan. */
    int base = W - 8 - TOOLBAR_FIXED_RIGHT_COUNT * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP)
                     + TOOLBAR_BTN_GAP;
    return base + action_idx * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP);
}

/* ===================== Hit-test ===================== */

int toolbar_hit(int x, int y, int W) {
    return toolbar_tool_at(x, y, W, NULL);
}

int toolbar_tool_at(int x, int y, int W, int *is_clear) {
    if (is_clear) *is_clear = 0;
    if (y < 8 || y > 8 + TOOLBAR_BTN_H) return -1;

    /* 1) Fixed kiri: indices 0..TOOLBAR_FIXED_LEFT_COUNT-1 */
    for (int i = 0; i < TOOLBAR_FIXED_LEFT_COUNT; i++) {
        int bx = 8 + i * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP);
        if (x >= bx && x <= bx + TOOLBAR_BTN_W) return i;
    }

    /* 2) Scrollable: btnTools[4..BTN_COUNT-1] */
    int sx_start = toolbar_scroll_start_x();
    int sx_end   = toolbar_scroll_end_x(W);
    if (x >= sx_start && x <= sx_end) {
        int scroll_x = toolbar_get_scroll_x(W);
        for (int i = TOOLBAR_FIXED_LEFT_COUNT; i < BTN_COUNT; i++) {
            int comp_pos = i - TOOLBAR_FIXED_LEFT_COUNT;
            int bx = sx_start + comp_pos * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP) - scroll_x;
            if (x >= bx && x <= bx + TOOLBAR_BTN_W) return i;
        }
    }

    /* 3) Fixed kanan: actions (indices BTN_COUNT..BTN_COUNT+3) */
    for (int i = 0; i < TOOLBAR_FIXED_RIGHT_COUNT; i++) {
        int bx = toolbar_action_x(i, W);
        if (x >= bx && x <= bx + TOOLBAR_BTN_W) {
            if (is_clear && i == 0) *is_clear = 1;
            return BTN_COUNT + i;
        }
    }
    return -1;
}

/* ===================== Wheel handler ===================== */

int toolbar_scroll_wheel(int delta, int x, int y, int W) {
    /* Terima seluruh tinggi toolbar (menubar sudah di-offset di caller). */
    if (y < 0 || y >= PALETTE_H) return 0;

    int sx_start = toolbar_scroll_start_x();
    int sx_end   = toolbar_scroll_end_x(W);
    if (x < sx_start || x > sx_end) return 0;

    int max = toolbar_max_scroll(W);
    if (max <= 0) return 0;

    int step = TOOLBAR_BTN_W + TOOLBAR_BTN_GAP;
    if (delta > 0)      g_toolbar_scroll_x -= step;
    else if (delta < 0) g_toolbar_scroll_x += step;

    if (g_toolbar_scroll_x < 0)   g_toolbar_scroll_x = 0;
    if (g_toolbar_scroll_x > max) g_toolbar_scroll_x = max;
    return 1;
}

/* ===================== Drawing helpers ===================== */

/* Btn untuk semua ToolKind (fixed kiri + scrollable). */
static void draw_one_tool_button(HDC hdc, ToolKind tool, int bx, int by, BOOL active) {
    HBRUSH bb  = CreateSolidBrush(active ? RGB(220,235,255) : RGB(255,255,255));
    HBRUSH obb = (HBRUSH)SelectObject(hdc, bb);
    HPEN bp    = CreatePen(PS_SOLID, active ? 2 : 1,
                           active ? RGB(50,128,255) : RGB(190,190,190));
    HPEN obp   = (HPEN)SelectObject(hdc, bp);
    RoundRect(hdc, bx, by, bx + TOOLBAR_BTN_W, by + TOOLBAR_BTN_H, 8, 8);
    SelectObject(hdc, obp);
    SelectObject(hdc, obb);
    DeleteObject(bp);
    DeleteObject(bb);

    if (toolbar_tool_is_component(tool)) {
        toolbar_draw_symbol(hdc, tool, bx + TOOLBAR_BTN_W / 2, by + 17, active);
        SetTextColor(hdc, active ? RGB(30,90,200) : RGB(90,90,90));
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, bx + 19, by + 30, toolbar_tool_shortname(tool),
                 (int)wcslen(toolbar_tool_shortname(tool)));
    } else {
        toolbar_draw_symbol(hdc, tool, bx + TOOLBAR_BTN_W / 2, by + 15, active);
        const wchar_t *nm = toolbar_tool_shortname(tool);
        int offx = (tool == TOOL_SELECT) ? 13 :
                   (tool == TOOL_WIRE)   ? 17 :
                   (tool == TOOL_BOX)    ? 16 : 14;
        SetTextColor(hdc, active ? RGB(30,90,200) : RGB(90,90,90));
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, bx + offx, by + 30, nm, (int)wcslen(nm));
    }
}


/* ===================== Scrollbar (interaksi) ===================== */

/* Geometri konsisten dengan draw_toolbar_scrollbar(). */
#define SB_Y_TOP    (PALETTE_H - 6)
#define SB_H        4
#define SB_HIT_PAD  4   /* toleransi hit-test di atas/bawah bar */

static void sb_metrics(int W, int *sx_start, int *bar_w,
                       int *thumb_w, int *max) {
    *sx_start = toolbar_scroll_start_x();
    int sx_end = toolbar_scroll_end_x(W);
    *bar_w     = sx_end - *sx_start;
    *max       = toolbar_max_scroll(W);
    if (*max <= 0 || *bar_w <= 0) { *thumb_w = 0; return; }
    int content_w = *bar_w + *max;
    *thumb_w = (*bar_w * *bar_w) / content_w;
    if (*thumb_w < 24)         *thumb_w = 24;
    if (*thumb_w > *bar_w)     *thumb_w = *bar_w;
}

int toolbar_scrollbar_hit(int x, int y, int W, int *is_thumb) {
    if (is_thumb) *is_thumb = 0;
    int sx_start, bar_w, thumb_w, max;
    sb_metrics(W, &sx_start, &bar_w, &thumb_w, &max);
    if (max <= 0 || thumb_w <= 0) return 0;

    int sx_end = sx_start + bar_w;
    if (x < sx_start || x > sx_end) return 0;
    if (y < SB_Y_TOP - SB_HIT_PAD) return 0;
    if (y > SB_Y_TOP + SB_H + SB_HIT_PAD) return 0;

    int scroll_x = toolbar_get_scroll_x(W);
    int track_len = bar_w - thumb_w;
    int thumb_x = sx_start;
    if (track_len > 0) thumb_x += (scroll_x * track_len) / max;

    if (x >= thumb_x && x <= thumb_x + thumb_w) {
        if (is_thumb) *is_thumb = 1;
        return 2;
    }
    return 1;
}

void toolbar_scrollbar_begin_drag(int x, int y, int W) {
    int is_thumb = 0;
    int hit = toolbar_scrollbar_hit(x, y, W, &is_thumb);
    if (hit == 0) return;

    if (hit == 2) {
        /* Drag thumb: rekam posisi awal. */
        sb_drag_active       = 1;
        sb_drag_start_mx     = x;
        sb_drag_start_scroll = toolbar_get_scroll_x(W);
    } else {
        /* Klik track: lompat sehingga kursor = tengah thumb. */
        int sx_start, bar_w, thumb_w, max;
        sb_metrics(W, &sx_start, &bar_w, &thumb_w, &max);
        if (max <= 0 || bar_w <= 0) return;
        int track_len = bar_w - thumb_w;
        if (track_len <= 0) return;
        int new_scroll = (x - sx_start - thumb_w / 2) * max / track_len;
        if (new_scroll < 0)   new_scroll = 0;
        if (new_scroll > max) new_scroll = max;
        g_toolbar_scroll_x = new_scroll;

        /* Lanjut sebagai drag (UX standar scrollbar). */
        sb_drag_active       = 1;
        sb_drag_start_mx     = x;
        sb_drag_start_scroll = g_toolbar_scroll_x;
    }
}

int toolbar_scrollbar_drag(int mx, int W) {
    if (!sb_drag_active) return 0;
    int sx_start, bar_w, thumb_w, max;
    sb_metrics(W, &sx_start, &bar_w, &thumb_w, &max);
    if (max <= 0 || thumb_w <= 0) { sb_drag_active = 0; return 1; }

    int track_len = bar_w - thumb_w;
    if (track_len <= 0) { sb_drag_active = 0; return 1; }

    int dmx = mx - sb_drag_start_mx;
    int new_scroll = sb_drag_start_scroll + (dmx * max) / track_len;
    if (new_scroll < 0)   new_scroll = 0;
    if (new_scroll > max) new_scroll = max;
    g_toolbar_scroll_x = new_scroll;
    return 1;
}

void toolbar_scrollbar_end_drag(void) { sb_drag_active = 0; }
int  toolbar_scrollbar_dragging(void) { return sb_drag_active; }

int toolbar_scrollbar_hover_thumb(int x, int y, int W) {
    int is_thumb = 0;
    return toolbar_scrollbar_hit(x, y, W, &is_thumb) == 2;
}

/* Scrollbar indicator di bawah scroll region. */
static void draw_toolbar_scrollbar(HDC hdc, int sx_start, int sx_end,
                                   int scroll_x, int max_scroll) {
    int bar_y = PALETTE_H - 6;
    int bar_h = 4;
    int bar_w = sx_end - sx_start;
    if (bar_w <= 0) return;

    /* Track */
    RECT track = { sx_start, bar_y, sx_start + bar_w, bar_y + bar_h };
    HBRUSH tb = CreateSolidBrush(RGB(220, 220, 220));
    FillRect(hdc, &track, tb);
    DeleteObject(tb);

    /* Thumb */
    int content_w = bar_w + max_scroll;
    int thumb_w   = (bar_w * bar_w) / content_w;
    if (thumb_w < 24) thumb_w = 24;
    if (thumb_w > bar_w) thumb_w = bar_w;
    int thumb_x = sx_start + (scroll_x * (bar_w - thumb_w)) / max_scroll;

    RECT thumb = { thumb_x, bar_y, thumb_x + thumb_w, bar_y + bar_h };
    HBRUSH tb2 = CreateSolidBrush(RGB(120, 150, 200));
    FillRect(hdc, &thumb, tb2);
    DeleteObject(tb2);
}

/* ===================== Main draw ===================== */

void draw_toolbar(HDC hdc, int W) {
    draw_menubar(hdc, W);

    int saved = SaveDC(hdc);
    OffsetViewportOrgEx(hdc, 0, MENUBAR_H, NULL);

    RECT bar = { 0, 0, W, PALETTE_H };
    HBRUSH bg = CreateSolidBrush(RGB(245,245,245));
    FillRect(hdc, &bar, bg);
    DeleteObject(bg);

    HPEN sep = CreatePen(PS_SOLID, 1, RGB(200,200,200));
    HPEN osp = (HPEN)SelectObject(hdc, sep);
    MoveToEx(hdc, 0, PALETTE_H - 1, NULL); LineTo(hdc, W, PALETTE_H - 1);
    SelectObject(hdc, osp);
    DeleteObject(sep);

    HFONT hF = font_btn_label();
    HFONT of = (HFONT)SelectObject(hdc, hF);
    SetBkMode(hdc, TRANSPARENT);

    int scroll_x = toolbar_get_scroll_x(W);
    int sx_start = toolbar_scroll_start_x();
    int sx_end   = toolbar_scroll_end_x(W);

    /* 1) Fixed kiri */
    for (int i = 0; i < TOOLBAR_FIXED_LEFT_COUNT; i++) {
        int bx = 8 + i * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP);
        ToolKind tool = btnTools[i];
        BOOL active = (tool == current_tool);
        draw_one_tool_button(hdc, tool, bx, 8, active);
    }

    /* 2) Scrollable komponen (klip ke scroll region) */
    if (sx_end > sx_start) {
        SaveDC(hdc);
        IntersectClipRect(hdc, sx_start, 0, sx_end, PALETTE_H);
        for (int i = TOOLBAR_FIXED_LEFT_COUNT; i < BTN_COUNT; i++) {
            int comp_pos = i - TOOLBAR_FIXED_LEFT_COUNT;
            int bx = sx_start + comp_pos * (TOOLBAR_BTN_W + TOOLBAR_BTN_GAP) - scroll_x;
            ToolKind tool = btnTools[i];
            BOOL active = (tool == current_tool);
            draw_one_tool_button(hdc, tool, bx, 8, active);
        }
        RestoreDC(hdc, -1);
    }

    /* 3) Fixed kanan (aksi) */
    for (int i = 0; i < TOOLBAR_FIXED_RIGHT_COUNT; i++) {
        int bx = toolbar_action_x(i, W);
        if      (i == 0) palette_draw_clear(hdc, bx, 8);
        else if (i == 1) palette_draw_save (hdc, bx, 8);
        else if (i == 2) palette_draw_open (hdc, bx, 8);
        else if (i == 3) palette_draw_run  (hdc, bx, 8);
    }

    /* 4) Scrollbar indicator (bila perlu) */
    int max_scroll = toolbar_max_scroll(W);
    if (max_scroll > 0) {
        draw_toolbar_scrollbar(hdc, sx_start, sx_end, scroll_x, max_scroll);
    }

    SelectObject(hdc, of);
    RestoreDC(hdc, saved);
}