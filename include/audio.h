#ifndef AUDIO_H
#define AUDIO_H

/* ============================================================================
   audio.h — API publik modul AUDIO (playback nada buzzer).

   ATURAN KETAT:
     * File ini HANYA berisi DEKLARASI (prototipe fungsi).
     * Implementasi ada di:
         - src/audio/buzzer_audio.c   (produksi, butuh <windows.h> + -lwinmm)
         - tests/stubs_audio.c        (test build, semua no-op)
     * JANGAN pernah menaruh body fungsi { ... } di header ini — akan
       menyebabkan "redefinition" saat file .c implementasi ikut di-compile.

   Aturan include:
     * TIDAK BOLEH include <windows.h> atau <mmsystem.h>.
     * Header ini murni deklarasi void → bisa di-include di mana saja.
   ============================================================================ */

#include "types.h"

/* Putar / stop / restart nada sesuai state buzzer aktif saat ini.
   Idempoten — aman dipanggil berkali-kali per detik. */
void buzzer_audio_update(void);

/* Hentikan playback segera. Idempoten. */
void buzzer_audio_stop(void);

/* Bersihkan resource audio: stop + free buffer WAV. Panggil saat
   window utama di-destroy (WM_DESTROY). */
void buzzer_audio_cleanup(void);

#endif /* AUDIO_H */