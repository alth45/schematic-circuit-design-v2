# CONTRIBUTING — Schematic Editor

Terima kasih sudah tertarik berkontribusi. Dokumen ini menjelaskan
**konvensi kode**, **alur development**, dan **checklist PR**.

Untuk pemahaman struktur, baca [`ARCHITECTURE.md`](ARCHITECTURE.md) dulu.

---

## 1. Setup development

### Windows (target produksi)

1. Install **MinGW-w64** (gcc) — rekomendasi: MSYS2 `mingw-w64-x86_64-gcc`.
2. Pastikan `gcc --version` jalan di Command Prompt / PowerShell.
3. (Opsional) install **make** dan/atau **CMake** untuk workflow lebih nyaman.
4. Clone repo, lalu:
   ```cmd
   build.bat          :: build app
   build.bat test     :: build + jalankan test

### Atau dengan make:
---

```sh
make app
make test
```
### Linux (untuk test suite)
---
```sh
sudo apt install build-essential  # atau padanan distro Anda
make test
./build.sh test
```
### Cross-compile Windows dari Linux (kalau ingin build .exe):

---
```sh 
sudo apt install mingw-w64
CC_PROD=x86_64-w64-mingw32-gcc make app
```

## 2. Alur Development

### Langkah

1. **Fork** repo & buat branch dari `main`:

   ```bash
   git checkout -b feat/nama-fitur
   # atau
   git checkout -b fix/nama-bug
   ```

2. **Tulis perubahan** — pastikan `make test` hijau sebelum commit.

3. **Commit** dengan pesan yang jelas *(lihat §5)*.

4. **Push** dan buka **Pull Request**.

---

## 3. Aturan Include — ⚠️ WAJIB

> Ini aturan **paling penting**. Pelanggaran menyebabkan `core` tidak pure
> dan **build test gagal**.

### Tabel Aturan

| File | Boleh `#include` | Catatan |
|------|------------------|---------|
| `src/core/*.c` | `model.h`, `sim.h`, `audio.h`, CRT standar | ❌ **Tidak boleh** `<windows.h>` atau `ui.h` |
| `src/io/fileio.c`, `src/io/png_writer.c` | `io.h` + CRT | ❌ **Tidak boleh** `<windows.h>` |
| `src/io/export_png.c` | `ui.h`, `io.h` | ✅ Boleh `<windows.h>` *(via `ui.h`)* |
| `src/audio/buzzer_audio.c` | `audio.h`, `model.h`, `sim.h`, `<windows.h>`, `<mmsystem.h>` | Platform-specific |
| `src/ui/**` | `ui.h` | *(membawa `<windows.h>` transitif)* |
| `src/components/**` | `ui.h` | — |
| `src/main.c` | `ui.h` | — |
| `tests/**` | `test_support.h`, `model.h`, `sim.h`, `audio.h` | ❌ **Tidak boleh** `ui.h` |

### Test Cepat Sebelum Commit

```bash
# Cek file core tidak menyentuh Win32
grep -n "windows.h\|HWND\|HDC\|HINSTANCE\|LPCWSTR\|LONG_PTR" src/core/*.c
# Expected: nol hasil

# Cek io pure tidak menyentuh Win32
grep -n "windows.h\|HWND\|HDC" src/io/fileio.c src/io/png_writer.c
# Expected: nol hasil
```

> 💡 Kalau menemukan include yang melanggar, **pindahkan fungsionalitas ke layer yang tepat** — jangan longgarkan aturan.

## 4. Coding Style

> Gaya **C11** konsisten — bukan style guide korporat.
> Yang penting: **konsisten dengan file yang sudah ada.**

### Aturan Singkat

| Aspek | Aturan |
|-------|--------|
| Indentasi | 4 spasi *(bukan tab)* |
| Brace fungsi | Allman-ish — `{` di baris baru |
| Brace control | K&R — `if (x) {` di baris sama |
| Panjang baris | Target ≤ 88 kolom *(tidak wajib)* |
| Tipe | `int`, `double`, `long` — hindari `int8_t`/`uint32_t` kecuali perlu |
| Konstanta | `SCREAMING_SNAKE_CASE` via `#define` |
| Fungsi publik | `snake_case` + prefix modul: `model_*`, `sim_*`, `draw_k_*`, `toolbar_*`, `minimap_*`, `osc_*` |
| Fungsi statis | `snake_case` tanpa prefix |

### ✅ Do / ❌ Don't

```c
/* ✅ Do — brace fungsi di baris baru, control K&R */
int model_add_wire(PinRef a, PinRef b) {
    if (same_pin(a, b)) return 0;
    return 1;
}

/* ❌ Don't — brace fungsi sebaris, tab, int8_t tanpa alasan */
int model_add_wire(PinRef a, PinRef b) {
	/* tab indent */
	int8_t ok = 1;  /* pakai int, bukan int8_t */
	return ok;
}
```

### 📝 Contoh Fungsi Lengkap

```c
/* Deskripsi singkat fungsi + kontrak (return, side effect). */
int model_add_wire(PinRef a, PinRef b) {
    if (same_pin(a, b)) return 0;

    /* Validasi endpoint. */
    if (a.comp_id < 0) {
        if (!find_junction_by_id(a.pin_idx)) return 0;
    } else {
        Component *ca = find_comp_by_id(a.comp_id);
        if (!ca || !comp_pin_valid(ca, a.pin_idx)) return 0;
    }

    /* Duplikat check. */
    for (int i = 0; i < wire_count; i++) {
        if (same_pin(wires[i].a, a) && same_pin(wires[i].b, b))
            return 0;
    }

    undo_push();
    wires[wire_count].a = a;
    wires[wire_count].b = b;
    wire_count++;
    sim_recompute();
    return 1;
}
```


## Komentar

| Aspek | Aturan |
|-------|--------|
| **Bahasa** | Indonesia *(konsisten dengan kode yang ada)* |
| **Fokus** | Jelaskan **"kenapa"**, bukan "apa" — kode sudah cukup jelas untuk "apa" |
| **Fungsi publik** | Blok komentar kontrak di header: parameter, return, side effect |
| **Hindari** | Komentar redundan seperti `/* increment i */ i++;` |

---

## Memory & Ownership

| Layer | Aturan Alokasi |
|-------|----------------|
| `core/` `sim/` `io/` | `malloc` / `free` *(hanya di file yang sudah ada)* |
| `ui/` | ❌ Tidak boleh `malloc` jangka panjang — pakai buffer stack atau `HeapAlloc`/`HeapFree` *(Win32 idiom)* |

> 📌 Kalau menambah fungsi yang **return pointer**, dokumentasikan **ownership** di komentar fungsi.

---

## Error Handling

| Layer | Konvensi |
|-------|----------|
| **Core / io** | Return `int` — `0` = gagal, `1` = sukses. Atau `NULL` untuk pointer. ❌ Tidak ada `assert` di produksi. |
| **UI** | `MessageBoxW` untuk error user-visible; `set_status` untuk status bar. |
| **Sim** | ❌ Tidak ada error return — panggilan simulasi **idempoten**. Kalau ada bug di state, hasilnya flag salah, **bukan crash**. |


## 5. Commit Messages

> Format: **Conventional Commits ringan**

```
<type>(<scope>): <deskripsi singkat>

<body opsional>
```

### Tipe Commit

| Tipe | Kegunaan |
|------|----------|
| `feat` | Fitur baru |
| `fix` | Bug fix |
| `refactor` | Perubahan struktur tanpa ubah perilaku |
| `docs` | Dokumentasi saja |
| `test` | Test baru atau perbaikan test |
| `build` | `Makefile`, `CMake`, build scripts |
| `style` | Formatasi, spasi *(nol perubahan logika)* |

### Scope *(opsional)*

Nama modul: `core`, `sim`, `io`, `ui`, `toolbar`, `components`, dll.

### Contoh

```
feat(components): tambah komponen TRIAC
fix(sim): short detection salah untuk gate NOT feedback
refactor(core): pindah edit_value_dialog ke ui/dialogs
docs: update shortcut list untuk F12
test(sim): tambah coverage untuk zener breakdown
```

## Menambah Komponen Baru

Ikuti **9 langkah** berikut:

### 1. Buat file komponen — `src/components/nama.c`

```c
#include "ui.h"

static void draw_nama(HDC hdc, int x, int y, BOOL sel) {
    /* Gambar simbol. Skala default: pin0 di (-40, 0),
       pin1 di (+40, 0). Untuk 3-pin, lihat transistor.c
       atau pot.c sebagai referensi. */
}

void draw_k_nama(HDC hdc, Component *c) {
    with_rotation(hdc, c, draw_nama);
}
```

> 💡 Kalau butuh **state RUN** *(misal LED menyala hijau)*, pakai pola wrapper
> `static int` + `xxx_at()` seperti di `komponen_led.c`.

### 2. Deklarasikan `draw_k_nama()`

Tambahkan `void draw_k_nama(HDC, Component *)` di `include/ui.h` *(bagian BAGIAN 4)*.

### 3. Tambah `COMP_NAMA`

Di `include/types.h` — letakkan di grup yang sesuai:
**dasar / sumber / alat ukur / gate**.

### 4. Tambah `TOOL_NAMA`

Di `include/types.h` — **urutan mengikuti tombol toolbar**.

### 5. Daftarkan di registry

`btnTools[]` dan `btnLabels[]` → `src/core/model.c`.

### 6. Tambah cabang di `draw_component()`

Di `src/ui/canvas/draw.c`:

```c
case COMP_NAMA: draw_k_nama(hdc, c); break;
```

### 7. Tambah ikon toolbar

Di `src/ui/toolbar/symbols.c`:

- Cabang ikon di `toolbar_draw_symbol()` — untuk ikon mini di toolbar
- Cabang di `toolbar_tool_shortname()` — label 2–4 huruf

### 8. Tambah pin handling

Di `src/core/model.c`:

- `comp_pin_count()` — tentukan **berapa pin**
- `comp_pin_pos()` — tentukan **posisi pin**

### 9. *(Opsional)* Perilaku listrik

Kalau komponen punya perilaku listrik *(edge/gate/sumber)*:

| Kasus | File & fungsi |
|-------|---------------|
| Edge resistif | `comp_edge_r()` → `src/core/sim.c` |
| Sumber tegangan | `is_dc_source` / `is_ac_source` + `na_assign_fixed_mode()` |
| Sensor / switch | `build_unions()` dan/atau `sim_recompute()` |

---

> 🛠️ **Build otomatis:** wildcard (`Makefile`) / `GLOB` (`CMake`) akan menemukan
> `src/components/nama.c` baru — **tidak perlu edit build system**.
>
> 🧪 **Test:** tambah test baru di `tests/test_model.c` *(kalau menambah logika pin)*
> atau `tests/test_sim.c` *(kalau menambah model listrik)*.


## 7. Testing Workflow

### Sebelum Setiap Commit

```bash
make test
```

> Expected: `=== Summary: N passed, 0 failed ===`

### Sebelum PR

| # | Langkah | Perintah |
|---|---------|----------|
| 1 | Build produksi | `make clean && make app` |
| 2 | Test suite | `make clean && make test` |
| 3 | Manual smoke test | *(lihat checklist di bawah)* |

### Manual Smoke Test

Buka `schematic_win.exe`, lalu coba:

- [ ] **Rangkaian dasar** — tambah resistor + baterai + LED, sambung jadi loop
- [ ] **Simulasi normal** — tekan `F5` → LED menyala hijau, banner kuning **"Rangkaian BENAR"**
- [ ] **Simulasi error** — tambah resistor dengan nilai terlalu kecil → LED meledak *(banner merah)*
- [ ] **Persistence** — Save JSON, close, open lagi → rangkaian kembali
- [ ] **Export** — Export PNG → file terbuka di viewer *(browser juga bisa)*
- [ ] **Osiloskop** — buka `F12` setelah tambah OSC probe → trace tampil

### Aturan Test Coverage

| Situasi | Wajib Test? |
|---------|:-----------:|
| Bug fix | ✅ Selalu *(regression test)* |
| Logika baru di `core/` | ✅ Selalu *(testable)* |
| Perubahan UI | ⬜ Opsional *(tidak testable tanpa Win32)* |


## 8. Checklist PR

Sebelum klik **"Create Pull Request"**:

### Build & Test

- [ ] `make test` hijau *(atau `./build.sh test`)*
- [ ] `make app` sukses *(atau `build.bat`)*

### Kualitas Kode

- [ ] `grep -n "windows.h" src/core/*.c src/io/fileio.c src/io/png_writer.c` → **kosong**
- [ ] Commit message mengikuti format *(§5)*
- [ ] Tidak ada `printf` / `fprintf` debug tersisa
- [ ] Tidak ada `TODO` baru tanpa penjelasan di PR description

### Dokumentasi & Kontrak

- [ ] Kalau menambah fitur → update `README.md` bagian **Shortcut / Fitur** bila perlu
- [ ] Kalau menambah `.c` di `src/components/` → **tidak perlu** ubah build system *(wildcard/GLOB)*
- [ ] Kalau mengubah kontrak publik di header → catat di PR description *(breaking change atau tidak)*


## 9. Pertanyaan?

Ada pertanyaan? Silakan lewat salah satu jalur berikut:

| Jalur | Cara |
|-------|------|
| 🐛 Issue | Buka issue dengan label `question` |
| 💬 Diskusi | Hubungi maintainer via diskusi repo |

> ⚠️ **Kalau tidak yakin apakah sesuatu termasuk pure core atau UI — tanya dulu sebelum menulis kode.**
>
> Refactor setelah PR lebih mahal daripada diskusi sebelum mulai.