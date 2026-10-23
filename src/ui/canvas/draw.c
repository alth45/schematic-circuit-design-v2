/* ============================================================================
   src/ui/canvas/draw.c — Dispatcher gambar simbol + render scene lengkap.

   File ini adalah inti layer "render" UI:
     * Dispatcher per-komponen (draw_component) + rotasi (with_rotation)
     * Overlay seleksi & marquee
     * Kabel + junction dot + preview wiring
     * render_canvas_scene() — ORKESTRATOR yang dipanggil dari WM_PAINT
       (main.c) dan export_canvas_png (io/export_png.c).

   Fungsi draw_canvas(HWND) lama SUDAH DIHAPUS — itu dead code; satu-satunya
   jalur render adalah render_canvas_scene(HDC, W, H, include_ui_overlay).

   Include:
     * "ui.h"  — <windows.h>, <math.h>, <stdio.h>, <string.h>, <wchar.h>,
                 model.h, sim.h (semuanya via ui.h).
   ============================================================================ */
#include "ui.h"

/* ============================================================================
   Font cache (umur = umur proses; tidak di-DeleteObject)
   ============================================================================ */
static HFONT font_value_label(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    return h;
}
static HFONT font_sel_label(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    return h;
}
/* font_banner dipindah dari main.c — hanya dipakai render_canvas_scene. */
static HFONT font_banner(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0,
                            L"Segoe UI");
    return h;
}

/* ============================================================================
   Forward declarations
   ============================================================================ */
static int comp_in_selection(int id);
static void draw_selection_boxes(HDC hdc);
static void draw_marquee(HDC hdc);

/* ============================================================================
   with_rotation — bungkus fn() dengan transform rotasi komponen.

   Komposisi: LOCAL -(rot+translate c)-> WORLD -(prev)-> SCREEN.

   PENTING (Win32 GDI):
   - SetWorldTransform MENIMPA transform, bukan mengomposisi. Jadi harus
     dikomposisi manual dengan transform parent (zoom/pan).
   - SaveDC/RestoreDC TIDAK menyimpan/mengembalikan world transform.
     Jadi transform parent harus disimpan via GetWorldTransform dan
     dipulihkan via SetWorldTransform setelah selesai.
   ============================================================================ */
void with_rotation(HDC hdc, Component *c,
                   void (*fn)(HDC, int, int, BOOL)) {
    int r = ((c->rot % 4) + 4) % 4;
    if (r == 0) { fn(hdc, c->x, c->y, c->id == selected_id); return; }

    /* 1) Simpan transform parent (zoom/pan). */
    int old_mode = GetGraphicsMode(hdc);
    XFORM prev;
    prev.eM11 = 1; prev.eM12 = 0; prev.eM21 = 0; prev.eM22 = 1;
    prev.eDx = 0;  prev.eDy = 0;
    if (old_mode == GM_ADVANCED) GetWorldTransform(hdc, &prev);

    int saved = SaveDC(hdc);
    SetGraphicsMode(hdc, GM_ADVANCED);

    /* 2) Matriks rotasi (screen coords, Y ke bawah):
       r=1: (x,y)->(-y, x)   -> r11=0,  r12=1,  r21=-1, r22=0
       r=2: (x,y)->(-x,-y)   -> r11=-1, r12=0,  r21=0,  r22=-1
       r=3: (x,y)->(y, -x)   -> r11=0,  r12=-1, r21=1,  r22=0 */
    double r11 = 1, r12 = 0, r21 = 0, r22 = 1;
    if (r == 1)      { r11 = 0;  r12 = 1;  r21 = -1; r22 = 0; }
    else if (r == 2) { r11 = -1; r12 = 0;  r21 = 0;  r22 = -1; }
    else             { r11 = 0;  r12 = -1; r21 = 1;  r22 = 0; }

    /* 3) Komposisi P ∘ (R + translate(c)):
       screen = (P·R)·local + P·(c) + P.offset */
    double p11 = prev.eM11, p12 = prev.eM12;
    double p21 = prev.eM21, p22 = prev.eM22;
    double pdx = prev.eDx,  pdy = prev.eDy;

    XFORM xf;
    xf.eM11 = (FLOAT)(r11 * p11 + r12 * p21);
    xf.eM12 = (FLOAT)(r11 * p12 + r12 * p22);
    xf.eM21 = (FLOAT)(r21 * p11 + r22 * p21);
    xf.eM22 = (FLOAT)(r21 * p12 + r22 * p22);
    xf.eDx  = (FLOAT)(c->x * p11 + c->y * p21 + pdx);
    xf.eDy  = (FLOAT)(c->x * p12 + c->y * p22 + pdy);

    SetWorldTransform(hdc, &xf);
    fn(hdc, 0, 0, c->id == selected_id);

    /* 4) RestoreDC mengembalikan graphics mode (dan me-reset transform
       bila target mode = GM_COMPATIBLE). Tapi bila prev ada di
       GM_ADVANCED, transform parent tetap perlu dipulihkan manual. */
    RestoreDC(hdc, saved);
    if (old_mode == GM_ADVANCED) SetWorldTransform(hdc, &prev);
}

/* ============================================================================
   Helper: pin, value label, selection overlay, marquee, stroke ortho
   ============================================================================ */
static void draw_pins(HDC hdc, Component *c) {
    int n = comp_pin_count(c);
    int show = (current_tool == TOOL_WIRE || wiring_active ||
                c->id == selected_id || comp_in_selection(c->id));
    for (int p = 0; p < n; p++) {
        int px, py;
        comp_pin_pos(c, p, &px, &py);
        HBRUSH br = CreateSolidBrush(show ? RGB(220, 60, 60) : RGB(150,150,150));
        HBRUSH oldb = (HBRUSH)SelectObject(hdc, br);
        HPEN pen = CreatePen(PS_SOLID, 1, show ? RGB(160,20,20) : RGB(120,120,120));
        HPEN oldp = (HPEN)SelectObject(hdc, pen);
        Ellipse(hdc, px - PIN_R, py - PIN_R, px + PIN_R, py + PIN_R);
        SelectObject(hdc, oldp);
        SelectObject(hdc, oldb);
        DeleteObject(pen);
        DeleteObject(br);
    }
}

static void draw_value_label(HDC hdc, Component *c) {
    if (!comp_has_value(c->type)) return;

    const wchar_t *unit = comp_value_unit(c->type);
    wchar_t vs[VALUE_STR_CAP];
    if (c->type == COMP_POT) {
        wchar_t base[32];
        format_value(c->v, L"\x03A9", base, 32);
        swprintf(vs, VALUE_STR_CAP, L"%ls @%d%%", base, c->on);
    } else {
        format_value(c->v, unit, vs, VALUE_STR_CAP);
    }

    HFONT hf = font_value_label();
    HFONT of = (HFONT)SelectObject(hdc, hf);
    SetBkMode(hdc, TRANSPARENT);

    SIZE sz; GetTextExtentPoint32W(hdc, vs, (int)wcslen(vs), &sz);
    int tw = sz.cx, th = sz.cy;

    int tx = c->x - tw / 2;
    int ty = (c->rot % 2 == 0) ? c->y - 34 : c->y - th / 2;
    if (c->rot % 2 != 0) {
        if ((c->rot % 4) == 1) tx = c->x - tw - 52;
        else                   tx = c->x + 52;
    }

    HPEN bp = CreatePen(PS_SOLID, 1, RGB(220,220,220));
    HPEN obp = (HPEN)SelectObject(hdc, bp);
    HBRUSH bb = CreateSolidBrush(RGB(255,255,255));
    HBRUSH obb = (HBRUSH)SelectObject(hdc, bb);
    Rectangle(hdc, tx - 4, ty - 2, tx + tw + 4, ty + th + 2);
    SelectObject(hdc, obb); SelectObject(hdc, obp);
    DeleteObject(bb); DeleteObject(bp);

    COLORREF col = RGB(70, 70, 70);
    if (sim_running && c->type == COMP_LED && sim_comp_blown(c))
        col = RGB(200, 30, 30);
    SetTextColor(hdc, col);
    TextOutW(hdc, tx, ty, vs, (int)wcslen(vs));

    SelectObject(hdc, of);
    /* font di-cache: JANGAN DeleteObject(hf) */
}

/* ============================================================================
   draw_component — dispatcher simbol + label + outline + pin
   ============================================================================ */
void draw_component(HDC hdc, Component *c) {
    BOOL sel = (c->id == selected_id);
    switch (c->type) {
        case COMP_RESISTOR:    draw_k_resistor(hdc, c);    break;
        case COMP_SWITCH:      draw_k_switch(hdc, c);      break;
        case COMP_BATTERY:     draw_k_battery(hdc, c);     break;
        case COMP_ACSOURCE:    draw_k_acsource(hdc, c);    break;
        case COMP_LED:         draw_k_led(hdc, c);         break;
        case COMP_GROUND:      draw_k_ground(hdc, c);      break;
        case COMP_CAPACITOR:   draw_k_capacitor(hdc, c);   break;
        case COMP_TRANSISTOR:  draw_k_transistor(hdc, c);  break;
        case COMP_DIODE:       draw_k_diode(hdc, c);       break;
        case COMP_CAPOLAR:     draw_k_capolar(hdc, c);     break;
        case COMP_LDR:         draw_k_ldr(hdc, c);         break;
        case COMP_BUZZER:      draw_k_buzzer(hdc, c);      break;
        case COMP_VOLTMETER:   draw_k_voltmeter(hdc, c);   break;
        case COMP_AMMETER:     draw_k_ammeter(hdc, c);     break;
        case COMP_OSCPROBE:    draw_k_oscprobe(hdc, c);    break;
        case COMP_INDUCTOR:    draw_k_inductor(hdc, c);    break;
        case COMP_ZENER:       draw_k_zener(hdc, c);       break;
        case COMP_MOSFET:      draw_k_mosfet(hdc, c);      break;
        case COMP_POT:         draw_k_pot(hdc, c);         break;
        case COMP_AND:         draw_k_and(hdc, c);         break;
        case COMP_OR:          draw_k_or(hdc, c);          break;
        case COMP_NOT:         draw_k_not(hdc, c);         break;
        case COMP_NAND:        draw_k_nand(hdc, c);        break;
        case COMP_NOR:         draw_k_nor(hdc, c);         break;
        case COMP_XOR:         draw_k_xor(hdc, c);         break;
        default: break;
    }
    if (!g_minimap_mode) {
        draw_value_label(hdc, c);
        if (sel) {
            HPEN pen = CreatePen(PS_DOT, 1, RGB(50,128,255));
            HPEN old = (HPEN)SelectObject(hdc, pen);
            HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            int hw = (c->rot % 2 == 0) ? 48 : 38;
            int hh = (c->rot % 2 == 0) ? 38 : 48;
            if (c->type == COMP_TRANSISTOR || c->type == COMP_MOSFET || c->type == COMP_POT) { hw = 52; hh = 42; }
            Rectangle(hdc, c->x - hw, c->y - hh, c->x + hw, c->y + hh);
            SelectObject(hdc, oldb);
            SelectObject(hdc, old);
            DeleteObject(pen);
            wchar_t buf[16];
            swprintf(buf, 16, L"%d deg", (c->rot % 4) * 90);
            SetTextColor(hdc, RGB(50,128,255));
            SetBkMode(hdc, TRANSPARENT);
            TextOutW(hdc, c->x - 24, c->y - hh - 18, buf, (int)wcslen(buf));
        }
        draw_pins(hdc, c);
    }
}

/* ============================================================================
   Overlay: seleksi + marquee
   ============================================================================ */
void draw_selection_overlay(HDC hdc) {
    draw_selection_boxes(hdc);
}

void draw_marquee_overlay(HDC hdc){
    if(box_selecting) draw_marquee(hdc);
}

static int comp_in_selection(int id) {
    for (int i = 0; i < sel_count; i++)
        if (selection[i] == id) return 1;
    return 0;
}

static void draw_selection_boxes(HDC hdc) {
    if (sel_count <= 1) return;
    int minx = 1 << 30, miny = 1 << 30, maxx = -(1 << 30), maxy = -(1 << 30);
    HPEN pen = CreatePen(PS_DOT, 1, RGB(0, 160, 90));
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    for (int i = 0; i < sel_count; i++) {
        Component *c = find_comp_by_id(selection[i]);
        if (!c) continue;
        int hw, hh;
        comp_bounding_half(c, &hw, &hh);
        Rectangle(hdc, c->x - hw, c->y - hh, c->x + hw, c->y + hh);
        if (c->x - hw < minx) minx = c->x - hw;
        if (c->y - hh < miny) miny = c->y - hh;
        if (c->x + hw > maxx) maxx = c->x + hw;
        if (c->y + hh > maxy) maxy = c->y + hh;
    }
    SelectObject(hdc, ob);
    SelectObject(hdc, oldp);
    DeleteObject(pen);
    if (minx <= maxx) {
        HPEN gp = CreatePen(PS_SOLID, 1, RGB(0, 160, 90));
        HPEN ogp = (HPEN)SelectObject(hdc, gp);
        HBRUSH gb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, minx - 6, miny - 6, maxx + 6, maxy + 6);
        SelectObject(hdc, gb);
        SelectObject(hdc, ogp);
        DeleteObject(gp);
        wchar_t buf[32];
        swprintf(buf, 32, L"%d komponen (blok)", sel_count);
        HFONT hf = font_sel_label();
        HFONT ofh = (HFONT)SelectObject(hdc, hf);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 130, 75));
        TextOutW(hdc, minx - 4, miny - 24, buf, (int)wcslen(buf));
        SelectObject(hdc, ofh);
        /* font di-cache: JANGAN DeleteObject(hf) */
    }
}

static void draw_marquee(HDC hdc) {
    if (!box_selecting) return;

    int cx1 = (box_x1 < box_x2) ? box_x1 : box_x2;
    int cy1 = (box_y1 < box_y2) ? box_y1 : box_y2;
    int cx2 = (box_x1 < box_x2) ? box_x2 : box_x1;
    int cy2 = (box_y1 < box_y2) ? box_y2 : box_y1;

    int slx, sty, srx, sby;
    canvas_canvas_to_screen(cx1, cy1, &slx, &sty);
    canvas_canvas_to_screen(cx2, cy2, &srx, &sby);

    if (srx > slx && sby > sty) {
        HDC mdc = CreateCompatibleDC(hdc);
        HBITMAP mono = CreateBitmap(srx - slx, sby - sty, 1, 1, NULL);
        HBITMAP omono = (HBITMAP)SelectObject(mdc, mono);
        PatBlt(mdc, 0, 0, srx - slx, sby - sty, BLACKNESS);
        BitBlt(hdc, slx, sty, srx - slx, sby - sty, mdc, 0, 0, MERGEPAINT);
        SelectObject(mdc, omono);
        DeleteObject(mono);
        DeleteDC(mdc);
    }

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(50, 128, 255));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, slx, sty, srx, sby);
    SelectObject(hdc, ob);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

/* ============================================================================
   Wires + junction dot + preview wiring
   ============================================================================ */
static void stroke_ortho(HDC hdc, int x1, int y1, int x2, int y2) {
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y1);
    LineTo(hdc, x2, y2);
}

void draw_wires(HDC hdc) {
    for (int i = 0; i < wire_count; i++) {
        int ax, ay, bx, by;
        pinref_pos(wires[i].a, &ax, &ay);
        pinref_pos(wires[i].b, &bx, &by);
        int hot = sim_running && sim_wire_hot(i);
        HPEN pen = CreatePen(PS_SOLID, hot ? 3 : 2,
                             hot ? RGB(230,50,50) : RGB(20,20,20));
        HPEN old = (HPEN)SelectObject(hdc, pen);
        stroke_ortho(hdc, ax, ay, bx, by);

        /* Endpoint dot — hanya kalau endpoint adalah PIN komponen. */
        HBRUSH dot = CreateSolidBrush(hot ? RGB(230,50,50) : RGB(20,20,20));
        HBRUSH od = (HBRUSH)SelectObject(hdc, dot);
        if (wires[i].a.comp_id >= 0) Ellipse(hdc, ax - 3, ay - 3, ax + 3, ay + 3);
        if (wires[i].b.comp_id >= 0) Ellipse(hdc, bx - 3, by - 3, bx + 3, by + 3);
        SelectObject(hdc, od);
        DeleteObject(dot);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }

    /* Junction dots — digambar SETELAH wire supaya tampil di atas. */
    for (int j = 0; j < MAX_JUNCTIONS; j++) {
        if (!junctions[j].used) continue;
        int jx = junctions[j].x, jy = junctions[j].y;

        int hot = 0;
        if (sim_running) {
            for (int i = 0; i < wire_count && !hot; i++) {
                if ((wires[i].a.comp_id == -1 && wires[i].a.pin_idx == j) ||
                    (wires[i].b.comp_id == -1 && wires[i].b.pin_idx == j)) {
                    if (sim_wire_hot(i)) hot = 1;
                }
            }
        }

        COLORREF jc = hot ? RGB(230, 50, 50) : RGB(20, 20, 20);
        HBRUSH jb = CreateSolidBrush(jc);
        HBRUSH ojb = (HBRUSH)SelectObject(hdc, jb);
        HPEN jp = CreatePen(PS_SOLID, 1, jc);
        HPEN ojp = (HPEN)SelectObject(hdc, jp);
        Ellipse(hdc, jx - 4, jy - 4, jx + 4, jy + 4);
        SelectObject(hdc, ojp);
        SelectObject(hdc, ojb);
        DeleteObject(jp);
        DeleteObject(jb);
    }

    /* Preview wiring aktif. */
    if (wiring_active) {
        int ax, ay;
        pinref_pos(wiring_from, &ax, &ay);
        HPEN pv = CreatePen(PS_DOT, 2, RGB(220, 60, 60));
        HPEN opv = (HPEN)SelectObject(hdc, pv);
        int mx = snap(wiring_mx), my = snap(wiring_my);
        stroke_ortho(hdc, ax, ay, mx, my);
        SelectObject(hdc, opv);
        DeleteObject(pv);
        PinRef t; int tx, ty;
        if (find_pin_at(wiring_mx, wiring_my, &t, &tx, &ty)) {
            HBRUSH br = CreateSolidBrush(RGB(60, 180, 60));
            HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
            Ellipse(hdc, tx - 10, ty - 10, tx + 10, ty + 10);
            SelectObject(hdc, ob);
            DeleteObject(br);
        }
    }
}

/* ============================================================================
   Marker short (dipindah dari main.c) — dipanggil render_canvas_scene
   saat RUN & sim_short aktif.
   ============================================================================ */
static void draw_short_marker(HDC hdc, int W, int H) {
    if (!sim_short || sim_short_x < 0) return;

    int sx, sy;
    canvas_canvas_to_screen(sim_short_x, sim_short_y, &sx, &sy);

    /* Hanya hide kalau benar-benar jauh di luar viewport. */
    if (sx < -200 || sx > W + 200 || sy < -200 || sy > H + 200) return;

    COLORREF red = RGB(220, 40, 40);

    /* 1) Lingkaran luar (2 lapis: tebal + tipis). */
    HPEN outer = CreatePen(PS_SOLID, 3, red);
    HPEN oldp = (HPEN)SelectObject(hdc, outer);
    HBRUSH oldb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, sx - 18, sy - 18, sx + 18, sy + 18);

    HPEN inner = CreatePen(PS_SOLID, 1, red);
    SelectObject(hdc, inner);
    Ellipse(hdc, sx - 12, sy - 12, sx + 12, sy + 12);

    /* 2) Titik merah pusat (solid). */
    HBRUSH dot = CreateSolidBrush(red);
    HBRUSH od = (HBRUSH)SelectObject(hdc, dot);
    HPEN dotp = CreatePen(PS_SOLID, 1, red);
    HPEN odp = (HPEN)SelectObject(hdc, dotp);
    Ellipse(hdc, sx - 5, sy - 5, sx + 5, sy + 5);
    SelectObject(hdc, odp);
    SelectObject(hdc, od);
    DeleteObject(dotp);
    DeleteObject(dot);

    /* 3) Panah: dari atas (default) atau dari bawah (bila sy < 80). */
    int arrow_above = (sy >= 80);
    int tail_y, tip_y;
    if (arrow_above) { tail_y = sy - 55; tip_y = sy - 22; }
    else             { tail_y = sy + 55; tip_y = sy + 22; }

    HPEN arrow_pen = CreatePen(PS_SOLID, 3, red);
    SelectObject(hdc, arrow_pen);
    MoveToEx(hdc, sx, tail_y, NULL);
    LineTo(hdc, sx, tip_y);

    POINT tri[3];
    if (arrow_above) {
        tri[0] = (POINT){ sx,     tip_y + 6 };
        tri[1] = (POINT){ sx - 9, tip_y - 8 };
        tri[2] = (POINT){ sx + 9, tip_y - 8 };
    } else {
        tri[0] = (POINT){ sx,     tip_y - 6 };
        tri[1] = (POINT){ sx - 9, tip_y + 8 };
        tri[2] = (POINT){ sx + 9, tip_y + 8 };
    }
    HBRUSH ab = CreateSolidBrush(red);
    HBRUSH oab = (HBRUSH)SelectObject(hdc, ab);
    Polygon(hdc, tri, 3);
    SelectObject(hdc, oab);
    DeleteObject(ab);

    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(outer);
    DeleteObject(inner);
    DeleteObject(arrow_pen);

    /* 4) Label "SHORT!" di seberang panah. */
    HFONT hf = CreateFontW(14, 0, 0, 0, FW_BOLD, 0, 0, 0,
        0, 0, 0, 0, 0, L"Segoe UI");
    HFONT of = (HFONT)SelectObject(hdc, hf);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, red);
    const wchar_t *lbl = L"SHORT!";
    SIZE sz;
    GetTextExtentPoint32W(hdc, lbl, 6, &sz);
    int lx = sx - sz.cx / 2;
    int ly = arrow_above ? (tail_y - sz.cy - 6) : (tail_y + 6);
    TextOutW(hdc, lx, ly, lbl, 6);
    SelectObject(hdc, of);
    DeleteObject(hf);
}

/* ============================================================================
   render_canvas_scene — orkestrator render (dipindah dari main.c).

   Pipeline:
     1. Grid (screen coords, sebelum world transform)  → tetap tajam
     2. World transform (zoom + pan), lalu:
        - draw_wires
        - draw_component per komponen
        - draw_selection_overlay (hanya kalau include_ui_overlay)
     3. Marquee overlay (screen coords)
     4. Banner simulasi (kalau RUN)
     5. Short marker (kalau RUN & sim_short)
     6. Minimap (hanya kalau include_ui_overlay)

   Parameter include_ui_overlay=0 untuk export PNG: tanpa seleksi,
   marquee, dan minimap.
   ============================================================================ */
void render_canvas_scene(HDC hdc, int W, int H, int include_ui_overlay) {
    /* 1) Grid (screen coords, sebelum world transform → tetap tajam) */
    if (g_show_grid) {
        HPEN gp = CreatePen(PS_SOLID, 1, RGB(235, 235, 235));
        HPEN oldp = (HPEN)SelectObject(hdc, gp);
        int first_gx = (int)(ceil((double)g_pan_x / GRID) * GRID);
        int first_gy = (int)(ceil((double)g_pan_y / GRID) * GRID);
        for (int gx = first_gx; ; gx += GRID) {
            int sx = (int)((gx - g_pan_x) * g_zoom);
            if (sx >= W) break;
            if (sx >= 0) { MoveToEx(hdc, sx, 0, NULL); LineTo(hdc, sx, H); }
        }
        for (int gy = first_gy; ; gy += GRID) {
            int sy = (int)((gy - g_pan_y) * g_zoom);
            if (sy >= H) break;
            if (sy >= 0) { MoveToEx(hdc, 0, sy, NULL); LineTo(hdc, W, sy); }
        }
        SelectObject(hdc, oldp);
        DeleteObject(gp);
    }

    /* 2) Konten dengan world transform (zoom + pan) */
    int saved2 = SaveDC(hdc);
    SetGraphicsMode(hdc, GM_ADVANCED);
    XFORM xf = {
        (FLOAT)g_zoom, 0,
        0, (FLOAT)g_zoom,
        (FLOAT)(-g_pan_x * g_zoom),
        (FLOAT)(-g_pan_y * g_zoom)
    };
    SetWorldTransform(hdc, &xf);

    draw_wires(hdc);
    for (int i = 0; i < comp_count; i++)
        draw_component(hdc, &comps[i]);
    if (include_ui_overlay)
        draw_selection_overlay(hdc);

    RestoreDC(hdc, saved2);

    /* 3) Marquee overlay (screen coords; hanya live) */
    if (include_ui_overlay)
        draw_marquee_overlay(hdc);

    /* 4) Banner simulasi (kalau RUN) */
    if (sim_running) {
        const wchar_t *msg = sim_message();
        COLORREF fg, bgc;
        if (sim_short) {
            fg = RGB(255, 255, 255);
            bgc = RGB(200, 40, 40);
        } else if (sim_led_count > 0) {
            fg = RGB(255, 255, 255);
            bgc = RGB(20, 150, 60);
        } else {
            fg = RGB(60, 60, 60);
            bgc = RGB(255, 235, 160);
        }
        HFONT hB = font_banner();
        HFONT oB = (HFONT)SelectObject(hdc, hB);
        SIZE bsz;
        GetTextExtentPoint32W(hdc, msg, (int)wcslen(msg), &bsz);
        int bw = bsz.cx + 28, bh = bsz.cy + 14;
        int bx = (W - bw) / 2, by = 10;
        HBRUSH bb2 = CreateSolidBrush(bgc);
        HBRUSH obb2 = (HBRUSH)SelectObject(hdc, bb2);
        HPEN bp2 = CreatePen(PS_SOLID, 1, fg);
        HPEN obp2 = (HPEN)SelectObject(hdc, bp2);
        RoundRect(hdc, bx, by, bx + bw, by + bh, 10, 10);
        SelectObject(hdc, obp2);
        SelectObject(hdc, obb2);
        DeleteObject(bp2);
        DeleteObject(bb2);
        SetTextColor(hdc, fg);
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, bx + 14, by + 7, msg, (int)wcslen(msg));
        SelectObject(hdc, oB);
    }
    /* Penanda panah titik korsleting (bila RUN & short terdeteksi). */
    if (sim_running && sim_short) {
        draw_short_marker(hdc, W, H);
    }
    /* Minimap: hanya di live paint, tidak ikut export PNG. */
    if (include_ui_overlay)
        minimap_render(hdc, W, H);
}