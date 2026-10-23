/* ============================================================================
   src/ui/dialogs/file_dialogs.c — Dialog Win32 untuk Save / Open / Export.

   Isinya adalah "shell tipis" di atas fungsi pure:
     * model_save_json / model_load_json (src/io/fileio.c)
     * export_canvas_png               (src/io/export_png.c)

   Semua interaksi Win32 (OPENFILENAMEW, MessageBoxW) tinggal di sini.
   Dengan pemisahan ini:
     * src/io/fileio.c bisa di-unit-test di Linux tanpa MinGW.
     * Perubahan UX dialog (mis. tambah filter file, format pesan)
       TIDAK menyentuh logika JSON sama sekali.

   Header yang dipakai:
     * "ui.h" — sudah membawa <windows.h>, <stdio.h>, <string.h>,
       <wchar.h>, model.h, sim.h.
     * <commdlg.h> — OPENFILENAMEW, GetSaveFileNameW, GetOpenFileNameW.
     * <stdarg.h> — va_list untuk show_msg().
     * "io.h" — model_save_json, model_load_json, export_canvas_png.
   ============================================================================ */
#include "ui.h"
#include <commdlg.h>
#include <stdarg.h>
#include "io.h"

/* ===================== Helpers ===================== */

static void show_msg(HWND hwnd, const wchar_t *title, const wchar_t *fmt, ...) {
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, 512, fmt, ap);
    buf[511] = L'\0';
    va_end(ap);
    MessageBoxW(hwnd, buf, title, MB_OK | MB_ICONINFORMATION);
}

static void show_err(HWND hwnd, const wchar_t *title, const wchar_t *msg) {
    MessageBoxW(hwnd, msg, title, MB_OK | MB_ICONERROR);
}

/* ===================== Save dialog ===================== */

void model_save_dialog(HWND hwnd) {
    wchar_t file[MAX_PATH] = L"schematic.json";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;
    wchar_t err[256] = L"";
    if (model_save_json(file, err, 256)) {
        const wchar_t *base = wcsrchr(file, L'\\');
        set_status(L"Tersimpan: %ls (%d komponen, %d kabel).",
            base ? base + 1 : file, comp_count, wire_count);
        show_msg(hwnd, L"Save JSON", L"Tersimpan:\n%ls\n\n%d komponen, %d kabel.",
            file, comp_count, wire_count);
    } else {
        show_err(hwnd, L"Save JSON", err[0] ? err : L"Gagal menyimpan.");
    }
}

/* ===================== Open dialog ===================== */

void model_load_dialog(HWND hwnd) {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    wchar_t err[256] = L"";
    if (model_load_json(file, err, 256)) {
        const wchar_t *base = wcsrchr(file, L'\\');
        set_status(L"Dimuat: %ls (%d komponen, %d kabel).",
            base ? base + 1 : file, comp_count, wire_count);
        show_msg(hwnd, L"Open JSON", L"Dimuat:\n%ls\n\n%d komponen, %d kabel.",
            file, comp_count, wire_count);
    } else {
        show_err(hwnd, L"Open JSON", err[0] ? err : L"Gagal membuka.");
    }
}

/* ===================== Export PNG dialog ===================== */

void model_export_png_dialog(HWND hwnd) {
    wchar_t file[MAX_PATH] = L"schematic.png";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = L"PNG Image (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt  = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;

    wchar_t err[256] = L"";
    if (export_canvas_png(hwnd, file, err, 256)) {
        const wchar_t *base = wcsrchr(file, L'\\');
        set_status(L"PNG tersimpan: %ls", base ? base + 1 : file);
        MessageBoxW(hwnd,
            L"Canvas berhasil diekspor ke PNG.",
            L"Export PNG", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(hwnd, err[0] ? err : L"Gagal menyimpan PNG.",
                    L"Export PNG", MB_OK | MB_ICONERROR);
    }
}