#ifndef AUDIO_H
#define AUDIO_H

/* ============================================================================
   audio.h — API publik modul AUDIO (playback nada buzzer).

   Satu-satunya consumer yang dikenal: core/sim.c (memanggil
   buzzer_audio_update di akhir sim_recompute) dan ui/window.c (memanggil
   buzzer_audio_cleanup saat window destroy).

   Aturan ketat:
     * TIDAK BOLEH include <windows.h> atau <mmsystem.h>.
     * Header ini murni deklarasi (void) → bisa di-include di mana saja.

   Implementasi (audio/buzzer_audio.c) BOLEH include <windows.h> +
   <mmsystem.h> — itu detail internal modul, tidak bocor ke header.

   ============================================================================
   STRATEGI STUB UNTUK UNIT TEST
   ============================================================================

   Build produksi:
       src/audio/buzzer_audio.c  → link -lwinmm, memutar nada sungguhan.

   Build test (SCHEMATIC_TEST_STUBS=1):
       tests/stubs_audio.c       → implementasi kosong (no-op):
           void buzzer_audio_update(void)  { /* no-op */ 
void buzzer_audio_stop(void) { /* no-op */ }
void buzzer_audio_cleanup(void) { /* no-op */ }

/*Dengan begitu,
    sim.c cukup link dengan stub dan TIDAK butuh -
            lwinmm atau<mmsystem.h> saat test di Linux(maupun Windows)
                .Makefile memilih stub otomatis saat target test.

        == == == == == == == == == == == == == == == == == == == == == == == ==
        == == == == == == == == == == == == == == KONTRAK SEMANTIK == == == ==
        == == == == == == == == == == == == == == == == == == == == == == == ==
        == == == == == == == == == ==

        Nada =
        sinus(bukan kotak) untuk mulus di loop boundary.Frekuensi mengikuti c
        -> v(Hz) buzzer AKTIF PERTAMA di comps[](single channel — hanya satu
                                                     buzzer yang boleh berbunyi
                                                         pada satu waktu)
            .

        Fungsi idempoten — aman dipanggil berkali
        - kali per detik : *buzzer_audio_update() hanya restart audio kalau
    : -status berubah dari diam → berbunyi,
        ATAU - frekuensi berubah >
            0.5 Hz.Kalau sudah berbunyi di frekuensi yang sama,
        no -
            op.

                Semua fungsi tahan NULL
                / state kosong.Tidak ada error
            - return;
kegagalan PlaySound(mis.audio device busy) → diam saja,
    tidak crash.== == == == == == == == == == == == == == == == == == == == ==
        == == == == == == == == == == == == == == == == ==
        */

/* ============================================================================
   BAGIAN 1 — Playback control (idempoten)
   ============================================================================
 */

/* Putar / stop / restart nada sesuai state buzzer aktif saat ini.
   Dipanggil dari sim_recompute() setiap kali state berubah (setelah
   sim_running, switch, LDR, pot, nilai komponen berubah, dll).

   Alur internal:
     1. Cari buzzer aktif pertama: type == COMP_BUZZER &&
   sim_buzzer_on(c).
     2. Kalau tidak ada → stop (kalau sedang playing).
     3. Kalau ada → hitung freq = clamp(c->v, 20..20000 Hz).
     4. Restart hanya kalau status berubah atau |freq - g_cur_freq|
   > 0.5. */
void buzzer_audio_update(void);

/* Hentikan playback segera (idempoten). Reset state g_cur_freq ke 0.
   Setelah ini, buzzer_audio_update() berikutnya akan restart dari nol
   (dianggap "belum playing"). */
void buzzer_audio_stop(void);

/* Bersihkan resource audio: panggil buzzer_audio_stop() + free buffer WAV
   in-memory. WAJIB dipanggil saat window utama di-destroy (WM_DESTROY)
   supaya tidak ada audio yang "nyangkut" setelah aplikasi ditutup. */
void buzzer_audio_cleanup(void);

#endif /* AUDIO_H */