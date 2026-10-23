#ifndef IO_H
#define IO_H

/* ============================================================================
   io.h — API publik modul IO (PERSISTENSI PURITY).

   Modul ini menangani serialisasi / deserialisasi state Schematic Editor
   ke/ dari media eksternal (file JSON, file PNG) — TANPA menyentuh Win32 UI.

   Aturan ketat:
     * HANYA #include "types.h" + <stddef.h> (untuk wchar_t & size_t).
     * TIDAK BOLEH include <windows.h>.
     * TIDAK BOLEH ada HWND/HDC/OPENFILENAME di signature mana pun.
     * Semua parameter murni tipe data (int, wchar_t*, unsigned char*).

   Konsekuensi: modul ini bisa di-compile & diuji di Linux (mis. untuk
   validasi png_writer yang memakai CRC/Adler manual), dan pemakainya
   (core/model.c, core/sim.c) TIDAK ikut menyeret <windows.h>.

   Fungsi yang butuh Win32 UI (dialog Save/Open/Export) SENGAJA tidak
   dideklarasikan di sini — deklarasinya ada di ui.h. Itu mencakup:
     * model_save_dialog(HWND)
     * model_load_dialog(HWND)
     * model_export_png_dialog(HWND)
     * export_canvas_png(HWND, ...)   ← butuh HDC untuk render → PNG

   Catatan tipe wchar_t:
     * C99 mendefinisikan wchar_t via <stddef.h>. Pakai itu untuk portabilitas
       maksimum. Jangan include <wchar.h> penuh hanya untuk satu typedef.
   ============================================================================
 */

#include "types.h"
#include <stddef.h> /* wchar_t, size_t */


/* ============================================================================
   BAGIAN 1 — Mapping nama tipe komponen ↔ string JSON

   Diperlukan saat serialisasi/deserialisasi. Strings di sini HARUS cocok
   dengan literals yang disimpan fileio.c, contoh:
     COMP_RESISTOR  ↔ "resistor"
     COMP_ACSOURCE  ↔ "acsource"
     COMP_MOSFET    ↔ "mosfet"
     COMP_POT       ↔ "potentiometer"

   Pemanggil (fileio.c) memakai ini untuk konversi dua arah. Nilai return
   adalah pointer ke string statis internal — jangan di-free, jangan
   dimodifikasi.
   ============================================================================
 */

/* Nama JSON untuk tipe komponen. Selalu return non-NULL ("none" untuk
   COMP_NONE). */
const char *comp_type_json_name(CompType t);

/* Kebalikan: parse nama JSON → CompType. Return COMP_NONE kalau tidak
   dikenali. Toleran terhadap alias umum: "ACSource"/"ac"/"ac_source",
   "NPN"/"Q", "MOSFET"/"FET", dll. */
int comp_type_from_json_name(const char *s);

/* ============================================================================
   BAGIAN 2 — Save / Load JSON (state skematik lengkap)

   Format v1 (lihat docs/ARCHITECTURE.md untuk skema lengkap):
     {
       "app": "schematic-c",
       "version": 1,
       "components": [ {"id":1, "type":"resistor", "x":100, ...}, ... ],
       "junctions":  [ {"id":0, "x":200, "y":100}, ... ],
       "wires":      [ {"a":{"id":1,"pin":0}, "b":{"j":3}}, ... ]
     }

   Endpoint wire bisa berupa pin komponen ("id"/"pin") atau junction ("j").
   Lihat PinRef di types.h.

   Validasi saat load: endpoint wire yang tidak valid di-drop silently
   (komponen/junction yang direferensi hilang → wire orphan dibuang).
   ============================================================================
 */

/* Tulis seluruh state model ke file JSON di `path`.
   - `path`  : jalur file target (UTF-16, karena pakai _wfopen)
   - `err`   : buffer pesan error (diisi kalau return 0). Boleh NULL.
   - `err_cap` : kapasitas buffer err (dalam wchar_t). Diabaikan kalau err=NULL.
   Return 1 sukses, 0 gagal (I/O error / buffer tidak cukup / dsb).

   Efek samping: TIDAK mengubah state apapun — murni baca-tulis. */
int model_save_json(const wchar_t *path, wchar_t *err, int err_cap);

/* Baca file JSON di `path` dan GANTI state model saat ini (atomic commit:
   parse dulu ke buffer sementara, baru copy ke state global kalau sukses
   penuh — jadi file JSON rusak TIDAK merusak state yang sedang aktif).

   Setelah load sukses:
     * next_id, comp_count, wire_count, junction_count di-reset dari file.
     * selection kosong, selected_id = -1, dragging_id = -1, wiring = 0.
     * undo_reset() dipanggil (riwayat undo lama tidak relevan).
     * sim_recompute() dipanggil untuk refresh state simulasi.

   Return 1 sukses, 0 gagal (file tidak ada / JSON rusak / versi tak
   didukung). Kalau gagal, state model tetap seperti sebelum load. */
int model_load_json(const wchar_t *path, wchar_t *err, int err_cap);

/* ============================================================================
   BAGIAN 3 — PNG writer (self-contained, tanpa libpng/zlib/GDI+)

   Menulis file PNG dari buffer RGB 24-bit top-down yang sudah jadi.
   Implementasi (png_writer.c) memakai teknik "deflate stored blocks"
   sehingga file lebih besar dari kompresi optimum, tapi 100% valid dan
   tidak butuh dependensi apa pun.

   Berguna untuk: export canvas (dipanggil oleh export_canvas_png di ui.h),
   snapshot test, dokumentasi.
   ============================================================================
 */

/* Tulis PNG RGB ke `path`.
   - `width`, `height` : dimensi piksel, harus > 0
   - `rgb`             : buffer W×H×3 byte (R,G,B per piksel, tanpa padding)
   Return 1 sukses, 0 gagal (file I/O / dimensi invalid / alokasi gagal).

   Buffer `rgb` dimiliki pemanggil — fungsi ini TIDAK membebaskannya. */
int png_write_rgb(const wchar_t *path, int width, int height,
                  const unsigned char *rgb);

#endif /* IO_H */