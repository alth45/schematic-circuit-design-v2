/* ============================================================================
   src/ui/window.c — WndProc + window lifecycle (create + message dispatch).

   Isi file ini adalah pecahan dari main.c lama (Step 23 refactor):
     * font_status()              — font cache untuk status bar WM_PAINT
     * setup_keyboard()           — helper SetFocus (static)
     * toolbar_click()            — dispatch idx toolbar ke aksi (static)
     * g_pan_active + state vars  — pan dengan mouse tengah (static)
     * WndProc()                  — dispatch 12 pesan Win32
     * g_main_hwnd                — handle window utama (extern, lihat ui.h)
     * main_window_create()       — daftar class + buat window (hidden)

   Yang TIDAK di sini (sudah dipindah / tetap):
     * wWinMain()                 — tetap di src/main.c (Step 24)
     * render_canvas_scene()      — di src/ui/canvas/draw.c (Step 18)
     * font_banner()              — di src/ui/canvas/draw.c (Step 18)
     * draw_short_marker()        — di src/ui/canvas/draw.c (Step 18)

   Include:
     * "ui.h"     — <windows.h>, model.h, sim.h (semua deklarasi UI).
     * "audio.h"  — buzzer_audio_cleanup() dipanggil di WM_DESTROY.
   ============================================================================ */
#include "ui.h"
#include "audio.h"

/* ============================================================================
   Font status bar (static cache, umur = umur proses)
   ============================================================================ */
static HFONT font_status(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0,
                            L"Segoe UI");
    return h;
}

/* ============================================================================
   Helper internal
   ============================================================================ */
static void setup_keyboard(HWND hwnd) { SetFocus(hwnd); }

static void toolbar_click(HWND hwnd, int x, int y) {
  RECT rc; GetClientRect(hwnd, &rc);
  int W = rc.right;
  int idx = toolbar_hit(x, y, W);
  if (idx < 0) return;
  if (idx == CLEAR_BTN_INDEX) {
    menubar_command(hwnd, IDM_EDIT_CLEAR);
  } else if (idx == SAVE_BTN_INDEX) {
    model_save_dialog(hwnd);
  } else if (idx == OPEN_BTN_INDEX) {
    model_load_dialog(hwnd);
    sim_recompute();
  } else if (idx == RUN_BTN_INDEX) {
    sim_set_running(!sim_running);
  } else {
    if (sim_running) return;
    current_tool = btnTools[idx];
    dragging_id = -1;
    wiring_active = 0;
    setup_keyboard(hwnd);
  }
  InvalidateRect(hwnd, NULL, FALSE);
}

/* ============================================================================
   State pan (drag dengan mouse tengah)
   ============================================================================ */
static int g_pan_active   = 0;
static int g_pan_start_sx = 0, g_pan_start_sy = 0;
static int g_pan_orig_px  = 0, g_pan_orig_py  = 0;

/* ============================================================================
   Global handle window utama — lihat ui.h BAGIAN 10b.
   ============================================================================ */
HWND g_main_hwnd = NULL;

/* ============================================================================
   WndProc — dispatch WM_* untuk window utama.
   ============================================================================ */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_CREATE:
    setup_keyboard(hwnd);
    return 0;

  case WM_LBUTTONDOWN: {
    int x = (short)LOWORD(lp);
    int y = (short)HIWORD(lp);
    if (y < MENUBAR_H) { menubar_click(hwnd, x, y); return 0; }
    if (y < TOOLBAR_H) {
      RECT rc; GetClientRect(hwnd, &rc);
      int local_y = y - MENUBAR_H;
      int is_thumb = 0;
      if (toolbar_scrollbar_hit(x, local_y, rc.right, &is_thumb)) {
        toolbar_scrollbar_begin_drag(x, local_y, rc.right);
        SetCapture(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
      toolbar_click(hwnd, x, y - MENUBAR_H);
      return 0;
    }
    setup_keyboard(hwnd);
    /* Klik marker short → pan canvas agar titik short ter-center. */
    if (sim_running && sim_short) {
      RECT rc; GetClientRect(hwnd, &rc);
      int canvas_w = rc.right;
      int canvas_h = rc.bottom - TOOLBAR_H - (g_show_status ? STATUS_H : 0);
      if (short_marker_hit(x, y - TOOLBAR_H)) {
        short_marker_pan_to(canvas_w, canvas_h);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
    }
    {
      RECT rc; GetClientRect(hwnd, &rc);
      int canvas_w = rc.right;
      int canvas_h = rc.bottom - TOOLBAR_H - (g_show_status ? STATUS_H : 0);
      int cx_screen = x;
      int cy_screen = y - TOOLBAR_H;
      if (minimap_hit(cx_screen, cy_screen, canvas_w, canvas_h)) {
        minimap_begin_drag(cx_screen, cy_screen, canvas_w, canvas_h);
        minimap_drag(cx_screen, cy_screen, canvas_w, canvas_h);
        SetCapture(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
    }
    int cx, cy;
    canvas_screen_to_canvas(x, y - TOOLBAR_H, &cx, &cy);
    on_lbutton_down(hwnd, cx, cy);
    return 0;
  }

  case WM_RBUTTONDOWN: {
    int x = (short)LOWORD(lp);
    int y = (short)HIWORD(lp);
    if (y < TOOLBAR_H) {
      if (y < MENUBAR_H) menubar_click(hwnd, x, y);
      return 0;
    }
    int cx, cy;
    canvas_screen_to_canvas(x, y - TOOLBAR_H, &cx, &cy);
    on_rbutton_down(hwnd, cx, cy);
    return 0;
  }

  case WM_MOUSEMOVE: {
    int x = (short)LOWORD(lp);
    int y = (short)HIWORD(lp);

    if (toolbar_scrollbar_dragging()) {
      RECT rc; GetClientRect(hwnd, &rc);
      if (toolbar_scrollbar_drag(x, rc.right))
        InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }

    if (minimap_dragging()) {
      RECT rc; GetClientRect(hwnd, &rc);
      int canvas_w = rc.right;
      int canvas_h = rc.bottom - TOOLBAR_H - (g_show_status ? STATUS_H : 0);
      minimap_drag(x, y - TOOLBAR_H, canvas_w, canvas_h);
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }

    if (y >= MENUBAR_H && y < TOOLBAR_H) {
      RECT rc; GetClientRect(hwnd, &rc);
      int local_y = y - MENUBAR_H;
      if (toolbar_scrollbar_hover_thumb(x, local_y, rc.right)) {
        SetCursor(LoadCursor(NULL, IDC_SIZEWE));
      }
    }

    if (g_pan_active) {
      g_pan_x = g_pan_orig_px - (int)((x - g_pan_start_sx) / g_zoom);
      g_pan_y = g_pan_orig_py - (int)((y - g_pan_start_sy) / g_zoom);
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    if (y < TOOLBAR_H) y = TOOLBAR_H;
    int cx, cy;
    canvas_screen_to_canvas(x, y - TOOLBAR_H, &cx, &cy);
    on_mouse_move(hwnd, cx, cy);
    return 0;
  }

  case WM_LBUTTONUP: {
    if (toolbar_scrollbar_dragging()) {
      toolbar_scrollbar_end_drag();
      ReleaseCapture();
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    if (minimap_dragging()) {
      minimap_end_drag();
      ReleaseCapture();
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    int x = (short)LOWORD(lp);
    int y = (short)HIWORD(lp);
    if (y < TOOLBAR_H) y = TOOLBAR_H;
    int cx, cy;
    canvas_screen_to_canvas(x, y - TOOLBAR_H, &cx, &cy);
    on_lbutton_up(hwnd, cx, cy);
    return 0;
  }

  case WM_MBUTTONDOWN: {
    int x = (short)LOWORD(lp);
    int y = (short)HIWORD(lp);
    if (y < TOOLBAR_H) return 0;
    g_pan_active   = 1;
    g_pan_start_sx = x;
    g_pan_start_sy = y;
    g_pan_orig_px  = g_pan_x;
    g_pan_orig_py  = g_pan_y;
    SetCapture(hwnd);
    SetCursor(LoadCursor(NULL, IDC_SIZEALL));
    return 0;
  }

  case WM_MBUTTONUP:
    if (g_pan_active) {
      g_pan_active = 0;
      ReleaseCapture();
      SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
    return 0;

  case WM_MOUSEWHEEL: {
    int delta = (short)HIWORD(wp);
    POINT pt;
    pt.x = (short)LOWORD(lp);
    pt.y = (short)HIWORD(lp);
    ScreenToClient(hwnd, &pt);

    if (pt.y < MENUBAR_H) return 0;

    if (pt.y < TOOLBAR_H) {
      RECT rc; GetClientRect(hwnd, &rc);
      int local_y = pt.y - MENUBAR_H;
      if (toolbar_scroll_wheel(delta, pt.x, local_y, rc.right))
        InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }

    int sx = pt.x;
    int sy = pt.y - TOOLBAR_H;
    if (delta > 0)      canvas_zoom_at(sx, sy, 1.25);
    else if (delta < 0) canvas_zoom_at(sx, sy, 1.0 / 1.25);
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }

  case WM_LBUTTONDBLCLK: {
    int x = (short)LOWORD(lp);
    int y = (short)HIWORD(lp);
    if (y < MENUBAR_H) { menubar_click(hwnd, x, y); return 0; }
    if (y < TOOLBAR_H) return 0;
    int cx, cy;
    canvas_screen_to_canvas(x, y - TOOLBAR_H, &cx, &cy);
    on_lbutton_dblclk(hwnd, cx, cy);
    return 0;
  }

  case WM_RBUTTONDBLCLK: {
    int x = (short)LOWORD(lp);
    int y = (short)HIWORD(lp);
    if (y < TOOLBAR_H) return 0;
    int cx, cy;
    canvas_screen_to_canvas(x, y - TOOLBAR_H, &cx, &cy);
    on_rbutton_dblclk(hwnd, cx, cy);
    return 0;
  }

  case WM_KEYDOWN:
    on_key_down(hwnd, wp);
    return 0;

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, W, H);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &rc, bg);
    DeleteObject(bg);

    draw_toolbar(memDC, W);

    int canvas_h = H - TOOLBAR_H - (g_show_status ? STATUS_H : 0);
    if (canvas_h < 1) canvas_h = 1;

    SaveDC(memDC);
    SetViewportOrgEx(memDC, 0, TOOLBAR_H, NULL);
    IntersectClipRect(memDC, 0, 0, W, canvas_h);

    render_canvas_scene(memDC, W, canvas_h, 1);

    RestoreDC(memDC, -1);

    if (g_show_status) {
      RECT sr = {0, H - STATUS_H, W, H};
      HBRUSH sb = CreateSolidBrush(RGB(245, 245, 245));
      FillRect(memDC, &sr, sb);
      DeleteObject(sb);
      HPEN sp2 = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
      HPEN osp4 = (HPEN)SelectObject(memDC, sp2);
      MoveToEx(memDC, 0, H - STATUS_H, NULL);
      LineTo(memDC, W, H - STATUS_H);
      SelectObject(memDC, osp4);
      DeleteObject(sp2);
      HFONT hSt = font_status();
      HFONT oSt = (HFONT)SelectObject(memDC, hSt);
      SetTextColor(memDC, RGB(60, 60, 60));
      SetBkMode(memDC, TRANSPARENT);
      TextOutW(memDC, 10, H - STATUS_H + 5, g_status, (int)wcslen(g_status));
      wchar_t cnt[96];
      swprintf(cnt, 96, L"%d komp, %d kabel | zoom %.0f%%",
               comp_count, wire_count, g_zoom * 100.0);
      SIZE csz;
      GetTextExtentPoint32W(memDC, cnt, (int)wcslen(cnt), &csz);
      TextOutW(memDC, W - csz.cx - 10, H - STATUS_H + 5, cnt, (int)wcslen(cnt));
      SelectObject(memDC, oSt);
    }

    BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_ERASEBKGND:
    return 1;

  case WM_SIZE:
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;

  case WM_GETMINMAXINFO: {
    MINMAXINFO *mmi = (MINMAXINFO *)lp;
    mmi->ptMinTrackSize.x = 1120;
    mmi->ptMinTrackSize.y = 600;
    return 0;
  }

  case WM_DESTROY:
    buzzer_audio_cleanup();
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ============================================================================
   main_window_create — daftarkan class + buat window utama (hidden).
   Dipanggil sekali oleh wWinMain sebelum splash_run().
   ============================================================================ */
void main_window_create(HINSTANCE hInst) {
  WNDCLASSW wc = {0};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = L"SchematicEditorWnd";
  RegisterClassW(&wc);

  g_main_hwnd = CreateWindowExW(0, L"SchematicEditorWnd",
      L"Schematic Editor - Win32 GDI",
      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 650,
      NULL, NULL, hInst, NULL);

  ShowWindow(g_main_hwnd, SW_HIDE);
  UpdateWindow(g_main_hwnd);
}