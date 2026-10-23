#ifndef SIM_H
#define SIM_H

/* ============================================================================
   sim.h — API publik modul SIMULASI (analisis nodal).

   Modul ini adalah "mesin fisika" Schematic Editor:
     * Analisis nodal (MNA-lite) pada graf komponen + kabel.
     * Union-Find untuk node ideal-0Ω (saklar ON, dioda forward, dll).
     * Model per-komponen: resistor, LED, zener, transistor, MOSFET, gate,
       kapasitor (AC), induktor (AC), LDR, buzzer, potensiometer.
     * Perhitungan Vth / Rth per LED (hukum Thevenin).
     * Domain-waktu superposisi untuk probe osiloskop.
     * Deteksi short, overcurrent, blown (meledak).
     * Format SI untuk bacaan alat ukur.

   Aturan ketat:
     * HANYA #include "types.h" (dan transitif "config.h").
     * TIDAK BOLEH include <windows.h> — supaya unit test "core murni"
       bisa dijalankan di Linux tanpa MinGW.
     * Semua parameter murni tipe data (int, double, pointer ke struct).
       Tidak ada HDC/HWND/HINSTANCE di signature mana pun.

   Fungsi audio (buzzer_audio_update, dst) SENGAJA tidak dideklarasikan
   di sini — itu ranah include/audio.h. sim.c akan #include "audio.h"
   sendiri untuk memanggil buzzer_audio_update() di akhir recompute.

   Catatan frekuensi AC:
     * sim_ac_voltage() memakai konstanta internal AC_FREQ_HZ = 50.0 Hz
       (grid listrik Indonesia). Konstanta ini sebaiknya dipindah ke
       config.h sebagai AC_MAINS_FREQ_HZ pada fase implementasi
       src/core/sim.c — supaya oscilloscope.c tidak perlu duplikat
       OSC_FREQ_HZ = 50.0. Header ini tidak mengeksposnya ke pemanggil;
       kalau perlu, expose via fungsi baru sim_ac_frequency().
   ============================================================================
 */

#include "types.h"

/* ============================================================================
   BAGIAN 1 — State global simulasi (definisi di src/core/sim.c)

   Semua variabel `extern` — hanya didefinisikan SEKALI di sim.c.
   Dibaca oleh UI untuk coloring (LED menyala hijau, kabel hot merah),
   banner status, dan panel osiloskop.

   Pola akses:
     * sim_recompute() menulis semua variabel ini.
     * UI & komponen_*.c HANYA membaca.
     * Jangan tulis langsung kecuali dari dalam sim.c.
   ============================================================================
 */

/* ---- Mode global ---- */
extern int sim_running;   /* 1 = RUN aktif (freeze edit), 0 = mode edit    */
extern int sim_led_count; /* jumlah LED yang menyala saat ini              */

/* ---- Peringatan & deteksi fault ---- */
extern int sim_short;       /* 1 = terdeteksi korsleting (+)-(−)             */
extern int sim_has_battery; /* 1 = canvas punya >=1 komponen COMP_BATTERY    */
extern int sim_has_ac;      /* 1 = canvas punya >=1 komponen COMP_ACSOURCE   */
extern int sim_ac_mode;     /* 1 = mode AC murni (ada AC, tidak ada DC)      */

/* ---- Hasil hitung LED utama (LED dengan arus terbesar) ---- */
extern double sim_vsrc;      /* V sumber efektif di loop LED utama (V)      */
extern double sim_rloop;     /* R total loop LED utama (Ω)                  */
extern double sim_iled_max;  /* arus LED terbesar (A)                       */
extern double sim_vdrop_max; /* Vf LED yang dipakai saat hitung (V)         */

/* ---- Fault LED ---- */
extern int sim_exploded; /* 1 = ada LED meledak (arus >> 20 mA)           */
extern int sim_overcur;  /* 1 = ada LED overcurrent (20 mA < I < meledak) */

/* ---- Posisi visual marker short (world coords). -1 = tidak ada ---- */
extern int sim_short_x, sim_short_y;
extern int sim_short_comp_id;  /* id komponen penyebab short, -1 = tidak ada */
extern int sim_short_wire_idx; /* index wire korban, -1 = tidak ada          */

/* ============================================================================
   BAGIAN 2 — Kontrol utama simulasi
   ============================================================================
 */

/* Hitung ulang seluruh hasil simulasi dari state model saat ini.
   Idempoten — aman dipanggil berkali-kali. Kalau canvas kosong atau tidak
   punya sumber, semua flag fault/reset ke 0.

   Efek samping: memicu buzzer_audio_update() (via audio.h). */
void sim_recompute(void);

/* Set mode RUN on/off. Saat on:
     - Guard: canvas kosong → tolak + set status message.
     - Freeze editing (dragging_id = -1, wiring_active = 0).
     - Panggil sim_recompute() sekali.
     - Set status message dari sim_message().
   Saat off: kembali ke mode edit + status default. */
void sim_set_running(int on);

/* Pesan status satu-baris untuk banner. Contoh:
     "Belum ada sumber (baterai/AC) — LED tidak bisa menyala."
     "KORSLETING! (+) tersambung langsung ke (-)."
     "Rangkaian BENAR — LED MENYALA. I = (9 - 2 V)/220 Ω = 31.8 mA. AMAN."
   Return pointer ke buffer internal statis — JANGAN di-free. */
const wchar_t *sim_message(void);

/* ============================================================================
   BAGIAN 3 — Query hasil per-komponen & per-kabel

   Semua fungsi mengambil pointer Component/Wire — mencari index di comps[]
   / wires[] via pointer arithmetic. Return 0 / -1 kalau komponen tidak
   ditemukan atau tipe tidak sesuai.
   ============================================================================
 */

/* 1 = kabel index `idx` ada di net (+), 0 = tidak. */
int sim_wire_hot(int idx);

/* 1 = LED menyala (I > 0 & Vth > Vf). Return 0 untuk tipe bukan LED. */
int sim_comp_lit(const Component *c);

/* 1 = transistor/MOSFET sedang konduktif (B/G HIGH & E/S LOW → C/D short E/S).
 */
int sim_comp_conducting(const Component *c);

/* 1 = dioda/Zener forward-biased (menghantar arah A→K). */
int sim_diode_fwd(const Component *c);

/* 1 = LED arus > 20 mA × toleransi (rusak pelan atau meledak). */
int sim_comp_blown(const Component *c);

/* Arus LED (A). -1 = tidak terdefinisi / bukan LED. ~1e9 = short-loop. */
double sim_comp_current(const Component *c);

/* ============================================================================
   BAGIAN 4 — Kontrak per-tipe komponen (dipakai komponen_*.c & info dialog)
   ============================================================================
 */

/* Buzzer berbunyi: RUN aktif & |V(pin0)-V(pin1)| > 1V. */
int sim_buzzer_on(const Component *c);

/* Resistansi efektif LDR saat ini (Ω). TERANG = 1% × nilai gelap. */
double sim_ldr_effective_r(const Component *c);

/* Zener: forward-biased (A→K), reverse-breakdown (men-clamp di Vz). */
int sim_zener_fwd(const Component *c);
int sim_zener_breakdown(const Component *c);

/* Output gerbang logika HIGH (1) saat RUN. HIGH = terhubung ke (+). */
int sim_gate_out_high(const Component *c);

/* ============================================================================
   BAGIAN 5 — Alat ukur: voltmeter & ammeter

   Voltmeter ideal (impedansi tak hingga) dipasang PARALEL:
     pin0 = titik A (+), pin1 = titik B (−).
   Ammeter ideal (~0Ω) dipasang SERI — R_ammeter = 1 mΩ di sim.c.
   ============================================================================
 */

/* Baca tegangan V(pin0) - V(pin1). -1 = tidak terhubung / tidak valid. */
double sim_voltmeter_reading(const Component *c);
int sim_voltmeter_connected(const Component *c);

/* Baca arus cabang |V(pin0)-V(pin1)| / R_ammeter. -1 = tidak terhubung. */
double sim_ammeter_reading(const Component *c);
int sim_ammeter_connected(const Component *c);

/* ============================================================================
   BAGIAN 6 — Probe osiloskop (domain-waktu via superposisi)

   Konsep: karena rangkaian linier (dianggap linier per-saat), tegangan
   probe = DC_offset + Σ gain[k] × AC_source[k](t).

   sim_build_probe_responses() menghitung:
     * DC_offset  : solve nodal dengan semua sumber AC di-set 0
     * gain[k]    : solve nodal dengan sumber AC ke-k = 1V, lainnya 0
   Hasil di-cache internal; dipanggil ulang saat state berubah.
   ============================================================================
 */

/* Nilai AC source pada waktu t (detik). Return V_instan (bisa negatif).
   Bentuk gelombang mengikuti src->wave (SINE/SQUARE/TRIANGLE).
   Frekuensi internal 50 Hz. Vpk = Vrms × √2. */
double sim_ac_voltage(const Component *src, double t);

/* Bangun ulang tabel respons semua probe (DC + gain per sumber AC). */
void sim_build_probe_responses(void);

/* Tegangan probe pada waktu t. Panggil sim_build_probe_responses() dulu
   (atau otomatis di-call internal via flag _sim_probe_resp_ready). */
double sim_probe_voltage(const Component *probe, double t);

/* ============================================================================
   BAGIAN 7 — Marker short (navigasi canvas)

   Hit-test & pan-to untuk marker panah merah "SHORT!" di canvas.
   Kedua fungsi ini KOORDINAT SCREEN-relative (bukan world) untuk hit
   (dipakai langsung dari WM_LBUTTONDOWN), tapi pan_to pakai world coords
   untuk set g_pan_x/y.
   ============================================================================
 */

/* 1 kalau (sx, sy) di dalam radius marker (25 px) DAN marker aktif. */
int short_marker_hit(int sx, int sy);

/* Geser g_pan_x/y agar titik short ter-center di viewport ukuran
   canvas_w × canvas_h. No-op kalau sim_short_x < 0. */
void short_marker_pan_to(int canvas_w, int canvas_h);

/* Update posisi visual marker (sim_short_x/y) dari state model terbaru.
   Murah — tidak rebuild union-find. Dipanggil dari events.c saat drag
   agar marker ikut bergerak real-time bersama komponen penyebabnya. */
void sim_update_short_position(void);

/* ============================================================================
   BAGIAN 8 — Format SI (dipakai semua modul yang menampilkan nilai)
   ============================================================================
 */

/* Format nilai dengan prefix SI ke buffer. Contoh:
     sim_format_si(0.0318, L"A", buf, 32)  → L"31.8 mA"
     sim_format_si(220.0,  L"Ω", buf, 32)  → L"220 Ω"
     sim_format_si(1e6,    L"Ω", buf, 32)  → L"1 kΩ"
     sim_format_si(100e-9, L"F", buf, 32)  → L"100 nF"
   Prefix range: p, n, u, m, (none), k. Tidak support M/G/T (jarang dipakai
   di domain elektronika dasar). Kalau perlu, upgrade di sini dan semua
   pemanggil otomatis ikut.
   `unit` boleh NULL (hanya angka + prefix). */
void sim_format_si(double v, const wchar_t *unit, wchar_t *out, int cap);

#endif /* SIM_H */