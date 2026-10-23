/* ============================================================================
   src/ui/canvas/minimap.c — Panel overview canvas di pojok kanan-bawah.

   Panel 200×140 yang menampilkan miniatur seluruh schematic (komponen +
   kabel) + viewport merah. Klik/drag → pan canvas utama ke titik itu.
   Skala otomatis dari gabungan (bounds komponen ∪ viewport ∪ padding).

   Include:
     * "ui.h" — <windows.h>, model.h (comp_bounding_half, canvas transforms,
                draw_wires, draw_component, g_pan_x/y, g_zoom, g_show_minimap,
                g_minimap_mode, comp_count, comps[]).
                Semua deklarasi minimap_* ada di ui.h (Step 7).

   Catatan implementasi:
     * Minimap menggunakan SetWorldTransform untuk render ulang komponen
       dengan simbol aslinya (bukan thumbnail). g_minimap_mode=1 memberi
       sinyal ke draw_component untuk melewati label, kotak seleksi, dan
       titik pin — supaya tampilan mini tetap bersih.
     * Viewport merah digambar DI LUAR world transform (screen coords).
     * Jika canvas kosong & view default (zoom=100%, pan=0), minimap
       auto-hide supaya tidak mengganggu.
   ============================================================================ */
#include "ui.h"

#define MM_W        200
#define MM_H        140
#define MM_MARGIN   12
#define MM_PAD      6

typedef struct {
    int x0, y0, x1, y1;   /* world-space bounds of content */
    int sx, sy;           /* screen top-left of panel */
    double sc;            /* world -> minimap scale */
} MinimapGeo;

static int mm_drag_active = 0;

/* Hitung world bounds dari semua komponen + viewport saat ini + padding. */
static void mm_world_bounds(int canvas_w, int canvas_h,
                            int *x0, int *y0, int *x1, int *y1) {
    int minx, miny, maxx, maxy;

    if (comp_count == 0) {
        minx = g_pan_x;
        miny = g_pan_y;
        maxx = (int)(g_pan_x + canvas_w / g_zoom);
        maxy = (int)(g_pan_y + canvas_h / g_zoom);
    } else {
        minx =  1 << 30;  miny =  1 << 30;
        maxx = -1 << 30;  maxy = -1 << 30;
        for (int i = 0; i < comp_count; i++) {
            int hw, hh;
            comp_bounding_half(&comps[i], &hw, &hh);
            if (comps[i].x - hw < minx) minx = comps[i].x - hw;
            if (comps[i].y - hh < miny) miny = comps[i].y - hh;
            if (comps[i].x + hw > maxx) maxx = comps[i].x + hw;
            if (comps[i].y + hh > maxy) maxy = comps[i].y + hh;
        }
        int vx0 = g_pan_x;
        int vy0 = g_pan_y;
        int vx1 = (int)(g_pan_x + canvas_w / g_zoom);
        int vy1 = (int)(g_pan_y + canvas_h / g_zoom);
        if (vx0 < minx) minx = vx0;
        if (vy0 < miny) miny = vy0;
        if (vx1 > maxx) maxx = vx1;
        if (vy1 > maxy) maxy = vy1;
    }

    minx -= 40; miny -= 40;
    maxx += 40; maxy += 40;
    *x0 = minx; *y0 = miny; *x1 = maxx; *y1 = maxy;
}

static void mm_geo(int canvas_w, int canvas_h, MinimapGeo *g) {
    g->sx = canvas_w - MM_W - MM_MARGIN;
    g->sy = canvas_h - MM_H - MM_MARGIN;
    if (g->sx < 4) g->sx = 4;
    if (g->sy < 4) g->sy = 4;

    mm_world_bounds(canvas_w, canvas_h, &g->x0, &g->y0, &g->x1, &g->y1);

    int bw = g->x1 - g->x0; if (bw < 1) bw = 1;
    int bh = g->y1 - g->y0; if (bh < 1) bh = 1;
    double scx = (double)(MM_W - 2 * MM_PAD) / bw;
    double scy = (double)(MM_H - 2 * MM_PAD) / bh;
    g->sc = (scx < scy) ? scx : scy;
}

static void mm_w2s(MinimapGeo *g, int wx, int wy, int *sx, int *sy) {
    int bw = g->x1 - g->x0, bh = g->y1 - g->y0;
    int draw_w = (int)(bw * g->sc);
    int draw_h = (int)(bh * g->sc);
    int off_x = (MM_W - draw_w) / 2;
    int off_y = (MM_H - draw_h) / 2;
    *sx = g->sx + off_x + (int)((wx - g->x0) * g->sc);
    *sy = g->sy + off_y + (int)((wy - g->y0) * g->sc);
}

static void mm_s2w(MinimapGeo *g, int sx, int sy, int *wx, int *wy) {
    int bw = g->x1 - g->x0, bh = g->y1 - g->y0;
    int draw_w = (int)(bw * g->sc);
    int draw_h = (int)(bh * g->sc);
    int off_x = (MM_W - draw_w) / 2;
    int off_y = (MM_H - draw_h) / 2;
    *wx = g->x0 + (int)((sx - g->sx - off_x) / g->sc);
    *wy = g->y0 + (int)((sy - g->sy - off_y) / g->sc);
}

/* ============================================================================
   Render minimap. Dipanggil dari render_canvas_scene saat include_ui_overlay
   != 0. Auto-hide kalau canvas kosong & view default.
   ============================================================================ */
void minimap_render(HDC hdc, int canvas_w, int canvas_h) {
    if (!g_show_minimap) return;

    /* Auto-hide bila canvas kosong & view default. */
    if (comp_count == 0 && g_zoom == 1.0 && g_pan_x == 0 && g_pan_y == 0) return;

    MinimapGeo g;
    mm_geo(canvas_w, canvas_h, &g);

    /* 1) Panel background + border. */
    RECT panel = { g.sx, g.sy, g.sx + MM_W, g.sy + MM_H };
    HBRUSH bg = CreateSolidBrush(RGB(252, 252, 248));
    FillRect(hdc, &panel, bg);
    DeleteObject(bg);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(180, 195, 215));
    HPEN ob = (HPEN)SelectObject(hdc, border);
    HBRUSH obr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, g.sx, g.sy, g.sx + MM_W, g.sy + MM_H);
    SelectObject(hdc, obr);

    /* 2) Clip seluruh isi ke dalam panel (tepi dalam). */
    int saved_clip = SaveDC(hdc);
    IntersectClipRect(hdc, g.sx + 1, g.sy + 1, g.sx + MM_W - 1, g.sy + MM_H - 1);

    /* 3) Setup world transform: world -> minimap screen.
       Sama dengan formula di mm_w2s(), tetapi dikomposisikan lewat
       SetWorldTransform agar draw_component (dan semua draw_k_*) otomatis
       di-skala + digeser. with_rotation() sudah mem-parse transform parent
       lewat GetWorldTransform, jadi komponen berotasi juga benar. */
    int bw = g.x1 - g.x0;
    int bh = g.y1 - g.y0;
    int draw_w = (int)(bw * g.sc);
    int draw_h = (int)(bh * g.sc);
    int off_x = (MM_W - draw_w) / 2;
    int off_y = (MM_H - draw_h) / 2;

    int saved_tf = SaveDC(hdc);
    SetGraphicsMode(hdc, GM_ADVANCED);
    XFORM xf = {
        (FLOAT)g.sc, 0,
        0, (FLOAT)g.sc,
        (FLOAT)(g.sx + off_x - g.x0 * g.sc),
        (FLOAT)(g.sy + off_y - g.y0 * g.sc)
    };
    SetWorldTransform(hdc, &xf);

    /* 4) Render wires + komponen dengan simbol asli.
       g_minimap_mode = 1 => draw_component melewati label, kotak seleksi,
       dan titik pin. Panjang pens (2 px) di-scale menjadi ~0.2 px -> GDI
       render sebagai 1 px, cukup tipis untuk simbol mini. */
    g_minimap_mode = 1;
    draw_wires(hdc);
    for (int i = 0; i < comp_count; i++)
        draw_component(hdc, &comps[i]);
    g_minimap_mode = 0;

    RestoreDC(hdc, saved_tf);

    /* 5) Viewport rectangle (screen coords, DI LUAR world transform). */
    int vx0 = g_pan_x;
    int vy0 = g_pan_y;
    int vx1 = (int)(g_pan_x + canvas_w / g_zoom);
    int vy1 = (int)(g_pan_y + canvas_h / g_zoom);
    int svx0, svy0, svx1, svy1;
    mm_w2s(&g, vx0, vy0, &svx0, &svy0);
    mm_w2s(&g, vx1, vy1, &svx1, &svy1);
    if (svx1 - svx0 < 3) svx1 = svx0 + 3;
    if (svy1 - svy0 < 3) svy1 = svy0 + 3;

    HPEN vpen = CreatePen(PS_SOLID, 2, RGB(230, 60, 60));
    HPEN ovp = (HPEN)SelectObject(hdc, vpen);
    HBRUSH ovb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, svx0, svy0, svx1, svy1);
    SelectObject(hdc, ovb);
    SelectObject(hdc, ovp);
    DeleteObject(vpen);

    RestoreDC(hdc, saved_clip);
    SelectObject(hdc, ob);
    DeleteObject(border);
}

/* ============================================================================
   Hit-test & interaksi (klik / drag → pan canvas utama).
   ============================================================================ */
int minimap_hit(int x, int y, int canvas_w, int canvas_h) {
    if (!g_show_minimap) return 0;
    if (comp_count == 0 && g_zoom == 1.0 && g_pan_x == 0 && g_pan_y == 0) return 0;
    MinimapGeo g;
    mm_geo(canvas_w, canvas_h, &g);
    return (x >= g.sx && x < g.sx + MM_W && y >= g.sy && y < g.sy + MM_H);
}

static void mm_center_on_world(int wx, int wy, int canvas_w, int canvas_h) {
    g_pan_x = wx - (int)(canvas_w / 2.0 / g_zoom);
    g_pan_y = wy - (int)(canvas_h / 2.0 / g_zoom);
}

void minimap_click(int x, int y, int canvas_w, int canvas_h) {
    MinimapGeo g;
    mm_geo(canvas_w, canvas_h, &g);
    int wx, wy;
    mm_s2w(&g, x, y, &wx, &wy);
    mm_center_on_world(wx, wy, canvas_w, canvas_h);
}

void minimap_begin_drag(int x, int y, int canvas_w, int canvas_h) {
    (void)x; (void)y; (void)canvas_w; (void)canvas_h;
    mm_drag_active = 1;
}

void minimap_drag(int x, int y, int canvas_w, int canvas_h) {
    if (!mm_drag_active) return;
    minimap_click(x, y, canvas_w, canvas_h);
}

void minimap_end_drag(void)  { mm_drag_active = 0; }
int  minimap_dragging(void)  { return mm_drag_active; }