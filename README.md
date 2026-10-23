# Schematic Editor — Win32 GDI Edition

Editor dan simulator rangkaian elektronika berbasis Win32 GDI untuk Windows.
Dirancang untuk pembelajaran, eksperimen, dan visualisasi konsep dasar
listrik secara interaktif.

![version](https://img.shields.io/badge/version-1.0-blue)
![platform](https://img.shields.io/badge/platform-Windows%2010%2B-lightgrey)
![language](https://img.shields.io/badge/C-11-green)
![license](https://img.shields.io/badge/license-MIT-blue)

---

## Fitur

- **Editor skematik** — rotasi, multi-select (blok), duplikasi blok, undo/redo
  (bounded 64 level), snap-to-grid, T-junction wire-to-wire.
- **25 jenis komponen** — resistor, saklar, baterai, sumber AC (sinus/kotak/
  segitiga), LED (4 warna + glow), ground, kapasitor, kapasitor polar,
  transistor NPN, dioda, dioda Zener, MOSFET N-channel, induktor, potensiometer,
  LDR, buzzer, dan 6 gerbang logika (AND/OR/NOT/NAND/NOR/XOR).
- **Simulasi DC & AC** — analisis nodal dengan model Thevenin per LED;
  deteksi korsleting, overcurrent (LED rusak), dan LED meledak.
- **Alat ukur** — Voltmeter, Ammeter (ideal), Probe Osiloskop dengan
  trace 4 kanal (mode domain-waktu via superposisi).
- **Panel osiloskop** — jendela terpisah, refresh 5 Hz, skala vertikal
  otomatis, bentuk gelombang sinus/kotak/segitiga.
- **Zoom & pan canvas** — wheel zoom, middle-click drag pan, zoom-fit,
  reset view.
- **Minimap** — panel overview canvas dengan viewport indikator, klik-drag
  untuk pan.
- **Simpan / muat** — format JSON v1 (atomic load — file rusak tidak merusak
  state aktif).
- **Export PNG** — render canvas ke PNG (encoder mandiri, tanpa libpng/zlib).
- **Status banner** — feedback visual saat RUN (LED menyala, korsleting,
  overcurrent, exploded).
- **Buzzer audio** — nada sinus pada frekuensi yang di-set, idempoten,
  loop seamless.

---

## Quick start (30 detik)

### Windows — MinGW-w64 (gcc)

```cmd
:: Clone atau ekstrak folder proyek
cd schematic-editor

:: Build dengan make (paling cepat)
make app
schematic_win.exe

:: Atau tanpa make — cukup gcc di PATH
build.bat
schematic_win.exe

:: Atau dengan CMake + Visual Studio
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target schematic_win
build\Release\schematic_win.exe
```

## Linux — test suite saja (pure core, no Win32)
```sh
make test                        # build + run
./build.sh test                  # fallback tanpa make
cmake -B build && cmake --build build --target run_tests
```

## Struktur proyek

```
schematic-editor/
├── include/                     Header publik (8 file)
│   ├── schematic.h              Umbrella (kompat ke belakang)
│   ├── config.h                 Konstanta numerik murni
│   ├── types.h                  Enum + struct (nol windows.h)
│   ├── model.h                  API core/model
│   ├── sim.h                    API core/sim
│   ├── io.h                     API io/ (JSON + PNG, pure)
│   ├── audio.h                  API audio/
│   └── ui.h                     API UI (satu-satunya dengan <windows.h>)
│
├── src/
│   ├── main.c                   Entry point (~40 baris)
│   ├── core/                    ← PURE (no <windows.h>)
│   │   ├── model.c              Data model, undo/redo, selection
│   │   └── sim.c                Analisis nodal, zener, probe, gate
│   ├── io/
│   │   ├── fileio.c             JSON save/load (pure)
│   │   ├── png_writer.c         PNG encoder mandiri (pure)
│   │   └── export_png.c         Render canvas → PNG (GDI)
│   ├── audio/
│   │   └── buzzer_audio.c       PlaySoundW + WAV in-memory
│   ├── ui/
│   │   ├── window.c             WndProc + lifecycle
│   │   ├── canvas/              draw, events, minimap, viewport
│   │   ├── toolbar/             menubar, symbols, actions, palette
│   │   ├── panels/              oscilloscope, splash
│   │   └── dialogs/             file_dialogs, value_editor
│   └── components/              20 file simbol (auto-discover)
│
├── tests/                       Test suite (Linux-friendly)
│   ├── platform_compat.h        Shim _wfopen/_vsnwprintf (non-Win32)
│   ├── test_support.h/c         Harness (assert + counter + reset)
│   ├── stubs_audio.c            No-op audio untuk build test
│   └── test_*.c                 5 file test (model, sim, selection, undo)
│
├── docs/                        ARCHITECTURE.md, CONTRIBUTING.md
├── examples/                    3 file JSON contoh
├── Makefile                     Dual-target: app + test
├── CMakeLists.txt               Portable (MSVC, VS Code, CI)
├── build.bat                    Wrapper Windows (fallback tanpa make)
└── build.sh                     Wrapper POSIX (fallback tanpa make)
```

*Detail arsitektur & dependency graph ada di*  [docs/ARCHITECTURE.md].

## Konsep Kunci: Pure Core vs UI Layer

Project ini memisahkan dengan ketat antara:

| Layer | Header | `<windows.h>` | Test Linux | Isolasi |
|-------|--------|:-------------:|:----------:|---------|
| `core/` | `model.h`, `sim.h` | ❌ | ✅ native | Pure logic |
| `io/` | `io.h` | ❌ | ✅ native | Serialisasi |
| `audio/` | `audio.h` | ❌ *(impl)* | ❌ stub | Platform |
| `ui/` | `ui.h` | ✅ *(satu-satunya)* | ❌ | Windows-only |

**Konsekuensi:**

| # | Dampak |
|---|--------|
| 1 | `make test` di Linux compile tanpa MinGW — cukup `gcc` native |
| 2 | Bug fisika / serialisasi JSON tertangkap di CI Linux |
| 3 | Refactor UI ⟂ logika inti (tidak saling menyentuh) |



## Shortcut Keyboard

### 🛠️ Tool

| Tombol | Aksi |
|--------|------|
| `V` | Tool Select (pilih / drag) |
| `B` | Tool Blok (multi-select) |
| `W` | Tool Wire (tarik kabel) |

### 🔌 Komponen — Baris Atas

| Tombol | Komponen |
|--------|----------|
| `1` | Resistor |
| `2` | Saklar |
| `3` | Baterai |
| `4` | LED |
| `5` | Ground |
| `6` | Kapasitor |
| `7` | Transistor |
| `8` | Dioda |
| `9` | Kap. Polar |
| `0` | Sumber AC |

### 🔋 Komponen — Baris Tengah

| Tombol | Komponen |
|--------|----------|
| `L` | LDR |
| `U` | Buzzer |
| `M` | Voltmeter |
| `I` | Ammeter |
| `P` | OSC |
| `J` | Induktor |
| `Z` | Zener |
| `F` | MOSFET |
| `T` | Potensiometer |

### 🧠 Gerbang Logika

| Tombol | Gerbang |
|--------|---------|
| `A` | AND |
| `O` | OR |
| `N` | NOT |
| `Y` | NAND |
| `X` | XOR |

### ✏️ Editing

| Tombol | Aksi |
|--------|------|
| `R` | Putar komponen terpilih 90° |
| `Del` / `Backspace` | Hapus komponen terpilih |
| `Esc` | Batal pilih / kabel |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+A` | Select All |
| `Ctrl+D` | Duplikat komponen terpilih |

### 💾 File

| Tombol | Aksi |
|--------|------|
| `Ctrl+S` / `Ctrl+O` | Save / Open JSON |
| `Ctrl+E` | Export PNG |

### 🔍 View

| Tombol | Aksi |
|--------|------|
| `Ctrl++` / `Ctrl+-` / `Ctrl+0` | Zoom in / out / fit |
| Mouse tengah + drag | Pan canvas |

### ▶️ Simulasi

| Tombol | Aksi |
|--------|------|
| `F5` / `F9` | Run / Stop simulasi / Recompute |
| `F12` | Buka panel Osiloskop |

### 🖱️ Mouse

| Aksi | Fungsi |
|------|--------|
| Double-click komponen | Edit nilai |
| Klik kanan komponen | Menu konteks |

## Development 
## Prasyarat

- **Windows build:** MinGW-w64 (gcc) atau MSVC 2019+
- **Test only (Linux/macOS):** gcc/clang + `make` *(opsional)*
- **PNG export:** tidak ada dependensi luar *(encoder mandiri)*


```ps1
# Produksi
make app                     # via Makefile
cmake -B build && cmake --build build --target schematic_win   # via CMake
build.bat / ./build.sh       # fallback gcc langsung

# Test
make test                    # build + run unit tests
cmake -B build && ctest --test-dir build --output-on-failure
./build.sh test

# Bersih-bersih
make clean
./build.sh clean
```

## Cross compile window dari linux 

```ps1
CC_PROD=x86_64-w64-mingw32-gcc make app
# atau
CC_PROD=x86_64-w64-mingw32-gcc ./build.sh app
```

Butuh paket `mingw-w64` di distro Anda.

## Menambah Komponen Baru

### 1. Buat file komponen

Buat `src/components/nama.c` dengan template:

```c
#include "ui.h"

static void draw_nama(HDC hdc, int x, int y, BOOL sel) {
    /* ... */
}

void draw_k_nama(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_nama);
}
```

### 2. Deklarasikan fungsi

Tambahkan di `include/ui.h`:

```c
void draw_k_nama(HDC, Component *);
```

### 3. Tambahkan tipe

- `COMP_NAMA` → enum `CompType` di `include/types.h`
- `TOOL_NAMA` → enum `ToolKind` di `include/types.h`

### 4. Daftarkan di registry

- `btnTools[]` + `btnLabels[]` → `src/core/model.c`
- Cabang `case COMP_NAMA:` di `draw_component()` → `src/ui/canvas/draw.c`
- Cabang di `toolbar_draw_symbol()` (ikon mini) → `src/ui/toolbar/symbols.c`
- Cabang di `comp_pin_count()` + `comp_pin_pos()` → `src/core/model.c`

### 5. Perilaku listrik (opsional)

Jika komponen punya perilaku listrik, tambahkan edge di `src/core/sim.c`.

> 📄 Panduan lengkap ada di [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md).


## Dukungan dan Bug report 

Buka issue dengan menyertakan:
- Versi Windows & compiler (MinGW gcc / MSVC / clang)
- Langkah reproduksi
- Output make test (kalau ada)


Lisensi
MIT License — lihat [`LICENSE`](LICENSE).

© 2026 Schematic Editor Project.

