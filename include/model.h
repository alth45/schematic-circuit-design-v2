#ifndef MODEL_H
#define MODEL_H

/* ============================================================================
   model.h — API publik modul CORE/MODEL.

   Modul ini adalah otak data Schematic Editor:
     * Array komponen, kabel, junction.
     * Operasi edit: tambah/hapus/duplikasi/rotate/translate.
     * Seleksi tunggal + multi (blok).
     * Undo/redo berbasis snapshot (bounded 64 level).
     * Parser & formatter nilai komponen (satuan SI).
     * Transform viewport canvas (zoom & pan) — matematika murni,
       TIDAK menyentuh HWND (wrapper UI ada di ui.h).

   Aturan ketat:
     * HANYA #include "types.h" (dan transitif "config.h").
     * TIDAK BOLEH include <windows.h> — file ini harus bisa di-compile
       di Linux/macOS untuk unit test "core murni".
     * Tidak ada HDC/HWND/HINSTANCE di signature mana pun.

   Dua fungsi lama (edit_value_dialog, canvas_zoom_center) yang secara
   historis tinggal di model.c TIDAK dideklarasikan di sini karena butuh
   HWND — deklarasi mereka ada di ui.h. Implementasi fisik keduanya akan
   dipindah ke src/ui/ pada fase refactor berikutnya.

   Urutan bagian file ini mengikuti urutan konseptual (data → akses →
   modifikasi → undo), bukan urutan implementasi di model.c.
   ============================================================================
 */

#include "types.h"

/* ============================================================================
   BAGIAN 1 — State global model (definisi ada di src/core/model.c)

   Semua variabel di sini `extern` — hanya didefinisikan SEKALI di model.c.
   Siapa pun yang mau baca/tulis state model include header ini.

   Konvensi penamaan:
     comps[], comp_count      — array komponen & jumlahnya
     wires[], wire_count      — array kabel & jumlahnya
     junctions[], junction_count — array junction & jumlah slot terpakai
       (junction_count = jumlah entry dengan .used == 1, bukan max index)
   ============================================================================
 */

/* ---- Komponen ---- */
extern Component comps[MAX_COMP];
extern int comp_count;
extern int next_id; /* id berikutnya; selalu > id terbesar aktif */

/* ---- Mode edit aktif ---- */
extern ToolKind current_tool; /* tool yang sedang dipilih di toolbar */
extern int selected_id;       /* id komponen terpilih tunggal, -1 = none */
extern int dragging_id;       /* id komponen yang sedang di-drag, -1 = none */
extern int drag_dx, drag_dy;  /* offset klik-ke-origin saat drag tunggal */

/* ---- Kabel ---- */
extern Wire wires[MAX_WIRES];
extern int wire_count;

/* ---- Junction (T-junction wire-to-wire) ---- */
extern Junction junctions[MAX_JUNCTIONS];
extern int junction_count; /* jumlah slot dengan .used == 1 */

/* ---- State wiring aktif (sedang menarik kabel) ---- */
extern int wiring_active;        /* 1 = preview kabel sedang digambar */
extern PinRef wiring_from;       /* endpoint asal (bisa pin atau junction) */
extern int wiring_mx, wiring_my; /* posisi kursor terkini (world coords) */

/* ---- Multi-select / blok rangkaian ---- */
extern int selection[MAX_SEL]; /* array id komponen terpilih */
extern int sel_count;          /* jumlah entry valid di selection[] */
extern int box_selecting;      /* 1 = marquee kotak sedang digambar */
extern int box_x1, box_y1;     /* pojok marquee awal (world) */
extern int box_x2, box_y2;     /* pojok marquee terkini (world) */

/* ---- Drag grup (multi-select ikut bergerak) ---- */
extern int group_dragging; /* 1 = sedang drag grup */
extern GroupDragItem group_drag_items[MAX_GROUP_DRAG];
extern int group_drag_n;  /* jumlah item aktif di group_drag_items */
extern int group_drag_ox; /* offset origin (anchor) X */
extern int group_drag_oy; /* offset origin (anchor) Y */

/* ---- Preferensi tampilan (bisa diubah dari menu View) ---- */
extern int g_show_grid;    /* tampilkan grid di canvas (default 1) */
extern int g_show_status;  /* tampilkan status bar bawah (default 1) */
extern int g_show_minimap; /* tampilkan panel minimap (default 1) */
extern int g_minimap_mode; /* 1 = sedang render minimap (matikan label) */

/* ---- Zoom & pan canvas (transform world↔screen) ---- */
extern double g_zoom; /* skala, 1.0 = 100%, range 0.25..8.0 */
extern int g_pan_x;   /* offset pan horizontal (world unit) */
extern int g_pan_y;   /* offset pan vertikal (world unit) */

/* ---- Tabel tombol toolbar (definisi di model.c) ----
   btnTools[i] = ToolKind tombol ke-i (urutan sesuai layout toolbar).
   btnLabels[i] = tooltip/label panjang (tidak dipakai untuk render tombol;
                  label pendek via toolbar_tool_shortname() di ui). */
extern const ToolKind btnTools[BTN_COUNT];
extern const wchar_t *btnLabels[BTN_COUNT];

/* ============================================================================
   BAGIAN 1b — Status message (cross-cutting notification)

   Status message adalah "satu baris pesan" yang ditampilkan status bar bawah.
   Dipanggil dari core (setelah edit), io (setelah save/load), dan ui
   (setelah interaksi user). Implementasi di core/model.c — pure, tidak
   butuh windows.h.
   ============================================================================
 */

/* Buffer status global — 256 wchar_t, null-terminated, selalu valid.
   UI (main.c WM_PAINT) hanya membaca buffer ini untuk render status bar. */
extern wchar_t g_status[256];

/* Set isi g_status dengan printf-style format.
   Contoh: set_status(L"Saklar #%d: %ls", c->id, c->on ? L"ON" : L"OFF");
   Aman dipanggil dari mana saja (single-threaded UI). */
void set_status(const wchar_t *fmt, ...);

/* ============================================================================
   BAGIAN 2 — Utilitas geometri & pembacaan komponen
   ============================================================================
 */

/* Snap nilai ke kelipatan GRID (pembulatan terdekat). */
int snap(int v);

/* Cari komponen berdasarkan id. Return NULL kalau tidak ketemu. */
Component *find_comp_by_id(int id);

/* Cari komponen teratas (z-order terakhir = paling atas) di titik (x,y).
   Menggunakan bounding box komponen. Return NULL kalau tidak ada. */
Component *find_comp_at(int x, int y);

/* Jumlah pin untuk tipe komponen. GROUND=1; transistor/MOSFET/POT=3;
   gate selain NOT=3; NOT dan komponen 2-pin lain=2. */
int comp_pin_count(const Component *c);

/* Validasi index pin terhadap comp_pin_count(c). */
int comp_pin_valid(const Component *c, int pin_idx);

/* Posisi pin `pin_idx` di world coords (memperhitungkan rotasi). */
void comp_pin_pos(const Component *c, int pin_idx, int *px, int *py);

/* Konversi koordinat lokal komponen (0,0 = pusat) → world coords
   setelah rotasi. */
void comp_local_to_world(const Component *c, int lx, int ly, int *wx, int *wy);

/* Setengah-lebar & setengah-tinggi bounding box komponen (sudah
   memperhitungkan rotasi ganjil → tukar w/h). */
void comp_bounding_half(const Component *c, int *hw, int *hh);

/* ============================================================================
   BAGIAN 3 — Hit-test pin / junction / wire
   ============================================================================
 */

/* Cari pin ATAU junction terdekat dalam radius toleransi klik. Kalau
   ketemu: isi *out (PinRef), *px/*py (posisi world endpoint). Return 1. */
int find_pin_at(int x, int y, PinRef *out, int *px, int *py);

/* Posisi endpoint PinRef di world coords.
   - comp_id >= 0 : pin komponen
   - comp_id < 0  : junction (pin_idx = id junction)
   Return (0,0) bila endpoint tidak valid. */
void pinref_pos(PinRef p, int *x, int *y);

/* Cari wire yang interior-nya terdekat dengan (x,y) untuk T-junction.
   Titik ujung wire tidak dianggap interior. Isi *out_wi (index wire),
   *out_jx/*out_jy (titik terdekat di segmen). Return 1 bila ketemu. */
int find_wire_segment_at(int x, int y, int *out_wi, int *out_jx, int *out_jy);

/* Cari junction by id (0..MAX_JUNCTIONS-1). Return NULL kalau slot
   tidak terpakai (.used == 0) atau id di luar range. */
Junction *find_junction_by_id(int id);

/* ============================================================================
   BAGIAN 4 — Operasi edit dasar
   ============================================================================
 */

/* Tambah komponen baru di posisi (x,y) — posisi di-snap ke grid.
   Auto-assign id = next_id++, dan set selected_id ke id baru.
   No-op kalau comp_count >= MAX_COMP atau sim_running != 0. */
void model_add_component(CompType type, int x, int y);

/* Hapus komponen / wire / junction di titik (x,y) dengan prioritas:
   1. junction (radius 10 px) → hapus junction + wire yang merujuk
   2. wire (toleransi WIRE_HIT)
   3. komponen (bounding box)
   Idempotent: kalau tidak ada apa pun di titik itu, tidak melakukan apa-apa. */
void model_remove_at(int x, int y);

/* Kosongkan seluruh canvas (komponen + wire + junction).
   Reset semua state seleksi/drag/box. Push undo snapshot sebelum hapus. */
void model_clear_all(void);

/* Rotate komponen terpilih 90° CW. No-op kalau tidak ada seleksi. */
void model_rotate_selected(void);

/* Tambah kabel antara dua endpoint (pin atau junction).
   Validasi: endpoint valid, bukan self-loop, belum ada duplikat.
   Return 1 kalau berhasil, 0 kalau ditolak. */
int model_add_wire(PinRef a, PinRef b);

/* Hapus wire terdekat dengan (x,y) dalam toleransi WIRE_HIT, sekaligus
   bersihkan wire "orphan" (endpoint komponen/junction sudah hilang).
   Return 1 kalau ada yang terhapus. */
int model_remove_wire_at(int x, int y);

/* Set nilai numerik komponen (via dialog value edit).
   Push undo snapshot kalau nilai benar-benar berubah. */
void model_set_component_value(int comp_id, double v);

/* ============================================================================
   BAGIAN 5 — Junction (titik sambung T)
   ============================================================================
 */

/* Tambah junction di (x,y). Kalau sudah ada junction di titik sama, return
   id yang sudah ada (idempotent). Return -1 kalau array penuh. */
int model_add_junction(int x, int y);

/* Hapus junction by id. Semua wire yang merujuk junction ini ikut dihapus. */
void model_remove_junction(int id);

/* ============================================================================
   BAGIAN 6 — Seleksi tunggal & multi
   ============================================================================
 */

/* Kosongkan seleksi multi + selected_id. */
void clear_selection(void);

/* Tambah id ke selection[] kalau belum ada (dan komponen masih valid).
   Set selected_id = id. Return 1 kalau benar-benar ditambah. */
int add_to_selection(int id);

/* Toggle id di selection[] (ada → hapus, tidak ada → tambah).
   selected_id mengikuti entry terakhir di selection[]. */
void toggle_selection(int id);

/* Pilih semua komponen dalam kotak (x1,y1)-(x2,y2) — koordinat world.
   `add != 0` = tambahkan ke seleksi existing (Shift-drag);
   `add == 0` = ganti seleksi total.
   Return jumlah komponen yang terpilih oleh operasi ini. */
int model_select_rect(int x1, int y1, int x2, int y2, int add);

/* Geser seluruh komponen terpilih sejauh (dx,dy) — push undo sekali. */
void model_translate_selection(int dx, int dy);

/* Hapus semua komponen dalam selection[] (dan wire yang merujuk).
   Undo di-batch jadi satu langkah. Return jumlah komponen terhapus. */
int model_delete_selection(void);

/* Duplikasi seluruh komponen terpilih sejauh (dx,dy). Wire yang
   menghubungkan dua komponen terpilih ikut diduplikasi (memetakan id
   lama → id baru). Return jumlah komponen yang berhasil diduplikasi. */
int model_duplicate_selection(int dx, int dy);

/* ============================================================================
   BAGIAN 7 — Undo / redo (snapshot stack)
   ============================================================================
 */

/* Ambil snapshot state saat ini → push ke undo stack, kosongkan redo stack.
   No-op kalau sim_running != 0 atau sedang di dalam batch (selain push
   pertama). */
void undo_push(void);

/* Undo langkah terakhir. Return 1 kalau ada yang di-undo.
   No-op saat sim_running. */
int undo_undo(void);

/* Redo langkah terakhir yang di-undo. Return 1 kalau ada yang di-redo.
   No-op saat sim_running. */
int undo_redo(void);

/* Query ketersediaan undo/redo (untuk enable/disable menu item). */
int undo_can_undo(void);
int undo_can_redo(void);

/* Buang seluruh riwayat undo/redo (mis. setelah load file baru). */
void undo_reset(void);

/* Grup beberapa operasi edit jadi SATU langkah undo.
   Harus dipanggil berpasangan (begin ... end). Snapshot hanya diambil
   pada undo_push() PERTAMA di dalam batch. */
void undo_begin_batch(void);
void undo_end_batch(void);

/* ============================================================================
   BAGIAN 8 — Nilai komponen & parser teks
   ============================================================================
 */

/* Nilai default (SI) per tipe komponen saat pertama kali diletakkan. */
double comp_default_value(CompType t);

/* 1 kalau tipe komponen punya nilai editable (v), 0 kalau tidak. */
int comp_has_value(CompType t);

/* Satuan SI untuk label UI ("Ω", "V", "F", dst) atau NULL kalau tidak
   punya nilai. Return string statis — jangan di-free. */
const wchar_t *comp_value_unit(CompType t);

/* Parser teks nilai dengan sufiks SI:
     "220"   → 220
     "4.7k"  → 4700
     "1M"    → 1e6
     "100n"  → 100e-9
     "10uF"  → 10e-6
     "5.1Vz" → 5.1
   Return 1 sukses. Return 0 kalau format tidak valid. */
int parse_value_text(const wchar_t *s, double *out);

/* Sama dengan parse_value_text, tapi input ASCII (untuk parser JSON). */
int parse_value_text_a(const char *s, double *out);

/* Format nilai ke teks dengan prefix SI. Contoh:
     format_value(4700, L"Ω", buf, cap) → L"4.7k Ω"
     format_value(100e-9, L"F", buf, cap) → L"100n F"
   `unit` boleh NULL (hanya angka + prefix). */
void format_value(double v, const wchar_t *unit, wchar_t *out, int cap);

/* ============================================================================
   BAGIAN 9 — Transform viewport (zoom & pan)

   Semua fungsi di sini murni matematika — tidak menyentuh HWND. Wrapper
   UI (mis. canvas_zoom_center yang butuh GetClientRect) ada di ui.h.
   ============================================================================
 */

/* Konversi koordinat screen (relatif ke pojok kiri-atas CANVAS, bukan
   window) → koordinat world canvas. */
void canvas_screen_to_canvas(int sx, int sy, int *cx, int *cy);

/* Kebalikan dari canvas_screen_to_canvas. */
void canvas_canvas_to_screen(int cx, int cy, int *sx, int *sy);

/* Zoom dengan faktor `factor` (>1 = perbesar, <1 = perkecil) dengan
   titik acuan (sx,sy) tetap di posisi layar yang sama.
   Clamp zoom ke range [0.25, 8.0]. */
void canvas_zoom_at(int sx, int sy, double factor);

/* Reset zoom ke 1.0 dan pan ke (0,0). */
void canvas_zoom_reset(void);

/* Pilih zoom & pan otomatis agar seluruh komponen muat di viewport
   berukuran view_w × view_h (dengan margin 40 px). */
void canvas_zoom_fit(int view_w, int view_h);

#endif /* MODEL_H */

/* ============================================================================
   Test-only hooks
   ============================================================================
 */
#ifdef SCHEMATIC_TEST_STUBS
/* Reset seluruh state model ke kondisi kosong untuk unit test.
   Didefinisikan di src/core/model.c di dalam #ifdef SCHEMATIC_TEST_STUBS.
   TIDAK ada di build produksi (compiler mengeluarkan simbol ini sepenuhnya).
   Implementasi harus reset: comps[], wires[], junctions[], selection[],
   semua counter, next_id=1, dan clear undo/redo stack. */
void test_reset_model(void);
#endif