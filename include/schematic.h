#ifndef SCHEMATIC_H
#define SCHEMATIC_H

/* ============================================================================
   schematic.h — Umbrella header (kompatibilitas ke belakang).

   TUJUAN
   ------
   Satu-satunya gunanya: mempertahankan kompatibilitas dengan kode lama yang
   masih `#include "schematic.h"`. Selama fase migrasi src/ (memindah file
   ke core/, io/, ui/, components/, audio/), setiap file lama otomatis tetap
   compile tanpa perlu diubah satu per satu.

   Setelah seluruh migrasi src/ selesai, file ini boleh:
     * dipertahankan sebagai shortcut kenyamanan untuk file baru yang
       memang butuh hampir semua modul (jarang), ATAU
     * dihapus total kalau tim memilih disiplin "satu header per modul".

   KONSEKUENSI PENTING
   -------------------
   Karena ui.h mencakup <windows.h>, meng-include schematic.h berarti
   menarik <windows.h> juga. Ini berarti:

     ✅ BOLEH dipakai di: src/main.c, src/ui/**, src/components/**,
        src/io/export_png.c (semua modul Win32).

     ❌ TIDAK BOLEH dipakai di:
          src/core/model.c
          src/core/sim.c
          src/io/fileio.c
          src/io/png_writer.c
          src/audio/buzzer_audio.c
          tests/**
        File-file tersebut WAJIB include header spesifik saja
        (model.h / sim.h / io.h / audio.h) supaya unit test bisa
        di-compile di Linux tanpa MinGW dan tanpa -lgdi32 -lwinmm.

   URUTAN MIGRASI YANG DISARANKAN
   ------------------------------
   Untuk menjaga `make test` hijau di setiap langkah:

     Langkah 1 — Header fase (SELESAI): include/ dibangun.
     Langkah 2 — Migrasi file PURE lebih dulu:
        core/model.c     → #include "model.h" "sim.h"
        core/sim.c       → #include "sim.h" "model.h" "audio.h"
        io/fileio.c      → #include "io.h" "model.h" "sim.h"
        io/png_writer.c  → #include "io.h"
        audio/buzzer_audio.c → #include "audio.h" "sim.h" + <windows.h> +
   <mmsystem.h> Langkah 3 — Migrasi tests/ + stubs_audio.c (make test lulus di
   Linux). Langkah 4 — Migrasi UI (src/main.c, src/ui/**, src/components/**):
        boleh tetap pakai "schematic.h" selamanya; tidak wajib diubah.
   ============================================================================
 */

/* ---- Modul pure (nol windows.h) — urutan ini bukan alfabetis, tapi
        dependency order: config → types → model/sim/io/audio. ---- */
#include "audio.h"
#include "config.h"
#include "io.h"
#include "model.h"
#include "sim.h"
#include "types.h"


/* ---- Modul UI — satu-satunya yang menarik <windows.h>. ----
        Diletakkan paling akhir karena ui.h sendiri meng-include
        model.h + sim.h (redundan tapi harmless — header guards
        mencegah dobel). */
#include "ui.h"

#endif /* SCHEMATIC_H */