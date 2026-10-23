#ifndef UI_H
#define UI_H

/* ============================================================================
   ui.h — API publik modul UI (Win32 GDI).

   Ini adalah SATU-SATUNYA header proyek yang boleh #include <windows.h>.
   Semua modul pure (core/, io/, audio/, tests/) TIDAK BOLEH menyentuh
   header ini — kecuali modul yang memang rendering UI.

   Isi header ini dikelompokkan:
     1. Menu command IDs (IDM_*)
     2. Toolbar (palette + scrollbar)
     3. Menu bar
     4. Canvas draw (simbol per komponen, wire, overlay)
     5. Canvas event handlers (mouse, keyboard, context menu)
     6. Minimap
     7. Panel osiloskop
     8. Splash screen
     9. Dialog wrappers (file save/load/export, edit value, zoom)
    10. Window procedure (entry point WM_* dispatch)

   Catatan penting:
     * draw_canvas(HWND) DENGAN SENGAJA tidak dideklarasikan — itu dead
       code di source lama. Satu-satunya jalur render adalah
       render_canvas_scene(HDC, W, H, include_ui_overlay) dari WM_PAINT
       dan export_png. Saat migrasi draw.c ke src/ui/canvas/draw.c,
       definisi draw_canvas() dihapus.
     * toolbar_intern.h (drawing detail tombol) TIDAK ada di sini; ia
       tinggal di src/ui/toolbar/toolbar_intern.h sebagai header internal
       modul toolbar.
   ============================================================================
 */

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>


#include "model.h" /* state model + canvas helpers yang dipakai di UI */
#include "sim.h"   /* sim_running, sim_short_x, dst untuk coloring     */
#include "types.h"


/* ============================================================================
   BAGIAN 1 — Menu command IDs

   Konstanta stabil untuk WM_COMMAND / TrackPopupMenu / accelerator table.
   Range:
     0x1101..0x110F = File
     0x1110..0x111F = Edit
     0x1120..0x112F = View
     0x1130..0x113F = Simulasi
     0x1140..0x114F = Bantu / Help
     0x1150..0x115F = View (extended — osiloskop)
     <0x0500        = context menu (event handler internal, lihat events.c)
   ============================================================================
 */

/* ---- File ---- */
#define IDM_FILE_NEW 0x1101
#define IDM_FILE_OPEN 0x1102
#define IDM_FILE_SAVE 0x1103
#define IDM_FILE_SAVEAS 0x1104
#define IDM_FILE_EXIT 0x1105
#define IDM_FILE_EXPORT_PNG 0x1106

/* ---- Edit ---- */
#define IDM_EDIT_UNDO 0x1110
#define IDM_EDIT_DUPL 0x1111
#define IDM_EDIT_DELSEL 0x1112
#define IDM_EDIT_ROTATE 0x1113
#define IDM_EDIT_CLEAR 0x1114
#define IDM_EDIT_SELECTALL 0x1115
#define IDM_EDIT_REDO 0x1116

/* ---- View ---- */
#define IDM_VIEW_GRID 0x1120
#define IDM_VIEW_STATUS 0x1121
#define IDM_VIEW_ZOOMIN 0x1122
#define IDM_VIEW_ZOOMOUT 0x1123
#define IDM_VIEW_ZOOMFIT 0x1124
#define IDM_VIEW_MINIMAP 0x1125

/* ---- Simulasi ---- */
#define IDM_SIM_RUN 0x1130
#define IDM_SIM_STOP 0x1131
#define IDM_SIM_RECOMPUTE 0x1132
#define IDM_SIM_RUN_TOGGLE 0x1133

/* ---- Bantu / Help ---- */
#define IDM_HELP_SHORTCUT 0x1140
#define IDM_HELP_ABOUT 0x1141

/* ---- View extended ---- */
#define IDM_VIEW_OSC 0x1150

/* ============================================================================
   BAGIAN 2 — Toolbar (palette tombol + scrollbar horizontal)

   Layout horizontal: [fixed kiri] [scrollable komponen] [fixed kanan aksi].
   Konstanta layout sudah di config.h (TOOLBAR_*). Di sini hanya API runtime.
   ============================================================================
 */

/* State scroll horizontal untuk area scrollable. */
extern int g_toolbar_scroll_x;

/* ---- Layout (dipakai oleh scrollbar & draw) ---- */
int toolbar_scroll_start_x(void); /* x awal area scrollable          */
int toolbar_scroll_end_x(int W);  /* x akhir area scrollable         */
int toolbar_max_scroll(int W);    /* max scroll offset (0 = no scroll) */
int toolbar_get_scroll_x(int W);  /* scroll_x yang sudah di-clamp    */
int toolbar_action_x(int action_idx, int W); /* x tombol aksi ke-N (0..3)  */

/* ---- Hit-test ---- */
/* Return idx tombol (0..BTN_COUNT-1) atau >= BTN_COUNT untuk aksi,
   atau -1 kalau tidak kena tombol apa pun. */
int toolbar_hit(int x, int y, int W);
int toolbar_tool_at(int x, int y, int W, int *is_clear);

/* ---- Drawing (dipanggil dari WM_PAINT) ---- */
void draw_toolbar(HDC hdc, int W);

/* ---- Wheel & scrollbar interaksi ---- */
int toolbar_scroll_wheel(int delta, int x, int y, int W);
int toolbar_scrollbar_hit(int x, int y, int W, int *is_thumb);
void toolbar_scrollbar_begin_drag(int x, int y, int W);
int toolbar_scrollbar_drag(int mx, int W);
void toolbar_scrollbar_end_drag(void);
int toolbar_scrollbar_dragging(void);
int toolbar_scrollbar_hover_thumb(int x, int y, int W);

/* ============================================================================
   BAGIAN 3 — Menu bar klasik (File/Edit/View/Simulasi/Bantu)
   ============================================================================
 */

/* Return index menu (0..4) di bawah kursor, atau -1. */
int menubar_hit(int x, int y);

/* Proses klik di menubar: buka popup, dispatch command ke menubar_command.
   Return command ID yang dieksekusi, atau 0 kalau user batal. */
int menubar_click(HWND hwnd, int x, int y);

/* Render menubar. Dipanggil internal oleh draw_toolbar — tetap public
   untuk fleksibilitas (mis. skin/custom menubar). */
void draw_menubar(HDC hdc, int W);

/* Dispatch IDM_* command ke handler. Dipanggil dari menubar_click,
   dari context menu kalau ID kebetulan IDM_*, atau dari accelerator
   table (main.c). */
void menubar_command(HWND hwnd, int cmd);

/* ============================================================================
   BAGIAN 4 — Canvas draw (simbol + wire + overlay)

   Dipanggil dari render_canvas_scene dan export_canvas_png.
   ============================================================================
 */

/* Bungkus fn() dengan transform rotasi komponen. Meng-komposisi dengan
   transform parent (zoom/pan) di DC — lihat komentar implementasi di
   src/ui/canvas/draw.c. */
void with_rotation(HDC hdc, Component *c, void (*fn)(HDC, int, int, BOOL));

/* ---- Simbol per komponen ---- */
void draw_k_resistor(HDC hdc, Component *c);
void draw_k_switch(HDC hdc, Component *c);
void draw_k_battery(HDC hdc, Component *c);
void draw_k_led(HDC hdc, Component *c);
void draw_k_ground(HDC hdc, Component *c);
void draw_k_capacitor(HDC hdc, Component *c);
void draw_k_transistor(HDC hdc, Component *c);
void draw_k_diode(HDC hdc, Component *c);
void draw_k_capolar(HDC hdc, Component *c);
void draw_k_acsource(HDC hdc, Component *c);
void draw_k_ldr(HDC hdc, Component *c);
void draw_k_buzzer(HDC hdc, Component *c);
void draw_k_voltmeter(HDC hdc, Component *c);
void draw_k_ammeter(HDC hdc, Component *c);
void draw_k_oscprobe(HDC hdc, Component *c);
void draw_k_inductor(HDC hdc, Component *c);
void draw_k_zener(HDC hdc, Component *c);
void draw_k_mosfet(HDC hdc, Component *c);
void draw_k_pot(HDC hdc, Component *c);
void draw_k_and(HDC hdc, Component *c);
void draw_k_or(HDC hdc, Component *c);
void draw_k_not(HDC hdc, Component *c);
void draw_k_nand(HDC hdc, Component *c);
void draw_k_nor(HDC hdc, Component *c);
void draw_k_xor(HDC hdc, Component *c);

/* ---- Komposit ---- */
/* Dispatcher: pilih draw_k_* berdasarkan c->type, tambahkan label nilai,
   outline seleksi (kalau selected), dan titik pin (kalau wiring aktif /
   seleksi / multi-select). */
void draw_component(HDC hdc, Component *c);

/* Gambar semua kabel + junction dot + preview wiring aktif. */
void draw_wires(HDC hdc);

/* Overlay seleksi (kotak putus-putus per komponen + bounding grup). */
void draw_selection_overlay(HDC hdc);

/* Marquee (kotak drag-select). */
void draw_marquee_overlay(HDC hdc);

/* Render seluruh scene: grid + wires + components + [overlay] + banner
   simulasi + short marker + minimap. Parameter include_ui_overlay=0
   untuk export PNG (tanpa seleksi/marquee/minimap).
   Dipanggil dari WM_PAINT (main.c) dan export_canvas_png(). */
void render_canvas_scene(HDC hdc, int W, int H, int include_ui_overlay);

/* ============================================================================
   BAGIAN 5 — Canvas event handlers

   Semua handler menerima WORLD coordinates (bukan screen). Konversi
   screen→world dilakukan di main.c sebelum memanggil handler ini
   (canvas_screen_to_canvas dari model.h).
   ============================================================================
 */

void on_lbutton_down(HWND hwnd, int x, int y);
void on_lbutton_up(HWND hwnd, int x, int y);
void on_rbutton_down(HWND hwnd, int x, int y);
void on_rbutton_dblclk(HWND hwnd, int x, int y);
void on_mouse_move(HWND hwnd, int x, int y);
void on_key_down(HWND hwnd, WPARAM key);
void on_lbutton_dblclk(HWND hwnd, int x, int y);

/* Tampilkan context menu untuk komponen (atau kosong = menu kanvas).
   `sx, sy` = screen coords (untuk TrackPopupMenu, BUKAN world coords). */
void show_context_menu(HWND hwnd, int sx, int sy, Component *c);

/* ============================================================================
   BAGIAN 6 — Minimap (panel overview canvas di pojok kanan-bawah)
   ============================================================================
 */

void minimap_render(HDC hdc, int canvas_w, int canvas_h);
int minimap_hit(int x, int y, int canvas_w, int canvas_h);
void minimap_click(int x, int y, int canvas_w, int canvas_h);
void minimap_begin_drag(int x, int y, int canvas_w, int canvas_h);
void minimap_drag(int x, int y, int canvas_w, int canvas_h);
void minimap_end_drag(void);
int minimap_dragging(void);

/* ============================================================================
   BAGIAN 7 — Panel osiloskop (top-level window terpisah)
   ============================================================================
 */

void osc_open(HWND hwnd_parent); /* buka / foreground-kan panel osiloskop */
void osc_refresh(void);          /* recompute trace + repaint (timer 5Hz) */
void osc_close(void);            /* destroy window osiloskop */
int osc_is_open(void);           /* 1 kalau window masih valid */

/* ============================================================================
   BAGIAN 8 — Splash screen (modal sampai timer habis / user klik)
   ============================================================================
 */

void splash_run(HINSTANCE hInst, HWND hwnd_main);

/* ============================================================================
   BAGIAN 9 — Dialog wrappers

   Fungsi-fungsi yang butuh Win32 UI (OPENFILENAMEW, MessageBoxW, GetDIBits)
   dan karenanya dipindah ke sini dari header pure (model.h / io.h).

   Implementasi fisik masih di file lama (model.c / fileio.c) — akan
   dipindah ke src/ui/dialogs/ & src/ui/canvas/ pada fase migrasi src.
   ============================================================================
 */

/* Edit nilai komponen: tampilkan modal dialog, update c->v via
   model_set_component_value(). */
void edit_value_dialog(HWND hwnd, Component *c);

/* Zoom berpusat di tengah viewport canvas (butuh GetClientRect). */
void canvas_zoom_center(HWND hwnd, double factor);

/* Dialog Save/Open/Export (file picker + pesan sukses/gagal). */
void model_save_dialog(HWND hwnd);
void model_load_dialog(HWND hwnd);
void model_export_png_dialog(HWND hwnd);

/* Render canvas ke bitmap lalu tulis PNG. Parameter `path` adalah file
   target. `err` diisi kalau return 0. `err_cap` = kapasitas buffer err. */
int export_canvas_png(HWND hwnd, const wchar_t *path, wchar_t *err,
                      int err_cap);




/* ============================================================================
   BAGIAN 10 — Window procedure

   Entry point dispatch WM_* untuk window utama (main.c mendaftarkan
   WndProc via WNDCLASSW.lpfnWndProc).
   ============================================================================
 */

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

   /* ============================================================================
   BAGIAN 10b — Window lifecycle (dipakai main.c wWinMain)

   g_main_hwnd di-set oleh main_window_create(). main.c menggunakannya
   untuk TranslateAcceleratorW di message loop.
   ============================================================================ */

/* Handle window utama. NULL sampai main_window_create() dipanggil. */
extern HWND g_main_hwnd;

/* Daftarkan window class + buat window utama (hidden). Dipanggil sekali
   oleh wWinMain sebelum splash_run(). Setelah splash selesai, window
   di-ShowWindow() oleh splash_run(). */
void main_window_create(HINSTANCE hInst);

#endif /* UI_H */