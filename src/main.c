/* ============================================================================
   src/main.c — Program entry point (wWinMain).

   Ini adalah SATU-SATUNYA fungsi yang tersisa di root src/. Semua logika
   sudah dipindah ke layer spesifik:

     Window lifecycle        → src/ui/window.c
       main_window_create()
       WndProc()
       g_main_hwnd

     Splash screen startup   → src/ui/panels/splash.c

     Accelerator table       → TETAP di sini (tanggung jawab main loop)

   Include:
     * "ui.h" — <windows.h> + deklarasi semua API UI yang dibutuhkan
                (main_window_create, splash_run, g_main_hwnd, semua IDM_*).

   Message loop di sini minimal:
     1. TranslateAcceleratorW (shortcut keyboard seperti Ctrl+S, F5, F12).
     2. TranslateMessage + DispatchMessage untuk sisanya.

   Catatan tentang `wWinMain`:
     * -municode di Makefile memetakan entry point ke wWinMain (wide char).
     * HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow tidak dipakai
       (argumen standar Win32 — diabaikan).
   ============================================================================ */
#include "ui.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show) {
  (void)hPrev; (void)cmd; (void)show;

  /* 1) Daftarkan window class + buat window utama (hidden). */
  main_window_create(hInst);

  /* 2) Tampilkan splash screen; splash akan ShowWindow(window utama)
        setelah timer habis / user klik. Fungsi ini juga menjalankan
        message loop-nya sendiri selama splash aktif. */
  splash_run(hInst, g_main_hwnd);

  /* 3) Accelerator table untuk shortcut global.
        Lihat tabel keyboard: docs/CONTRIBUTING.md atau menu Bantu. */
  ACCEL accels[] = {
      {FCONTROL | FVIRTKEY, 'N', IDM_FILE_NEW},
      {FCONTROL | FVIRTKEY, 'O', IDM_FILE_OPEN},
      {FCONTROL | FVIRTKEY, 'S', IDM_FILE_SAVE},
      {FCONTROL | FVIRTKEY, 'E', IDM_FILE_EXPORT_PNG},
      {FVIRTKEY, VK_F5, IDM_SIM_RUN_TOGGLE},
      {FVIRTKEY, VK_F9, IDM_SIM_RECOMPUTE},
      {FVIRTKEY, VK_F12, IDM_VIEW_OSC},
  };
  HACCEL hAccel = CreateAcceleratorTableW(
      accels, (int)(sizeof(accels) / sizeof(accels[0])));

  /* 4) Main message loop. */
  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0) > 0) {
    if (hAccel && TranslateAcceleratorW(g_main_hwnd, hAccel, &msg))
      continue;
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  if (hAccel)
    DestroyAcceleratorTable(hAccel);
  return (int)msg.wParam;
}