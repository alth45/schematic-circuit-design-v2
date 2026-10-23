/* ============================================================================
   src/ui/panels/splash.c — Splash screen startup (modal, fade in/out).

   Window topmost layered 720×400 yang tampil selama ~2.2 detik saat startup,
   lalu di-dismiss dan window utama di-show. Menampilkan:
     * Panel kiri: logo resistor zig-zag + judul "Schematic Editor" + versi.
     * Panel kanan: grid 12 simbol komponen mini + label.
     * Panel bawah: progress bar + status.

   Include:
     * "ui.h"         — <windows.h>, semua API UI (splash_run dideklarasi di sini).
     * <stdbool.h>    — bool (dipakai original; C99 standar).

   Tidak memerlukan symbol core (comps[], sim_running, dst) — splash adalah
   window dekoratif murni.
   ============================================================================ */
#include "ui.h"
#include <stdbool.h>

#define SPLASH_W         720
#define SPLASH_H         400
#define SPLASH_MS        2200
#define SPLASH_FADE_IN   600
#define SPLASH_FADE_OUT  500
#define SPLASH_TICK      30

static HWND    g_splash_wnd      = NULL;
static int     g_splash_progress = 0;
static bool    g_splash_closing  = false;

#define SP_DARK    RGB(18, 28, 48)
#define SP_ACCENT  RGB(70, 130, 240)
#define SP_DIVIDE  RGB(40, 55, 75)
#define SP_SYM     RGB(180, 200, 220)
#define SP_LABEL   RGB(140, 160, 180)

/* ===================== Cache font ===================== */
static HFONT font_splash_title(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(40, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return h;
}
static HFONT font_splash_sub(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return h;
}
static HFONT font_splash_tiny(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return h;
}
static HFONT font_splash_label(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return h;
}

/* ===================== Simbol mini (skala ~0.55× simbol canvas) ===================== */
/* Semua fungsi memakai pens/brushes caller; hanya geometry. */

static void sp_resistor(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 10, cy);
    MoveToEx(hdc, cx + 10, cy, NULL); LineTo(hdc, cx + 22, cy);
    Rectangle(hdc, cx - 10, cy - 6, cx + 10, cy + 6);
}

static void sp_capacitor(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 3, cy);
    MoveToEx(hdc, cx + 3, cy, NULL); LineTo(hdc, cx + 22, cy);
    MoveToEx(hdc, cx - 3, cy - 9, NULL); LineTo(hdc, cx - 3, cy + 9);
    MoveToEx(hdc, cx + 3, cy - 9, NULL); LineTo(hdc, cx + 3, cy + 9);
}

static void sp_capolar(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 3, cy);
    MoveToEx(hdc, cx + 3, cy, NULL); LineTo(hdc, cx + 22, cy);
    MoveToEx(hdc, cx - 3, cy - 9, NULL); LineTo(hdc, cx - 3, cy + 9);
    Arc(hdc, cx - 12, cy - 18, cx + 21, cy + 18, cx + 3, cy + 9, cx + 3, cy - 9);
}

static void sp_led(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 6, cy);
    MoveToEx(hdc, cx + 6, cy, NULL); LineTo(hdc, cx + 22, cy);
    POINT tri[3] = { {cx - 6, cy - 8}, {cx - 6, cy + 8}, {cx + 6, cy} };
    Polygon(hdc, tri, 3);
    MoveToEx(hdc, cx + 6, cy - 8, NULL); LineTo(hdc, cx + 6, cy + 8);
    MoveToEx(hdc, cx - 2, cy - 10, NULL); LineTo(hdc, cx - 6, cy - 17);
    MoveToEx(hdc, cx + 2, cy - 10, NULL); LineTo(hdc, cx - 2, cy - 17);
}

static void sp_diode(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 6, cy);
    MoveToEx(hdc, cx + 6, cy, NULL); LineTo(hdc, cx + 22, cy);
    POINT tri[3] = { {cx - 6, cy - 8}, {cx - 6, cy + 8}, {cx + 6, cy} };
    Polygon(hdc, tri, 3);
    MoveToEx(hdc, cx + 6, cy - 8, NULL); LineTo(hdc, cx + 6, cy + 8);
}

static void sp_battery(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 5, cy);
    MoveToEx(hdc, cx + 5, cy, NULL); LineTo(hdc, cx + 22, cy);
    MoveToEx(hdc, cx - 5, cy - 9, NULL); LineTo(hdc, cx - 5, cy + 9);
    MoveToEx(hdc, cx + 5, cy - 6, NULL); LineTo(hdc, cx + 5, cy + 6);
}

static void sp_acsource(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 12, cy);
    MoveToEx(hdc, cx + 12, cy, NULL); LineTo(hdc, cx + 22, cy);
    Ellipse(hdc, cx - 12, cy - 12, cx + 12, cy + 12);
    int started = 0;
    for (int px = -9; px <= 9; px++) {
        double ph = (double)px / 9.0 * 3.14159265;
        int py = (int)(-6.0 * sin(ph));
        if (!started) { MoveToEx(hdc, cx + px, cy + py, NULL); started = 1; }
        else            LineTo(hdc, cx + px, cy + py);
    }
}

static void sp_switch(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 8, cy);
    MoveToEx(hdc, cx + 8, cy, NULL); LineTo(hdc, cx + 22, cy);
    Ellipse(hdc, cx - 11, cy - 2, cx - 5, cy + 2);
    Ellipse(hdc, cx + 5, cy - 2, cx + 11, cy + 2);
    MoveToEx(hdc, cx - 8, cy, NULL); LineTo(hdc, cx + 7, cy - 10);
}

static void sp_ground(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx, cy - 14, NULL); LineTo(hdc, cx, cy);
    MoveToEx(hdc, cx - 12, cy, NULL); LineTo(hdc, cx + 12, cy);
    MoveToEx(hdc, cx - 8, cy + 5, NULL); LineTo(hdc, cx + 8, cy + 5);
    MoveToEx(hdc, cx - 3, cy + 10, NULL); LineTo(hdc, cx + 3, cy + 10);
}

static void sp_transistor(HDC hdc, int cx, int cy) {
    Ellipse(hdc, cx - 14, cy - 14, cx + 14, cy + 14);
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 5, cy);
    MoveToEx(hdc, cx - 5, cy - 9, NULL); LineTo(hdc, cx - 5, cy + 9);
    MoveToEx(hdc, cx - 4, cy - 4, NULL); LineTo(hdc, cx + 22, cy - 16);
    MoveToEx(hdc, cx - 4, cy + 4, NULL); LineTo(hdc, cx + 22, cy + 16);
}

static void sp_ldr(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 14, cy);
    MoveToEx(hdc, cx + 14, cy, NULL); LineTo(hdc, cx + 22, cy);
    Rectangle(hdc, cx - 14, cy - 7, cx + 14, cy + 7);
    MoveToEx(hdc, cx - 14, cy - 3, NULL); LineTo(hdc, cx + 14, cy - 3);
    MoveToEx(hdc, cx - 14, cy + 3, NULL); LineTo(hdc, cx + 14, cy + 3);
    MoveToEx(hdc, cx - 18, cy - 16, NULL); LineTo(hdc, cx - 8, cy - 9);
    MoveToEx(hdc, cx - 10, cy - 18, NULL); LineTo(hdc, cx, cy - 11);
}

static void sp_buzzer(HDC hdc, int cx, int cy) {
    MoveToEx(hdc, cx - 22, cy, NULL); LineTo(hdc, cx - 8, cy);
    MoveToEx(hdc, cx + 8, cy, NULL); LineTo(hdc, cx + 22, cy);
    Arc(hdc, cx - 8, cy - 11, cx + 14, cy + 11, cx - 8, cy - 11, cx - 8, cy + 11);
    MoveToEx(hdc, cx - 8, cy - 11, NULL); LineTo(hdc, cx - 8, cy + 11);
    Arc(hdc, cx + 9, cy - 5, cx + 15, cy + 5, cx + 12, cy - 3, cx + 12, cy + 3);
    Arc(hdc, cx + 12, cy - 8, cx + 20, cy + 8, cx + 16, cy - 5, cx + 16, cy + 5);
}

/* ===================== Panel kiri: logo + judul ===================== */
static void splash_paint_left(HDC dc) {
    /* Ikon resistor zig-zag di atas judul */
    HPEN pen = CreatePen(PS_SOLID, 3, SP_ACCENT);
    HPEN op = (HPEN)SelectObject(dc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    {
        int ix = 40, iy = 52;
        static const int dz[] = { -10, 10, -10, 10, -10, 10, -10, 10 };
        MoveToEx(dc, ix, iy, NULL);
        LineTo(dc, ix + 14, iy);
        int px = ix + 14;
        for (unsigned k = 0; k < sizeof(dz) / sizeof(dz[0]); k++) {
            px += 9;
            LineTo(dc, px, iy + dz[k]);
        }
        px += 9;
        MoveToEx(dc, px, iy, NULL);
        LineTo(dc, px + 14, iy);
    }
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);

    /* Judul dua baris */
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, font_splash_title());
    SetTextColor(dc, RGB(255, 255, 255));
    TextOutW(dc, 38,  95, L"Schematic", 9);
    TextOutW(dc, 38, 138, L"Editor",    6);

    /* Subtitle */
    SelectObject(dc, font_splash_sub());
    SetTextColor(dc, RGB(160, 180, 210));
    TextOutW(dc, 40, 200, L"Simulasi rangkaian", 18);
    TextOutW(dc, 40, 222, L"Win32 GDI Edition",  17);

    /* Versi */
    SelectObject(dc, font_splash_tiny());
    SetTextColor(dc, RGB(120, 135, 160));
    TextOutW(dc, 40, 268, L"v1.0", 4);
}

/* ===================== Panel kanan: grid simbol ===================== */
typedef struct { int cx, cy; void (*fn)(HDC, int, int); const wchar_t *lbl; } Slot;

static void splash_paint_symbols(HDC dc) {
    static const Slot slots[12] = {
        /* Baris 1 */
        { 403,  70, sp_resistor,  L"R"   },
        { 530,  70, sp_capacitor, L"C"   },
        { 657,  70, sp_led,       L"LED" },
        /* Baris 2 */
        { 403, 140, sp_battery,   L"BAT" },
        { 530, 140, sp_acsource,  L"AC"  },
        { 657, 140, sp_diode,     L"D"   },
        /* Baris 3 */
        { 403, 210, sp_switch,    L"SW"  },
        { 530, 210, sp_transistor,L"Q"   },
        { 657, 210, sp_ground,    L"GND" },
        /* Baris 4 */
        { 403, 280, sp_ldr,       L"LDR" },
        { 530, 280, sp_buzzer,    L"BZR" },
        { 657, 280, sp_capolar,   L"CP"  },
    };

    HPEN symPen = CreatePen(PS_SOLID, 2, SP_SYM);
    HPEN oldP   = (HPEN)SelectObject(dc, symPen);
    HBRUSH oldB = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));

    for (int i = 0; i < 12; i++)
        slots[i].fn(dc, slots[i].cx, slots[i].cy - 12);

    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(symPen);

    /* Label di bawah tiap simbol */
    SelectObject(dc, font_splash_label());
    SetTextColor(dc, SP_LABEL);
    for (int i = 0; i < 12; i++) {
        SIZE sz;
        GetTextExtentPoint32W(dc, slots[i].lbl, (int)wcslen(slots[i].lbl), &sz);
        TextOutW(dc, slots[i].cx - sz.cx / 2, slots[i].cy + 20,
                 slots[i].lbl, (int)wcslen(slots[i].lbl));
    }
}

/* ===================== Panel bawah: progress bar + status ===================== */
static void splash_paint_progress(HDC dc, int prog) {
    int bx = 40, by = 340, bw = SPLASH_W - 80, bh = 10;

    HPEN bp = CreatePen(PS_SOLID, 1, RGB(60, 75, 105));
    HPEN obp = (HPEN)SelectObject(dc, bp);
    HBRUSH bb = CreateSolidBrush(RGB(32, 42, 66));
    HBRUSH obb = (HBRUSH)SelectObject(dc, bb);
    RoundRect(dc, bx, by, bx + bw, by + bh, 10, 10);
    SelectObject(dc, obp);
    SelectObject(dc, obb);
    DeleteObject(bp);
    DeleteObject(bb);

    if (prog > 0) {
        int fw = (bw - 4) * prog / 100;
        HBRUSH fb = CreateSolidBrush(SP_ACCENT);
        HBRUSH ofb = (HBRUSH)SelectObject(dc, fb);
        PatBlt(dc, bx + 2, by + 2, fw, bh - 4, PATCOPY);
        SelectObject(dc, ofb);
        DeleteObject(fb);
    }

    SelectObject(dc, font_splash_tiny());
    SetTextColor(dc, RGB(120, 135, 160));
    const wchar_t *st = (prog >= 100) ? L"Siap..." : L"Memuat editor...";
    TextOutW(dc, 40, 358, st, (int)wcslen(st));

    wchar_t pct[8];
    swprintf(pct, 8, L"%d%%", prog);
    SIZE psz;
    GetTextExtentPoint32W(dc, pct, (int)wcslen(pct), &psz);
    SetTextColor(dc, RGB(0, 200, 170));
    TextOutW(dc, SPLASH_W - 40 - psz.cx, 358, pct, (int)wcslen(pct));
}

/* ===================== Compositor ===================== */
static void splash_paint_content(HDC dc, int prog) {
    /* Latar gelap penuh */
    RECT rc = { 0, 0, SPLASH_W, SPLASH_H };
    HBRUSH bg = CreateSolidBrush(SP_DARK);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    /* Aksen vertikal kiri */
    HBRUSH ab = CreateSolidBrush(SP_ACCENT);
    RECT arc = { 0, 0, 6, SPLASH_H };
    FillRect(dc, &arc, ab);
    DeleteObject(ab);

    /* Garis pemisah vertikal antara panel kiri & kanan */
    HPEN div = CreatePen(PS_SOLID, 1, SP_DIVIDE);
    HPEN oldp = (HPEN)SelectObject(dc, div);
    MoveToEx(dc, 340, 30, NULL);
    LineTo(dc, 340, SPLASH_H - 70);
    SelectObject(dc, oldp);
    DeleteObject(div);

    splash_paint_left(dc);
    splash_paint_symbols(dc);
    splash_paint_progress(dc, prog);
}

/* ===================== Window procedure ===================== */
static LRESULT CALLBACK SplashProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int W = rc.right, H = rc.bottom;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, W, H);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            splash_paint_content(memDC, g_splash_progress);

            BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN:
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        case WM_DESTROY:
            g_splash_wnd = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ===================== Entry point ===================== */
void splash_run(HINSTANCE hInst, HWND hwnd_main) {
    static const wchar_t CLS[] = L"SchematicSplashWnd";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = SplashProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = CLS;
    BOOL we_registered = RegisterClassW(&wc);
    if (!we_registered) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            ShowWindow(hwnd_main, SW_SHOW);
            UpdateWindow(hwnd_main);
            SetForegroundWindow(hwnd_main);
            return;
        }
    }

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - SPLASH_W) / 2;
    int y = (sh - SPLASH_H) / 2;

    HWND hs = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED, CLS, L"Schematic Editor",
        WS_POPUP, x, y, SPLASH_W, SPLASH_H,
        NULL, NULL, hInst, NULL);
    if (!hs) {
        ShowWindow(hwnd_main, SW_SHOW);
        UpdateWindow(hwnd_main);
        SetForegroundWindow(hwnd_main);
        if (we_registered) UnregisterClassW(CLS, hInst);
        return;
    }

    g_splash_wnd      = hs;
    g_splash_progress = 0;
    g_splash_closing  = false;

    SetLayeredWindowAttributes(hs, 0, 0, LWA_ALPHA);
    ShowWindow(hs, SW_SHOWNA);
    UpdateWindow(hs);
    SetTimer(hs, 1, SPLASH_TICK, NULL);

    MSG m;
    DWORD t0 = GetTickCount();
    int fade_ticks = 0;
    bool main_shown = false;
    bool done = false;

    while (!done && GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);

        if (m.message == WM_TIMER && m.hwnd == hs && m.wParam == 1) {
            DWORD el = GetTickCount() - t0;

            if (!g_splash_closing) {
                g_splash_progress = (int)((el * 100) / 1600);
                if (g_splash_progress > 100) g_splash_progress = 100;

                DWORD ain = (el < SPLASH_FADE_IN) ? el : SPLASH_FADE_IN;
                BYTE a = (BYTE)(255 * ain / SPLASH_FADE_IN);
                SetLayeredWindowAttributes(hs, 0, a, LWA_ALPHA);
                InvalidateRect(hs, NULL, FALSE);

                if (el >= (DWORD)(SPLASH_MS - SPLASH_FADE_OUT)) {
                    g_splash_closing = true;
                    if (!main_shown) {
                        main_shown = true;
                        ShowWindow(hwnd_main, SW_SHOW);
                        UpdateWindow(hwnd_main);
                        SetForegroundWindow(hwnd_main);
                    }
                }
            } else {
                fade_ticks++;
                int steps = SPLASH_FADE_OUT / SPLASH_TICK;
                int a = 255 - (255 * fade_ticks) / (steps > 0 ? steps : 1);
                if (a <= 0) {
                    KillTimer(hs, 1);
                    DestroyWindow(hs);
                    done = true;
                } else {
                    SetLayeredWindowAttributes(hs, 0, (BYTE)a, LWA_ALPHA);
                }
            }
        }

        if (!IsWindow(hs)) done = true;
    }

    if (hs && IsWindow(hs)) {
        KillTimer(hs, 1);
        DestroyWindow(hs);
    }
    g_splash_wnd = NULL;

    if (!main_shown) {
        ShowWindow(hwnd_main, SW_SHOW);
        UpdateWindow(hwnd_main);
    }
    SetForegroundWindow(hwnd_main);

    if (we_registered) {
        UnregisterClassW(CLS, hInst);
    }
}