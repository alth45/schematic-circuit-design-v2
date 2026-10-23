#ifndef TYPES_H
#define TYPES_H

/* ============================================================================
   types.h — Tipe data murni Schematic Editor.

   File ini berisi enum dan struct yang menjadi kosakata bersama seluruh
   modul (core, io, ui, components, tests).

   Aturan ketat:
     * HANYA #include "config.h" (untuk batas array MAX_*).
     * TIDAK BOLEH include <windows.h> atau header platform lain.
     * TIDAK BOLEH ada extern, fungsi, atau #define konstanta baru
       (konstanta tetap di config.h).

   Konsekuensi: header ini bisa di-compile di Linux/macOS tanpa MinGW,
   sehingga unit test "core murni" tidak butuh toolchain Win32.

   Urutan deklarasi PENTING — tipe yang dipakai harus dideklarasikan lebih
   dulu. Urutan di file ini sudah final: jangan acak saat edit.
   ============================================================================
 */

#include "config.h"

/* ===================== 1. Jenis komponen ===================== */
/* Semua simbol yang bisa diletakkan di canvas. Nilai enum SANGAT STABIL —
   jangan ubah urutan tanpa migrasi file JSON (fileio.c menyimpan string,
   bukan angka, tapi urutan memengaruhi switch-case di mana-mana). */
typedef enum {
  COMP_NONE = 0, /* placeholder / sentinel                    */

  /* --- komponen dasar --- */
  COMP_RESISTOR,   /* resistor 2-pin: pin0=kiri, pin1=kanan     */
  COMP_SWITCH,     /* saklar 2-pin: pin0=kiri, pin1=kanan       */
  COMP_BATTERY,    /* baterai DC: pin0=(+), pin1=(−)            */
  COMP_LED,        /* LED: pin0=anoda, pin1=katoda              */
  COMP_GROUND,     /* ground 1-pin                              */
  COMP_CAPACITOR,  /* kapasitor non-polar                       */
  COMP_TRANSISTOR, /* NPN: pin0=C(kanan-atas), pin1=E(kanan-bawah), pin2=B(kiri)
                    */
  COMP_DIODE,      /* dioda: pin0=A(kiri), pin1=K(kanan)         */
  COMP_CAPOLAR,    /* kapasitor polar: pin0=(+) kiri, pin1=(−) kanan */

  /* --- sumber & sensor --- */
  COMP_ACSOURCE, /* pin0=Live, pin1=Netral; v=VAC RMS; wave=SINE/SQUARE/TRIANGLE
                  */
  COMP_LDR, /* fotoresistor: pin0=kiri, pin1=kanan; v=R_gelap(Ω); on=1 berarti
               TERANG */
  COMP_BUZZER, /* buzzer: pin0=(+), pin1=gnd; v=frekuensi(Hz)  */

  /* --- alat ukur --- */
  COMP_VOLTMETER, /* pin0=(+), pin1=(−); ideal (impedansi tak hingga) */
  COMP_AMMETER,   /* pin0=masuk, pin1=keluar; ideal (hambatan ~0)     */
  COMP_OSCPROBE, /* pin0=sinyal, pin1=referensi; ideal (impedansi tak hingga) */

  /* --- komponen reaktif & lanjutan --- */
  COMP_INDUCTOR, /* pin0=kiri, pin1=kanan; DC: short, AC: XL=2πfL; v=Henry */
  COMP_ZENER, /* pin0=A(kiri), pin1=K(kanan); forward Vf≈0.7, reverse breakdown
                 @ v (Vz) */
  COMP_MOSFET, /* N-ch enhancement: pin0=D(kanan-atas), pin1=S(kanan-bawah),
                  pin2=G(kiri); v=Vth */
  COMP_POT, /* potensiometer 3-pin: pin0=A(kiri), pin1=B(kanan), pin2=W(atas);
               R_A = v·on%, R_B = v·(100−on)%; on = posisi wiper 0..100 */

  /* --- gerbang logika --- */
  COMP_AND, /* pin0=A, pin1=B, pin2=OUT */
  COMP_OR,
  COMP_NOT, /* pin0=IN, pin1=OUT */
  COMP_NAND,
  COMP_NOR,
  COMP_XOR
} CompType;

/* ===================== 2. Tool aktif (toolbar) ===================== */
/* Urutan di sini dipakai btnTools[] di core/model.c — jangan ubah urutan
   tanpa mengubah array tersebut. Nilai pertama TOOL_SELECT = 0 wajib
   karena current_tool diinisialisasi ke 0. */
typedef enum {
  /* --- 4 tool fix kiri --- */
  TOOL_SELECT = 0,
  TOOL_BOX,  /* multi-select / blok rangkaian  */
  TOOL_WIRE, /* tarik kabel antar pin          */
  TOOL_DELETE,

  /* --- 25 tool komponen scrollable (indeks sejajar CompType) --- */
  TOOL_RESISTOR,
  TOOL_SWITCH,
  TOOL_BATTERY,
  TOOL_ACSOURCE,
  TOOL_LED,
  TOOL_GROUND,
  TOOL_CAPACITOR,
  TOOL_TRANSISTOR,
  TOOL_DIODE,
  TOOL_CAPOLAR,
  TOOL_LDR,
  TOOL_BUZZER,
  TOOL_VOLTMETER,
  TOOL_AMMETER,
  TOOL_OSCPROBE,
  TOOL_INDUCTOR,
  TOOL_ZENER,
  TOOL_MOSFET,
  TOOL_POT,
  TOOL_AND,
  TOOL_OR,
  TOOL_NOT,
  TOOL_NAND,
  TOOL_NOR,
  TOOL_XOR
} ToolKind;

/* ===================== 3. Bentuk gelombang sumber AC ===================== */
typedef enum { WAVE_SINE = 0, WAVE_SQUARE, WAVE_TRIANGLE } WaveKind;

/* ===================== 4. Junction (titik sambung wire-to-wire)
 * ===================== */
/* Junction adalah node virtual di canvas, BUKAN komponen. Dipakai saat
   user menarik kabel ke tengah kabel lain (T-junction). Junction boleh
   jadi endpoint wire (lihat PinRef.comp_id < 0). */
typedef struct {
  int x, y;
  int used; /* 0 = slot kosong, 1 = dipakai. Semua array junction
               diiterasi penuh dan dicek flag ini — inilah
               sumber "bisa hapus + tambah tanpa re-index". */
} Junction;

/* ===================== 5. Komponen di canvas ===================== */
/* Satu instance komponen. Field yang WAJIB dipertahankan saat duplikasi /
   undo-restore: type, x, y, rot, on, v, wave, led_color. Field `id` harus
   unik di seluruh comps[] — itu tanggung jawab model_add_component().

   Ukuran struct ~= 48 byte. Dengan MAX_COMP=256, array comps[] makan
   ~12 KB. Ini sengaja kecil supaya snapshot undo (64 level) = ~1.5 MB
   masih nyaman. JANGAN tambah field besar di sini tanpa menghitung ulang. */
typedef struct {
  CompType type;

  /* --- posisi di world-space (snap ke grid) --- */
  int x, y;

  /* --- identitas --- */
  int id; /* unik, mulai 1; 0/-1 = invalid */

  /* --- orientasi & mode --- */
  int rot; /* 0..3 = 0°/90°/180°/270° CW               */
  int on;  /* multiperan:
              SWITCH    : 0/1 (buka/tutup)
              LDR       : 0/1 (gelap/terang)
              POT       : 0..100 (posisi wiper %)
              lain      : 0 tak bermakna                */

  /* --- nilai SI --- */
  double v; /* satuan per CompType, lihat comp_value_unit():
               RESISTOR, LDR(dark), POT     → Ω
               BATTERY, ACSOURCE (RMS), LED, DIODE, ZENER → V
               CAPACITOR, CAPOLAR           → F
               INDUCTOR                     → H
               BUZZER                       → Hz
               MOSFET                       → Vth            */

  /* --- AC source: bentuk gelombang --- */
  WaveKind wave; /* hanya relevan untuk COMP_ACSOURCE; tipe lain
                    default WAVE_SINE dan diabaikan               */

  /* --- LED: warna (0=merah, 1=hijau, 2=kuning, 3=biru) --- */
  int led_color; /* hanya relevan untuk COMP_LED                  */
} Component;

/* ===================== 6. Referensi pin / junction ===================== */
/* Endpoint wire bisa berupa:
     comp_id >= 0  →  pin komponen (pin_idx = 0..2 sesuai comp_pin_count)
     comp_id < 0   →  junction       (pin_idx = id junction 0..MAX_JUNCTIONS-1)

   Konvensi tanda ini SANGAT PENTING — dipakai di pinref_pos(), sim.c,
   fileio.c, events.c. Jangan ganti jadi misal `type` enum terpisah tanpa
   refactor menyeluruh. */
typedef struct {
  int comp_id; /* >=0: id komponen; <0: junction marker */
  int pin_idx; /* 0..N untuk pin; id junction untuk junction */
} PinRef;

/* ===================== 7. Kabel ===================== */
/* Satu kabel = satu segmen ortho (L-shape: horizontal dulu, lalu vertikal).
   Rute tidak disimpan — dihitung on-the-fly oleh stroke_ortho() /
   find_wire_segment_at() berdasarkan posisi dua endpoint saat ini. */
typedef struct {
  PinRef a, b;
} Wire;

/* ===================== 8. Item drag grup ===================== */
/* Selama user men-drag blok multi-select, kita butuh posisi awal tiap
   komponen untuk menghitung delta yang konsisten. Array ini diisi di
   begin_group_drag() (events.c) dan dikonsumsi di on_mouse_move(). */
typedef struct {
  int idx;    /* index ke comps[] — TIDAK boleh pakai id!
                 (index lebih cepat; blok tidak boleh dihapus
                 saat drag berlangsung)                      */
  int x0, y0; /* posisi awal sebelum drag                    */
} GroupDragItem;

#endif /* TYPES_H */