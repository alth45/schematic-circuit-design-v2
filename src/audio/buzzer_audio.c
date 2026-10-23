/* ============================================================================
   src/audio/buzzer_audio.c — Playback nada sinus untuk buzzer aktif.

   Memainkan nada sinus pada frekuensi buzzer aktif (v Hz) selama RUN.
   Implementasi: PlaySound + WAV in-memory yang dibangkitkan dinamis.

   Kenapa WAV (bukan Beep / MessageBeep)?
     - Beep hanya bisa satu nada dan blocking.
     - MessageBeep pakai suara sistem, tidak bisa atur frekuensi.
     - PlaySound + WAV: frekuensi bebas, loop continuous, async (tidak
       blokir UI).

   Kenapa sinus (bukan kotak)?
     - Kotak menghasilkan click di boundary loop (jump dari -amp ke +amp).
     - Sinus siklus penuh mulai & akhir di 0 → loop mulus.

   Single channel PlaySound:
     - Hanya buzzer AKTIF PERTAMA yang diputar.
     - Restart audio hanya bila frekuensi berubah (>0.5 Hz) atau status
       berubah (aktif → nonaktif → aktif).

   -----------------------------------------------------------------------
   Platform:
     File ini tetap Windows-specific karena memakai PlaySoundW (winmm.dll).
     Di build test, file ini DIGANTI oleh tests/stubs_audio.c (no-op) —
     tidak ikut dikompilasi. Itu sebabnya file ini BOLEH include <windows.h>.

   Include:
     * "audio.h"   — kontrak publik (buzzer_audio_update/stop/cleanup).
     * "sim.h"     — sim_buzzer_on(), sim_running.
     * "model.h"   — comps[], comp_count.
     * <windows.h> — LPCWSTR, typedef Windows inti.
     * <mmsystem.h> — PlaySoundW, SND_MEMORY/SND_ASYNC/SND_LOOP/SND_NODEFAULT.
     * <math.h>     — M_PI, sin(), fabs(), lround().
     * <stdlib.h>   — malloc(), free().
     * <string.h>   — memcpy().

   Link: -lwinmm (sudah ada di Makefile produksi).
   ============================================================================ */

/* _USE_MATH_DEFINES wajib didefinisikan SEBELUM <math.h> supaya M_PI
   tersedia di MSVC / MinGW. Di glibc flag ini diabaikan (harmless). */
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "model.h"
#include "sim.h"

/* ===================== Konstanta audio ===================== */
#define BA_SAMPLE_RATE   44100
#define BA_AMPLITUDE     10000     /* ~30% dari int16 max */
#define BA_DURATION_SEC  0.25      /* 250 ms per loop */
#define BA_FADE_SAMPLES  10        /* anti-click di boundary */

/* ===================== State internal (file-scope static) =====================
   Semua state di sini PRIVATE — tidak ada yang bocor via header.
   Umur state = umur proses, kecuali dibersihkan oleh buzzer_audio_cleanup. */
static unsigned char *g_wav_buf    = NULL;
static size_t         g_wav_size   = 0;
static double         g_cur_freq   = 0.0;
static int            g_is_playing = 0;

/* ===================== Generator WAV ===================== */

/* Bangun WAV mono 16-bit sinus pada freq_hz ke g_wav_buf. */
static void ba_generate_wav(double freq_hz) {
    /* Durasi mendekati BA_DURATION_SEC; bulatkan ke jumlah siklus
       terdekat supaya loop boundary berada di fase ~2π. */
    int n_cycles = (int)lround(freq_hz * BA_DURATION_SEC);
    if (n_cycles < 1) n_cycles = 1;

    int n_samples = (int)((double)n_cycles * BA_SAMPLE_RATE / freq_hz);
    if (n_samples < 2) n_samples = 2;

    int data_size  = n_samples * 2;    /* 16-bit mono */
    int total_size = 44 + data_size;

    if (g_wav_buf) { free(g_wav_buf); g_wav_buf = NULL; }
    g_wav_buf = (unsigned char *)malloc((size_t)total_size);
    if (!g_wav_buf) { g_wav_size = 0; return; }

    unsigned char *p = g_wav_buf;

    /* ---- RIFF header ---- */
    memcpy(p, "RIFF", 4); p += 4;
    *(unsigned int *)p = (unsigned int)(total_size - 8); p += 4;
    memcpy(p, "WAVE", 4); p += 4;

    /* ---- fmt chunk (PCM, mono, 16-bit) ---- */
    memcpy(p, "fmt ", 4); p += 4;
    *(unsigned int  *)p = 16;                 p += 4;  /* chunk size     */
    *(unsigned short*)p = 1;                  p += 2;  /* PCM            */
    *(unsigned short*)p = 1;                  p += 2;  /* channel = mono */
    *(unsigned int  *)p = BA_SAMPLE_RATE;     p += 4;  /* sample rate    */
    *(unsigned int  *)p = BA_SAMPLE_RATE * 2; p += 4;  /* byte rate      */
    *(unsigned short*)p = 2;                  p += 2;  /* block align    */
    *(unsigned short*)p = 16;                 p += 2;  /* bits/sample    */

    /* ---- data chunk ---- */
    memcpy(p, "data", 4); p += 4;
    *(unsigned int *)p = (unsigned int)data_size; p += 4;

    /* ---- sine samples ---- */
    short *samples = (short *)p;
    for (int i = 0; i < n_samples; i++) {
        double t = (double)i / BA_SAMPLE_RATE;
        double s = sin(2.0 * M_PI * freq_hz * t);
        samples[i] = (short)(s * BA_AMPLITUDE);
    }

    /* ---- fade-out 10 sample terakhir (anti-click di loop boundary) ---- */
    for (int i = 0; i < BA_FADE_SAMPLES && i < n_samples; i++) {
        int idx = n_samples - 1 - i;
        double f = (double)i / BA_FADE_SAMPLES;
        samples[idx] = (short)(samples[idx] * f);
    }

    g_wav_size = (size_t)total_size;
}

/* ===================== API publik (kontrak audio.h) ===================== */

void buzzer_audio_stop(void) {
    if (g_is_playing) {
        PlaySoundW(NULL, NULL, 0);   /* stop audio played by this process */
        g_is_playing = 0;
        g_cur_freq = 0.0;
    }
}

void buzzer_audio_update(void) {
    /* Cari buzzer aktif pertama. */
    Component *active = NULL;
    for (int i = 0; i < comp_count; i++) {
        if (comps[i].type == COMP_BUZZER && sim_buzzer_on(&comps[i])) {
            active = &comps[i];
            break;
        }
    }

    if (!active) {
        buzzer_audio_stop();
        return;
    }

    /* Frekuensi aman. */
    double freq = (active->v > 0.0) ? active->v : BUZZER_FREQ_DEFAULT;
    if (freq < 20.0)    freq = 20.0;
    if (freq > 20000.0) freq = 20000.0;

    int need_restart = 0;
    if (!g_is_playing)                          need_restart = 1;
    else if (fabs(freq - g_cur_freq) > 0.5)     need_restart = 1;

    if (!need_restart) return;   /* sudah playing di freq yang sama */

    if (g_is_playing) {
        PlaySoundW(NULL, NULL, 0);   /* stop sebelum regenerate */
        g_is_playing = 0;
    }

    ba_generate_wav(freq);
    if (!g_wav_buf || g_wav_size == 0) return;

    if (PlaySoundW((LPCWSTR)g_wav_buf, NULL,
                   SND_MEMORY | SND_ASYNC | SND_LOOP | SND_NODEFAULT)) {
        g_is_playing = 1;
        g_cur_freq   = freq;
    }
}

void buzzer_audio_cleanup(void) {
    buzzer_audio_stop();
    if (g_wav_buf) {
        free(g_wav_buf);
        g_wav_buf  = NULL;
        g_wav_size = 0;
    }
}