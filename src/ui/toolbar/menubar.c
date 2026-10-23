#include "toolbar_intern.h"

#define MENU_TOP    2
#define MENU_ITEM_H 20
#define MENU_PAD_X  12

static const wchar_t *g_menus[] = { L"File", L"Edit", L"View", L"Simulasi", L"Bantu" };
#define MENU_COUNT ((int)(sizeof(g_menus)/sizeof(g_menus[0])))

/* Satu font menubar di-cache untuk seluruh proses. */
static HFONT font_menubar(void) {
    static HFONT h = NULL;
    if (!h) h = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        0, 0, 0, 0, 0, L"Segoe UI");
    return h;
}

static int menu_x0(int i) {
    static int xs[8] = { 0 };
    static int cached = -1;
    HDC probe = GetDC(NULL);
    HFONT hF = font_menubar();
    HFONT of = (HFONT)SelectObject(probe, hF);
    if (cached != MENU_COUNT) {
        int x = 8;
        for (int j = 0; j < MENU_COUNT && j < 8; j++) {
            xs[j] = x;
            SIZE sz;
            GetTextExtentPoint32W(probe, g_menus[j], lstrlenW(g_menus[j]), &sz);
            x += sz.cx + MENU_PAD_X * 2;
        }
        cached = MENU_COUNT;
    }
    SelectObject(probe, of);
    /* font di-cache: jangan DeleteObject */
    ReleaseDC(NULL, probe);
    return (i >= 0 && i < 8) ? xs[i] : 8;
}

static int menu_width(HDC hdc, int i) {
    SIZE sz;
    GetTextExtentPoint32W(hdc, g_menus[i], lstrlenW(g_menus[i]), &sz);
    return sz.cx + MENU_PAD_X * 2;
}

int menubar_hit(int x, int y) {
    if (y < 0 || y >= MENUBAR_H) return -1;
    HDC hdc = GetDC(NULL);
    HFONT hF = font_menubar();
    HFONT of = (HFONT)SelectObject(hdc, hF);
    int res = -1;
    for (int i = 0; i < MENU_COUNT; i++) {
        int x0 = menu_x0(i);
        if (x >= x0 && x < x0 + menu_width(hdc, i)) { res = i; break; }
    }
    SelectObject(hdc, of);
    ReleaseDC(NULL, hdc);
    return res;
}

/* ---- build_popup, menubar_command, menubar_click tetap SAMA PERSIS
       dengan versimu; hanya draw_menubar di bawah yang berubah ---- */

static void build_popup(int menu_idx, HMENU m) {
    switch (menu_idx) {
        case 0:
            AppendMenuW(m, MF_STRING, IDM_FILE_NEW,    L"New / Bersihkan Canvas\tCtrl+N");
            AppendMenuW(m, MF_STRING, IDM_FILE_OPEN,   L"Open... (JSON)\tCtrl+O");
            AppendMenuW(m, MF_STRING, IDM_FILE_SAVE,   L"Save\tCtrl+S");
            AppendMenuW(m, MF_STRING, IDM_FILE_SAVEAS, L"Save As...\tCtrl+Shift+S");
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
            AppendMenuW(m, MF_STRING, IDM_FILE_EXPORT_PNG, L"Export as PNG...\tCtrl+E");
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);

            AppendMenuW(m, MF_STRING, IDM_FILE_EXIT,   L"Exit\tAlt+F4");
            break;
        case 1:
            AppendMenuW(m, MF_STRING | (undo_can_undo() ? 0 : MF_GRAYED), IDM_EDIT_UNDO, L"Undo\tCtrl+Z");
            AppendMenuW(m, MF_STRING | (undo_can_redo() ? 0 : MF_GRAYED), IDM_EDIT_REDO, L"Redo\tCtrl+Y");
            AppendMenuW(m, MF_STRING, IDM_EDIT_DUPL,   L"Duplikat Komponen Terpilih");
            AppendMenuW(m, MF_STRING, IDM_EDIT_DELSEL, L"Hapus Komponen Terpilih\tDel");
            AppendMenuW(m, MF_STRING, IDM_EDIT_ROTATE, L"Putar 90\xB0\tR");
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
            AppendMenuW(m, MF_STRING, IDM_EDIT_CLEAR,  L"Clear Canvas");
            break;
        case 2:
            AppendMenuW(m, MF_STRING | (g_show_grid ? MF_CHECKED : 0),
                        IDM_VIEW_GRID, L"Tampilkan Grid");
            AppendMenuW(m, MF_STRING | (g_show_status ? MF_CHECKED : 0),
                        IDM_VIEW_STATUS, L"Tampilkan Status Bar");
             AppendMenuW(m, MF_STRING | (g_show_minimap ? MF_CHECKED : 0),
                       IDM_VIEW_MINIMAP, L"Minimap");
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
            AppendMenuW(m, MF_STRING, IDM_VIEW_OSC, L"Oscilloscope (Probe)\tF12");
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
            AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMIN,  L"Perbesar\tCtrl++");
            AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMOUT, L"Perkecil\tCtrl+-");
            AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMFIT, L"Sesuaikan Tampilan\tCtrl+0");
            break;
        case 3:
            AppendMenuW(m, MF_STRING | ((sim_running || comp_count == 0) ? MF_GRAYED : 0),
                        IDM_SIM_RUN, L"Jalankan Simulasi\tF5");
            AppendMenuW(m, MF_STRING | (sim_running ? 0 : MF_GRAYED),
                        IDM_SIM_STOP, L"Hentikan Simulasi\tF5");
            AppendMenuW(m, MF_STRING | (sim_running ? MF_GRAYED : 0),
                        IDM_SIM_RECOMPUTE, L"Hitung Ulang\tF9");
            break;
        case 4:
            AppendMenuW(m, MF_STRING, IDM_HELP_SHORTCUT, L"Daftar Pintasan Papan Ketik...");
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
            AppendMenuW(m, MF_STRING, IDM_HELP_ABOUT, L"Tentang Schematic Editor...");
            break;
    }
}

void menubar_command(HWND hwnd, int cmd) {
    switch (cmd) {
        case IDM_FILE_NEW:
            if (sim_running) { set_status(L"STOP dulu sebelum New/Clear."); break; }
            model_clear_all();
            sim_recompute();
            undo_reset();
            set_status(L"Canvas dikosongkan (File > New).");
            break;
        case IDM_FILE_OPEN:
            model_load_dialog(hwnd);
            sim_recompute();
            undo_reset();
            break;
        case IDM_FILE_SAVE:
        case IDM_FILE_SAVEAS:
            model_save_dialog(hwnd);
            break;
        case IDM_FILE_EXPORT_PNG:
            model_export_png_dialog(hwnd);
            break;
        case IDM_FILE_EXIT:
            DestroyWindow(hwnd);
            break;
        case IDM_EDIT_UNDO:
            if (sim_running) break;
            if (undo_undo()) set_status(L"Undo.");
            else set_status(L"Tidak ada lagi yang bisa di-undo.");
            break;
        case IDM_EDIT_REDO:
            if (sim_running) break;
            if (undo_redo()) set_status(L"Redo.");
            else set_status(L"Tidak ada lagi yang bisa di-redo.");
            break;
        case IDM_EDIT_DUPL: {
            Component *c = find_comp_by_id(selected_id);
            if (!c) { set_status(L"Edit > Duplikat: pilih komponen dulu."); break; }
            CompType t = c->type; double v = c->v;
            int rot = c->rot, on = c->on;
            model_add_component(t, c->x + 3 * GRID, c->y + 3 * GRID);
            Component *n = find_comp_by_id(selected_id);
            if (n) { n->v = v; n->rot = rot; n->on = on; sim_recompute(); }
            set_status(L"Komponen diduplikat.");
            break;
        }
        case IDM_EDIT_DELSEL: {
            if (sim_running) break;
            Component *c = find_comp_by_id(selected_id);
            if (!c) {
                set_status(L"Edit > Hapus: tidak ada komponen terpilih.");
                break;
            }
            model_remove_at(c->x, c->y);
            sim_recompute();
            break;
        }
        case IDM_EDIT_ROTATE:
            if (sim_running) break;
            model_rotate_selected();
            break;
        case IDM_EDIT_CLEAR:
            if (sim_running) { set_status(L"STOP dulu sebelum Clear."); break; }
            model_clear_all();
            sim_recompute();
            set_status(L"Dibersihkan. Canvas kosong.");
            break;
        case IDM_EDIT_SELECTALL: {
            current_tool = TOOL_SELECT;
            sel_count = 0; selected_id = -1;
            for (int i = 0; i < comp_count; i++) add_to_selection(comps[i].id);
            set_status(L"%d komponen terpilih (semua).", sel_count);
            break;
        }
        case IDM_VIEW_GRID:
            g_show_grid = !g_show_grid;
            set_status(L"Grid %ls.", g_show_grid ? L"ditampilkan" : L"disembunyikan");
            break;
        case IDM_VIEW_STATUS:
            g_show_status = !g_show_status;
            set_status(L"Status bar %ls.", g_show_status ? L"ditampilkan" : L"disembunyikan");
            break;
        case IDM_VIEW_MINIMAP:
            g_show_minimap = !g_show_minimap;
            set_status(L"Minimap %ls.", g_show_minimap ? L"ditampilkan" : L"disembunyikan");
            break;
        case IDM_VIEW_ZOOMIN:
            canvas_zoom_center(hwnd, 1.25);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case IDM_VIEW_ZOOMOUT:
            canvas_zoom_center(hwnd, 1.0 / 1.25);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case IDM_VIEW_ZOOMFIT: {
            RECT rc; GetClientRect(hwnd, &rc);
            int canvas_h = rc.bottom - TOOLBAR_H - (g_show_status ? STATUS_H : 0);
            canvas_zoom_fit(rc.right, canvas_h);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        case IDM_VIEW_OSC:
            osc_open(hwnd);
            break;
        case IDM_SIM_RUN:
            if (!sim_running) sim_set_running(1);
            break;
        case IDM_SIM_STOP:
            if (sim_running) sim_set_running(0);
            break;
        case IDM_SIM_RECOMPUTE:
            sim_recompute();
            set_status(L"%ls", sim_message());
            break;
        case IDM_SIM_RUN_TOGGLE:
            sim_set_running(!sim_running);
            break;
        case IDM_HELP_SHORTCUT:
            MessageBoxW(hwnd,
                L"F5 / F9      : Run / Stop simulasi\n"
                L"Ctrl+S       : Save JSON\n"
                L"Ctrl+O       : Open JSON\n"
                L"Ctrl+Z / Ctrl+Y : Undo / Redo\n"
                L"V / W        : tool Select / Wire\n"
                L"1..9, 0      : Resistor, Saklar, Baterai, LED, Ground,\n"
                L"               Kapasitor, Transistor, Dioda, Kap. Polar, Sumber AC\n"
                L"R            : Putar komponen terpilih\n"
                L"Del / Back   : Hapus komponen terpilih\n"
                L"Esc          : Batal pilih / kabel\n"
                L"Double-klik  : Edit nilai komponen\n"
                L"Klik-kanan   : Menu konteks komponen",
                L"Pintasan Papan Ketik", MB_OK | MB_ICONINFORMATION);
            break;
            case IDM_HELP_ABOUT: {
            const wchar_t *msg =
                L"Schematic Editor\n"
                L"Version 1.0  (Win32 GDI Edition)\n"
                L"\n"
                L"Editor dan simulator rangkaian elektronika berbasis Win32 GDI.\n"
                L"Dirancang untuk pembelajaran, eksperimen, dan visualisasi\n"
                L"konsep dasar listrik secara interaktif.\n"
                L"\n"
                L"Fitur utama:\n"
                L"  \x2022  Editor skematik dengan rotasi, multi-select,\n"
                L"     duplikasi blok rangkaian, dan undo/redo.\n"
                L"  \x2022  Simulasi DC & AC (analisis nodal) dengan LED, dioda,\n"
                L"     transistor NPN, kapasitor, dan induktor.\n"
                L"  \x2022  Alat ukur interaktif: Voltmeter, Ammeter, dan\n"
                L"     Probe Osiloskop untuk bentuk gelombang.\n"
                L"  \x2022  Zoom & pan canvas, penyimpanan skematik via JSON.\n"
                L"\n"
                L"Menu: File \x2022 Edit \x2022 View \x2022 Simulasi \x2022 Bantu\n"
                L"\n"
                L"\xA9 2026 Schematic Editor Project. All rights reserved.";
            MessageBoxW(hwnd, msg, L"About Schematic Editor",
                        MB_OK | MB_ICONINFORMATION);
            break;
        }
        
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

int menubar_click(HWND hwnd, int x, int y) {
    int idx = menubar_hit(x, y);
    if (idx < 0) return 0;
    HMENU m = CreatePopupMenu();
    if (!m) return 0;
    build_popup(idx, m);
    RECT rc; GetClientRect(hwnd, &rc);
    POINT p = { menu_x0(idx), MENUBAR_H };
    ClientToScreen(hwnd, &p);
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY,
                             p.x, p.y, 0, hwnd, NULL);
    DestroyMenu(m);
    if (cmd == 0) return 0;
    menubar_command(hwnd, cmd);
    return cmd;
}

void draw_menubar(HDC hdc, int W) {
    RECT bar = { 0, 0, W, MENUBAR_H };
    HBRUSH bg = CreateSolidBrush(RGB(250, 250, 250));
    FillRect(hdc, &bar, bg);
    DeleteObject(bg);
    HPEN sep = CreatePen(PS_SOLID, 1, RGB(205, 205, 205));
    HPEN osp = (HPEN)SelectObject(hdc, sep);
    MoveToEx(hdc, 0, MENUBAR_H - 1, NULL); LineTo(hdc, W, MENUBAR_H - 1);
    SelectObject(hdc, osp);
    DeleteObject(sep);

    HFONT hF = font_menubar();
    HFONT of = (HFONT)SelectObject(hdc, hF);
    SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < MENU_COUNT; i++) {
        int x0 = menu_x0(i);
        int wdt = menu_width(hdc, i);
        SetTextColor(hdc, RGB(40, 40, 40));
        RECT it = { x0, 2, x0 + wdt, MENUBAR_H - 3 };
        DrawTextW(hdc, g_menus[i], -1, &it, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(hdc, of);
    /* font di-cache */
}