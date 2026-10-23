/* ============================================================================
   tests/stubs_audio.c — Implementasi no-op audio untuk build TEST.

   Fungsi:
     Menggantikan src/audio/buzzer_audio.c saat build test. Tujuannya
     dua:
       1. Memenuhi symbol yang dipanggil oleh sim.c (buzzer_audio_update).
       2. Menghilangkan dependensi -lwinmm + <windows.h> + <mmsystem.h>
          dari binary test, sehingga test bisa dijalankan di Linux tanpa
          MinGW sama sekali.

   Kapan file ini dipakai:
     * HANYA saat build target test (SCHEMATIC_TEST_STUBS=1).
     * TIDAK PERNAH di build produksi (lihat Makefile: SRC_AUDIO dipilih
       berdasarkan target).

   Kontrak:
     Ketiga fungsi HARUS punya signature IDENTIK dengan yang dideklarasikan
     di include/audio.h:
         void buzzer_audio_update(void);
         void buzzer_audio_stop(void);
         void buzzer_audio_cleanup(void);

   Kalau signature berubah di audio.h, file ini WAJIB di-update bersamaan
   (kalau tidak: linker error di build test).

   Semantik: semua no-op. Test tidak mempedulikan audio — hanya butuh
   symbol terdefinisi agar link berhasil. Efek samping apapun (buka audio
   device, alokasi buffer WAV, panggilan PlaySound) TIDAK diinginkan
   karena akan memperlambat test dan bisa menyebabkan hang di CI headless.
   ============================================================================ */

#include "audio.h"

void buzzer_audio_update(void)  { /* no-op di build test */ }
void buzzer_audio_stop(void)    { /* no-op di build test */ }
void buzzer_audio_cleanup(void) { /* no-op di build test */ }