/* ============================================================================
   src/ui/dialogs/value_editor.c — Dialog modal "Edit Nilai Komponen".

   Dipindah dari src/core/model.c (Step 15 refactor). Alasan:
     * model.c adalah CORE murni — tidak boleh include <windows.h> supaya
       bisa di-unit-test di Linux.
     * Dialog ini butuh Win32 (CreateWindowExW, MessageBoxW, GetMessageW, dll)
       → ranah UI layer.

   Alur:
     1. Buka modal dialog dengan edit box berisi nilai awal (formatted SI).
     2. User ketik nilai baru → parse via parse_value_text() (core/model.h).
     3. Valid? → model_set_component_value() + sim_recompute().

   Include:
     * "ui.h"    — <windows.h>, <stdio.h>, <wchar.h>, model.h, sim.h.
     * "model.h" — parse_value_text, format_value, comp_has_value,
                   comp_value_unit, model_set_component_value, set_status.
     * "sim.h"   — sim_recompute (untuk refresh setelah nilai berubah).
   ============================================================================ */
#include "ui.h"
#include "model.h"
#include "sim.h"

#define VE_ID_EDIT 1001

typedef struct {
    Component *c;
    HWND   edit;
    wchar_t start[VALUE_STR_CAP];
    int    ok;
    int    done;
} ValEditCtx;

static ValEditCtx *ve_get_ctx(HWND hDlg) {
    return (ValEditCtx *)(LONG_PTR)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
}

static void ve_set_ctx(HWND hDlg, ValEditCtx *ctx) {
    SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)ctx);
}

static void ve_do_close(ValEditCtx *ctx, HWND hDlg) {
    if (ctx) ctx->done = 1;
    if (hDlg && IsWindow(hDlg)) DestroyWindow(hDlg);
    PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
}

static HFONT ve_font(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    return h;
}

static LRESULT CALLBACK ValEditProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        if (cs && cs->lpCreateParams) ve_set_ctx(hDlg, (ValEditCtx *)cs->lpCreateParams);
        return DefWindowProcW(hDlg, msg, wp, lp);
    }

    ValEditCtx *ctx = ve_get_ctx(hDlg);

    switch (msg) {
        case WM_CREATE: {
            if (!ctx) return 0;
            HINSTANCE hi = ((CREATESTRUCTW *)lp)->hInstance;

            const int DLG_W = 380;
            const int cw = 12, ch = 10;
            const int lblH = 22;
            const int edX = cw, edY = ch + lblH + 4;
            const int edW = DLG_W - 2 * cw - 120;
            const int btnY = edY + 30 + 14;

            const wchar_t *unit = comp_value_unit(ctx->c->type);

            wchar_t lbl[96];
            swprintf(lbl, 96, L"Nilai (%ls) — Enter/OK untuk simpan:", unit ? unit : L"?");
            HWND hLbl = CreateWindowExW(0, L"STATIC", lbl,
                WS_CHILD | WS_VISIBLE | SS_LEFT, cw, ch, DLG_W - 2 * cw, lblH,
                hDlg, NULL, hi, NULL);

            HWND hEd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ctx->start,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL,
                edX, edY, edW, 26, hDlg, (HMENU)(INT_PTR)VE_ID_EDIT, hi, NULL);
            ctx->edit = hEd;

            wchar_t hint[48];
            swprintf(hint, 48, L"satuan: %ls", unit ? unit : L"-");
            HWND hHint = CreateWindowExW(0, L"STATIC", hint,
                WS_CHILD | WS_VISIBLE | SS_LEFT, edX + edW + 10, edY + 4, 110, 20,
                hDlg, NULL, hi, NULL);

            HWND hOk = CreateWindowExW(0, L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                DLG_W - 190, btnY, 85, 28, hDlg, (HMENU)(INT_PTR)IDOK, hi, NULL);
            HWND hCa = CreateWindowExW(0, L"BUTTON", L"Batal",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                DLG_W - 100, btnY, 85, 28, hDlg, (HMENU)(INT_PTR)IDCANCEL, hi, NULL);

            HFONT hfont = ve_font();
            if (hfont) {
                SendMessageW(hLbl,  WM_SETFONT, (WPARAM)hfont, TRUE);
                SendMessageW(hEd,   WM_SETFONT, (WPARAM)hfont, TRUE);
                SendMessageW(hHint, WM_SETFONT, (WPARAM)hfont, TRUE);
                SendMessageW(hOk,   WM_SETFONT, (WPARAM)hfont, TRUE);
                SendMessageW(hCa,   WM_SETFONT, (WPARAM)hfont, TRUE);
            }
            return 0;
        }

        case WM_SETFOCUS:
            if (ctx && ctx->edit) SetFocus(ctx->edit);
            return 0;

        case WM_COMMAND:
            if (!ctx) return 0;
            if (LOWORD(wp) == IDOK) {
                wchar_t txt[64] = L"";
                GetWindowTextW(ctx->edit, txt, 64);
                double v = 0.0;
                if (!parse_value_text(txt, &v)) {
                    MessageBoxW(hDlg,
                        L"Nilai tidak valid.\n\nContoh yang diterima:\n"
                        L"  resistor   : 220, 4.7k, 1M, 10k\n"
                        L"  baterai/AC : 9, 4.5, 220 (AC = VAC RMS)\n"
                        L"  kapasitor  : 100n, 47u, 10uF",
                        L"Edit Nilai", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                model_set_component_value(ctx->c ? ctx->c->id : -1, v);
                ctx->ok = 1;
                ve_do_close(ctx, hDlg);
                return 0;
            }
            if (LOWORD(wp) == IDCANCEL) {
                ve_do_close(ctx, hDlg);
                return 0;
            }
            return 0;

        case WM_CLOSE:
            ve_do_close(ctx, hDlg);
            return 0;

        case WM_DESTROY:
            if (ctx) ctx->done = 1;
            return 0;
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

static ATOM val_edit_atom(void) {
    static ATOM atom = 0;
    if (atom) return atom;
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof wc;
    wc.lpfnWndProc   = ValEditProc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SchematicValEditDlg";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    atom = RegisterClassExW(&wc);
    return atom;
}

void edit_value_dialog(HWND hwnd, Component *c) {
    if (!c || !comp_has_value(c->type)) return;

    ATOM a = val_edit_atom();
    if (!a) return;

    const wchar_t *unit = comp_value_unit(c->type);
    wchar_t start[VALUE_STR_CAP];
    /* Format HANYA angka + prefix SI (k/M/m/u/n/p), tanpa unit suffix.
       Contoh: 5.1 (bukan "5.1 Vz"). Unit tetap tampil di hint label
       di sebelah edit box. Ini memastikan round-trip:
         format_value(v, NULL) → parse_value_text() → v  (selalu berhasil). */
    format_value(c->v, NULL, start, VALUE_STR_CAP);

    wchar_t title[96];
    swprintf(title, 96, L"Edit Nilai Komponen — %s",
        (c->type == COMP_RESISTOR)  ? L"Resistor" :
        (c->type == COMP_BATTERY)   ? L"Baterai" :
        (c->type == COMP_ACSOURCE)  ? L"Sumber AC (VAC RMS)" :
        (c->type == COMP_LED)       ? L"LED (Vf)" :
        (c->type == COMP_DIODE)     ? L"Dioda (Vf)" :
        (c->type == COMP_CAPACITOR) ? L"Kapasitor" :
        (c->type == COMP_CAPOLAR)   ? L"Kapasitor Polar" : L"Komponen");

    ValEditCtx ctx;
    ZeroMemory(&ctx, sizeof ctx);
    ctx.c = c;
    ctx.ok = 0;
    ctx.done = 0;
    lstrcpynW(ctx.start, start, VALUE_STR_CAP);

    const int DLG_W = 380, DLG_H = 150;
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        (LPCWSTR)(UINT_PTR)a, title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, DLG_W, DLG_H,
        hwnd, NULL, GetModuleHandleW(NULL), &ctx);
    if (!hDlg) return;

    RECT pr; GetWindowRect(hwnd, &pr);
    int px = pr.left + (((pr.right - pr.left) - DLG_W) / 2);
    int py = pr.top  + (((pr.bottom - pr.top) - DLG_H) / 2);
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    SetWindowPos(hDlg, HWND_TOPMOST, px, py, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    if (ctx.edit) { SetFocus(ctx.edit); SendMessageW(ctx.edit, EM_SETSEL, 0, -1); }

    EnableWindow(hwnd, FALSE);

    MSG m;
    while (!ctx.done && GetMessageW(&m, NULL, 0, 0) > 0) {
        if (IsWindow(hDlg) && IsDialogMessageW(hDlg, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    if (IsWindow(hDlg)) DestroyWindow(hDlg);

    EnableWindow(hwnd, TRUE);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    if (ctx.ok) {
        wchar_t nv[VALUE_STR_CAP];
        format_value(c->v, unit, nv, VALUE_STR_CAP);
        set_status(L"Nilai baru dipasang: %ls.", nv);
        sim_recompute();
    }
}