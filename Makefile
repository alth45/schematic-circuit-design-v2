# ============================================================================
#  Makefile — Schematic Editor (Win32 GDI) + Unit Test Suite
#
#  Pemisah ketat TES vs PRODUKSI:
#    * app   : -DSCHEMATIC_PRODUCTION=1, TIDAK PERNAH menyentuh tests/.
#    * test  : -DSCHEMATIC_TEST_STUBS=1, HANYA src/core/ + src/io/ (pure)
#              + tests/. Tidak butuh Win32 (bisa jalan di Linux tanpa MinGW).
#
#  Struktur folder (Step 1-26 refactor):
#    include/           header publik (config, types, model, sim, io, audio, ui)
#    src/core/          model.c, sim.c               ← PURE (no <windows.h>)
#    src/io/            fileio.c, png_writer.c       ← PURE
#                       export_png.c                 ← butuh GDI (Win32)
#    src/audio/         buzzer_audio.c               ← butuh <mmsystem.h>
#    src/ui/            window.c, canvas/, toolbar/, panels/, dialogs/
#    src/components/    20 file simbol (auto-wildcard)
#    tests/             test_*.c + stubs_audio.c + platform_compat.h
# ============================================================================

# --- Toolchain (bisa di-override dari command line / env) ---
CC      ?= gcc
CC_PROD ?= $(CC)
CC_TEST ?= $(CC)

# --- Output ---
# APP selalu .exe (Windows target, mungkin cross-compiled).
APP := schematic_win.exe
# TEST_BIN native: tanpa .exe di Linux.
ifeq ($(OS),Windows_NT)
TEST_BIN := run_tests.exe
else
TEST_BIN := run_tests
endif

# --- Flags: produksi (Windows GUI) ---
CFLAGS_PROD  := -O2 -Wall -mwindows -municode \
                -DUNICODE -D_UNICODE -DSCHEMATIC_PRODUCTION=1 \
                -Iinclude
LDFLAGS_PROD := -lgdi32 -luser32 -lcomdlg32 -lwinmm -lm

# --- Flags: test ---
# `-include tests/platform_compat.h` menyediakan shim _wfopen/_vsnwprintf
# untuk Linux. Di Windows header ini kosong (native MSVCRT dipakai), jadi
# flag aman di kedua platform. TIDAK pakai -municode: test pakai main().
CFLAGS_TEST  := -O1 -Wall -Wextra -Wno-unused-parameter \
                -DUNICODE -D_UNICODE -DSCHEMATIC_TEST_STUBS=1 \
                -Iinclude -include tests/platform_compat.h
LDFLAGS_TEST := -lm

# --- Sumber: produksi ---
SRC_CORE  := src/core/model.c src/core/sim.c
SRC_IO    := src/io/fileio.c src/io/png_writer.c src/io/export_png.c
SRC_AUDIO := src/audio/buzzer_audio.c
SRC_UI    := src/ui/window.c \
             src/ui/canvas/draw.c src/ui/canvas/events.c \
             src/ui/canvas/minimap.c src/ui/canvas/viewport.c \
             src/ui/toolbar/menubar.c src/ui/toolbar/symbols.c \
             src/ui/toolbar/actions.c src/ui/toolbar/palette.c \
             src/ui/panels/oscilloscope.c src/ui/panels/splash.c \
             src/ui/dialogs/file_dialogs.c src/ui/dialogs/value_editor.c
SRC_COMP  := $(wildcard src/components/*.c)

SRC_PROD := src/main.c $(SRC_CORE) $(SRC_IO) $(SRC_AUDIO) $(SRC_UI) $(SRC_COMP)

# --- Sumber: test (pure modules + harness + audio stub) ---
SRC_TEST := tests/test_main.c tests/test_model.c tests/test_sim.c \
            tests/test_selection.c tests/test_undo.c tests/test_support.c \
            tests/stubs_audio.c \
            src/core/model.c src/core/sim.c \
            src/io/fileio.c src/io/png_writer.c

# ============================================================================
#  Guard: mode test tidak boleh bocor ke build produksi
# ============================================================================
ifneq (,$(findstring SCHEMATIC_TEST_STUBS,$(CPPFLAGS) $(CFLAGS) $(CFLAGS_PROD)))
$(error [ERROR] CFLAGS memuat SCHEMATIC_TEST_STUBS — itu mode TES, bukan produksi)
endif

# ============================================================================
#  Target default = aplikasi produksi
# ============================================================================
.PHONY: all app rebuild tests test clean help check-prod-guard

all: app

app: check-prod-guard $(APP)

rebuild: clean app

$(APP): $(SRC_PROD) $(wildcard include/*.h)
	@echo "[build] $(APP) (production)"
	$(CC_PROD) $(CFLAGS_PROD) $(SRC_PROD) -o $(APP) $(LDFLAGS_PROD)
	@echo "[done ] $(APP)"

check-prod-guard:
	@echo "[check] mode produksi OK (tanpa SCHEMATIC_TEST_STUBS)"

# ============================================================================
#  Suite unit test
# ============================================================================
tests: $(TEST_BIN)

$(TEST_BIN): $(SRC_TEST) $(wildcard include/*.h) $(wildcard tests/*.h)
	@echo "[build] $(TEST_BIN) (test)"
	$(CC_TEST) $(CFLAGS_TEST) $(SRC_TEST) -o $(TEST_BIN) $(LDFLAGS_TEST)
	@echo "[done ] $(TEST_BIN)"

test: $(TEST_BIN)
	@echo
	@./$(TEST_BIN)

# ============================================================================
#  Bersih-bersih
# ============================================================================
ifeq ($(OS),Windows_NT)
# clean:
# 	-del /Q $(APP) $(TEST_BIN) 2>NUL
# 	-del /Q *.o 2>NUL
# 	-del /Q src\core\*.o src\io\*.o src\audio\*.o 2>NUL
# 	-del /Q src\ui\*.o src\ui\canvas\*.o src\ui\toolbar\*.o 2>NUL
# 	-del /Q src\ui\panels\*.o src\ui\dialogs\*.o 2>NUL
# 	-del /Q src\components\*.o tests\*.o 2>NUL
# 	-if exist build_tmp rmdir /S /Q build_tmp
clean:
	rm -f schematic_win.exe run_tests.exe
	rm -f *.o
	rm -f src/core/*.o src/io/*.o src/audio/*.o
	rm -f src/ui/*.o src/ui/canvas/*.o src/ui/toolbar/*.o
	rm -f src/ui/panels/*.o src/ui/dialogs/*.o
	rm -f src/components/*.o tests/*.o
	rm -rf build_tmp

else
clean:
	rm -f $(APP) $(TEST_BIN) *.o
	rm -f src/*.o
	rm -f src/core/*.o src/io/*.o src/audio/*.o
	rm -f src/ui/*.o src/ui/canvas/*.o src/ui/toolbar/*.o
	rm -f src/ui/panels/*.o src/ui/dialogs/*.o
	rm -f src/components/*.o tests/*.o
	rm -rf build_tmp
endif

# ============================================================================
#  Help
# ============================================================================
help:
	@echo "Target yang tersedia:"
	@echo "  make / make app   build aplikasi produksi  -> $(APP)"
	@echo "  make rebuild      clean + build ulang"
	@echo "  make tests        build binary tes         -> $(TEST_BIN)"
	@echo "  make test         build + jalankan binary tes"
	@echo "  make clean        hapus hasil build"
	@echo "  make help         tampilkan bantuan ini"
	@echo ""
	@echo "Override toolchain:"
	@echo "  make CC_PROD=i686-w64-mingw32-gcc app  (cross-compile Windows dari Linux)"
	@echo "  make CC_PROD=x86_64-w64-mingw32-gcc app"
	@echo "  make CC_TEST=gcc                       (native test build)"