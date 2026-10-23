/* ============================================================================
   src/ui/panels/oscilloscope.c — Panel osiloskop (top-level window).

   Jendela terpisah yang menampilkan gelombang dari tiap COMP_OSCPROBE di
   canvas (maks 4 kanal, warna merah/hijau/biru/oranye). Trace di-refresh
   5 Hz via WM_TIMER.

   Grid: 10 div horizontal × 8 div vertikal, 5 subdivision tiap div.
   Skala vertikal dipilih otomatis dari max |V| (round-up pangkat 2).

   Include:
     * "ui.h" — <windows.h>, <math.h>, <stdio.h>, <string.h>, <wchar.h>,
                model.h, sim.h (deklarasi osc_open/refresh/close/is_open,
                comps[], comp_count, COMP_OSCPROBE, sim_ac_voltage,
                sim_build_probe_responses, sim_probe_voltage).
   ============================================================================ */
#include "ui.h"

#define OSC_W            720
#define OSC_H            360
#define OSC_SAMPLES      512
#define OSC_MAX_CH       4
#define OSC_PERIODS      4.0
#define OSC_FREQ_HZ      50.0

typedef struct {
    int      idx;                /* index ke comps[] */
    COLORREF color;
    double   buf[OSC_SAMPLES];
} OscChannel;

static HWND       g_osc_wnd  = NULL;
static OscChannel g_osc_ch[OSC_MAX_CH];
static int        g_osc_nch  = 0;

/* Font di-cache sekali per proses. */
static HFONT osc_font(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        0, 0, 0, 0, 0, L"Consolas");
    return h;
}

static void osc_recompute_traces(void) {
    g_osc_nch = 0;
    static const COLORREF pal[OSC_MAX_CH] = {
        RGB(230, 60, 60),  RGB(80, 220, 80),
        RGB(80, 150, 240), RGB(240, 170, 60)
    };
    for (int i = 0; i < comp_count && g_osc_nch < OSC_MAX_CH; i++) {
        if (comps[i].type == COMP_OSCPROBE) {
            g_osc_ch[g_osc_nch].idx   = i;
            g_osc_ch[g_osc_nch].color = pal[g_osc_nch];
            g_osc_nch++;
        }
    }
    if (g_osc_nch == 0) return;

    sim_build_probe_responses();

    double T  = 1.0 / OSC_FREQ_HZ;
    double dt = T * OSC_PERIODS / OSC_SAMPLES;
    for (int ch = 0; ch < g_osc_nch; ch++) {
        Component *probe = &comps[g_osc_ch[ch].idx];
        for (int s = 0; s < OSC_SAMPLES; s++)
            g_osc_ch[ch].buf[s] = sim_probe_voltage(probe, s * dt);
    }
}

static void osc_draw_grid(HDC hdc, const RECT *plot, int ndx, int ndy, int sub) {
    int pw = plot->right - plot->left;
    int ph = plot->bottom - plot->top;

    HPEN minor = CreatePen(PS_SOLID, 1, RGB(30, 50, 30));
    HPEN major = CreatePen(PS_SOLID, 1, RGB(60, 90, 60));
    HPEN center = CreatePen(PS_SOLID, 1, RGB(110, 150, 110));
    HPEN oldp = (HPEN)SelectObject(hdc, minor);

    int sx = ndx * sub, sy = ndy * sub;
    for (int i = 0; i <= sx; i++) {
        int x = plot->left + i * pw / sx;
        MoveToEx(hdc, x, plot->top, NULL); LineTo(hdc, x, plot->bottom);
    }
    for (int i = 0; i <= sy; i++) {
        int y = plot->top + i * ph / sy;
        MoveToEx(hdc, plot->left, y, NULL); LineTo(hdc, plot->right, y);
    }

    SelectObject(hdc, major);
    for (int i = 0; i <= ndx; i++) {
        int x = plot->left + i * pw / ndx;
        MoveToEx(hdc, x, plot->top, NULL); LineTo(hdc, x, plot->bottom);
    }
    for (int i = 0; i <= ndy; i++) {
        int y = plot->top + i * ph / ndy;
        MoveToEx(hdc, plot->left, y, NULL); LineTo(hdc, plot->right, y);
    }

    SelectObject(hdc, center);
    int cx = plot->left + pw / 2;
    int cy = plot->top + ph / 2;
    MoveToEx(hdc, cx, plot->top, NULL); LineTo(hdc, cx, plot->bottom);
    MoveToEx(hdc, plot->left, cy, NULL); LineTo(hdc, plot->right, cy);

    SelectObject(hdc, oldp);
    DeleteObject(minor);
    DeleteObject(major);
    DeleteObject(center);
}

static void osc_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    HBITMAP oldbmp = (HBITMAP)SelectObject(mem, bmp);

    HBRUSH bg = CreateSolidBrush(RGB(15, 25, 15));
    FillRect(mem, &rc, bg);
    DeleteObject(bg);

    RECT plot = { 50, 30, W - 20, H - 40 };
    int pw = plot.right - plot.left;
    int ph = plot.bottom - plot.top;

    osc_draw_grid(mem, &plot, 10, 8, 5);

    /* Skala vertikal otomatis: vmax → 4 div atas/bawah. */
    double vmax = 0.5;
    for (int ch = 0; ch < g_osc_nch; ch++)
        for (int s = 0; s < OSC_SAMPLES; s++) {
            double v = fabs(g_osc_ch[ch].buf[s]);
            if (v > vmax) vmax = v;
        }
    double v_per_div = 1.0;
    while (4.0 * v_per_div < vmax) v_per_div *= 2.0;

    /* Trace per kanal. */
    for (int ch = 0; ch < g_osc_nch; ch++) {
        HPEN pen = CreatePen(PS_SOLID, 2, g_osc_ch[ch].color);
        HPEN oldp = (HPEN)SelectObject(mem, pen);
        int started = 0;
        for (int s = 0; s < OSC_SAMPLES; s++) {
            int x = plot.left + s * pw / (OSC_SAMPLES - 1);
            double v = g_osc_ch[ch].buf[s];
            int y = plot.top + ph / 2 -
                    (int)(v / (4.0 * v_per_div) * (ph / 2.0));
            if (!started) { MoveToEx(mem, x, y, NULL); started = 1; }
            else LineTo(mem, x, y);
        }
        SelectObject(mem, oldp);
        DeleteObject(pen);
    }

    /* Info skala + legenda. */
    HFONT hf = osc_font();
    HFONT oldf = (HFONT)SelectObject(mem, hf);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(180, 200, 180));

    wchar_t info[128];
    swprintf(info, 128, L"Time: %.2f ms/div  |  V: %.3g V/div",
        OSC_PERIODS * (1.0 / OSC_FREQ_HZ) / 10.0 * 1000.0, v_per_div);
    TextOutW(mem, 10, 6, info, (int)wcslen(info));

    int ly = H - 28;
    static const wchar_t *names[] = { L"CH1", L"CH2", L"CH3", L"CH4" };
    for (int ch = 0; ch < g_osc_nch; ch++) {
        HBRUSH br = CreateSolidBrush(g_osc_ch[ch].color);
        RECT lr = { 12 + ch * 90, ly + 2, 26 + ch * 90, ly + 14 };
        FillRect(mem, &lr, br);
        DeleteObject(br);
        SetTextColor(mem, RGB(200, 220, 200));
        TextOutW(mem, 32 + ch * 90, ly, names[ch], 3);
    }

    if (g_osc_nch == 0) {
        SetTextColor(mem, RGB(150, 150, 150));
        const wchar_t *msg =
            L"Belum ada probe osiloskop di canvas.\n"
            L"Tambahkan via toolbar (OSC) atau shortcut 'P'.";
        DrawTextW(mem, msg, -1, &plot, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }

    SelectObject(mem, oldf);

    BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldbmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK OscProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            osc_paint(hwnd);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_TIMER:
            osc_recompute_traces();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            g_osc_wnd = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void osc_open(HWND hwnd_parent) {
    if (g_osc_wnd && IsWindow(g_osc_wnd)) {
        ShowWindow(g_osc_wnd, SW_SHOW);
        SetForegroundWindow(g_osc_wnd);
        return;
    }

    static const wchar_t CLS[] = L"OscilloscopeWnd";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = OscProc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = CLS;
    if (!RegisterClassW(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

    RECT pr; GetWindowRect(hwnd_parent, &pr);
    g_osc_wnd = CreateWindowExW(WS_EX_TOOLWINDOW, CLS, L"Oscilloscope",
        WS_OVERLAPPEDWINDOW, pr.left + 80, pr.top + 80, OSC_W, OSC_H,
        hwnd_parent, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_osc_wnd) return;

    ShowWindow(g_osc_wnd, SW_SHOW);
    UpdateWindow(g_osc_wnd);
    osc_recompute_traces();
    InvalidateRect(g_osc_wnd, NULL, FALSE);
    SetTimer(g_osc_wnd, 1, 200, NULL);
}

void osc_refresh(void) {
    if (!g_osc_wnd || !IsWindow(g_osc_wnd)) return;
    osc_recompute_traces();
    InvalidateRect(g_osc_wnd, NULL, FALSE);
}

void osc_close(void) {
    if (g_osc_wnd && IsWindow(g_osc_wnd))
        DestroyWindow(g_osc_wnd);
    g_osc_wnd = NULL;
}

int osc_is_open(void) {
    return g_osc_wnd && IsWindow(g_osc_wnd);
}