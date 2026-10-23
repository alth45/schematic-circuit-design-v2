/* ============================================================================
   src/ui/canvas/viewport.c — Zoom berpusat di tengah viewport window.

   Dipindah dari src/core/model.c (Step 15 refactor). Alasan:
     * Butuh HWND + GetClientRect (Win32) → bukan ranah core.
     * Logika zoom itu sendiri (canvas_zoom_at) tetap di model.c karena
       murni matematika dan dipakai di banyak tempat.

   Fungsi lain yang masih terkait viewport tapi TETAP di core/model.c
   (murni matematika, tidak butuh HWND):
     canvas_screen_to_canvas, canvas_canvas_to_screen,
     canvas_zoom_at, canvas_zoom_fit, canvas_zoom_reset.
   ============================================================================ */
#include "ui.h"
#include "model.h"

void canvas_zoom_center(HWND hwnd, double factor) {
    if (!hwnd) return;
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right;
    int canvas_h = rc.bottom - TOOLBAR_H - (g_show_status ? STATUS_H : 0);
    if (canvas_h < 1) canvas_h = 1;
    canvas_zoom_at(W / 2, canvas_h / 2, factor);
}