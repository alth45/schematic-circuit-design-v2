#ifndef CONFIG_H
#define CONFIG_H

/* ============================================================================
   config.h — Konstanta global Schematic Editor.

   File ini adalah AKAR dependency tree. Aturan ketat:
     * TIDAK BOLEH ada #include apa pun (tidak ada windows.h, math.h, dll).
     * TIDAK BOLEH ada typedef / struct / extern / fungsi.
     * HANYA #define konstanta numerik murni.

   Konsekuensi: file ini bisa di-include dari mana saja (core murni, unit
   test Linux, UI Win32) tanpa menarik dependensi platform.

   Konstanta dikelompokkan berdasarkan domain:
     1. Kapasitas array
     2. Grid & geometri canvas
     3. Layout UI (menubar, toolbar, status bar)
     4. Index tombol aksi tetap di toolbar
     5. Nilai default komponen (satuan SI)
   ============================================================================
 */

/* ===================== 1. Kapasitas array ===================== */
/* Semua struct berukuran tetap — tidak ada malloc untuk array utama,
   supaya state undo/redo bisa di-copy dengan mudah (memcpy snapshot). */
#define MAX_COMP 256       /* jumlah maksimum komponen di canvas */
#define MAX_WIRES 512      /* jumlah maksimum kabel */
#define MAX_JUNCTIONS 128  /* junction = titik sambung wire-to-wire  */
#define MAX_SEL 256        /* batas seleksi multi (blok)             */
#define MAX_GROUP_DRAG 256 /* batas item yang ikut drag grup         */
#define BTN_COUNT 29       /* tombol palette: 4 fix + 25 komponen    */

/* Ukuran ruang node union-find di sim.c:
   3 pin per komponen (transistor/MOSFET/pot butuh 3, sisanya pakai subset)
   + junction sebagai node virtual. */
#define NODE_SPACE (MAX_COMP * 3 + MAX_JUNCTIONS)

/* ===================== 2. Grid & geometri canvas ===================== */
#define GRID 20        /* jarak snap grid (px)                   */
#define PIN_R 7        /* radius gambar pin komponen (px)        */
#define WIRE_HIT 8     /* toleransi hit-test kabel (px)          */
#define BOX_DRAG_MIN 4 /* threshold sebelum box-select aktif     */

/* ===================== 3. Layout UI (fixed height bands) =====================
 */
/* Tinggi vertikal panel top-down: menubar → palette → canvas → statusbar. */
#define MENUBAR_H 30 /* baris menu File/Edit/View/Simulasi/Bantu */
#define TOOLBAR_H 94 /* total area UI atas = menubar + palette   */
#define PALETTE_H 64 /* strip tombol komponen di bawah menubar   */
#define STATUS_H 26  /* baris status paling bawah                */

/* Dimensi satu tombol toolbar. */
#define TOOLBAR_BTN_W 46
#define TOOLBAR_BTN_H 46
#define TOOLBAR_BTN_GAP 3

/* Pembagian area toolbar horizontal:
     [4 fixed kiri] [N scrollable komponen] [4 fixed kanan aksi]
   Fixed-left  = Select, Blok, Wire, Delete
   Fixed-right = Clear, Save, Open, Run                            */
#define TOOLBAR_FIXED_LEFT_COUNT 4
#define TOOLBAR_FIXED_RIGHT_COUNT 4
#define TOOLBAR_SCROLL_COUNT (BTN_COUNT - TOOLBAR_FIXED_LEFT_COUNT)

/* Index pseudo-tombol untuk toolbar_hit() hasil return.
   Nilai asli BTN_COUNT..BTN_COUNT+3 adalah tombol aksi kanan.
   Caller membedakan: idx >= BTN_COUNT => aksi, bukan tool komponen. */
#define CLEAR_BTN_INDEX BTN_COUNT
#define SAVE_BTN_INDEX (BTN_COUNT + 1)
#define OPEN_BTN_INDEX (BTN_COUNT + 2)
#define RUN_BTN_INDEX (BTN_COUNT + 3)

/* ===================== 4. Nilai default komponen (SI) ===================== */
/* Dipakai oleh comp_default_value() di core/model.c, dan fallback di sim.c
   bila user mengosongkan nilai komponen.                                */

/* LED & dioda */
#define VF_LED_DEFAULT 2.0 /* forward voltage LED default (V)     */
#define VF_LED_MAX 20e-3   /* arus aman maksimum LED (A) = 20 mA  */
#define VF_DIODE 0.7       /* forward voltage dioda silikon (V)   */
#define V_EXPLODE 50.0     /* V sumber > ini + overcur → meledak  */
#define I_LEAK 1e-9        /* arus bocor dioda reverse (A)        */

/* LDR (fotoresistor) */
#define LDR_DARK_DEFAULT 1e6 /* R saat gelap (Ω) = 1 MΩ             */
#define LDR_LIGHT_RATIO 0.01 /* R terang = 1% R gelap               */

/* Buzzer */
#define BUZZER_FREQ_DEFAULT 1000.0 /* frekuensi default (Hz)              */

/* Induktor */
#define IND_DEFAULT_H 100e-6 /* induktansi default (H) = 100 µH     */

/* Frekuensi jala-jala listrik (grid). Dipakai di core/sim.c untuk
   menghitung reaktansi AC (XL = 2πfL, XC = 1/(2πfC)) dan untuk
   sim_ac_voltage(). Sebelumnya konstanta ini terdefinisi DUA KALI
   secara terpisah: `AC_FREQ_HZ` di sim.c dan `OSC_FREQ_HZ` di
   oscilloscope.c — berisiko drift. Sekarang jadi single source of truth. */
#define AC_MAINS_FREQ_HZ 50.0 /* frekuensi grid (Hz) — Indonesia  */

/* Zener */
#define VZENER_DEFAULT 5.1 /* tegangan breakdown default (V)      */

/* MOSFET */
#define MOSFET_VTH_DEFAULT 2.0 /* threshold gate default (V)          */

/* Potensiometer */
#define POT_R_DEFAULT 10000.0 /* resistansi total default (Ω) = 10k  */
#define POT_WIPER_DEFAULT 50  /* posisi wiper default (%) = 50       */

/* ===================== 5. Format string nilai ===================== */
#define VALUE_STR_CAP 48 /* kapasitas buffer teks nilai (char)  */

#endif /* CONFIG_H */