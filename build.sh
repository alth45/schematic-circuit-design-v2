#!/usr/bin/env bash
# ============================================================================
#  build.sh — Build wrapper untuk POSIX (Linux / macOS / MSYS / WSL).
#
#  Pengguna tidak butuh make atau CMake — cukup gcc di PATH.
#  Kalau make tersedia, wrapper ini delegasi ke Makefile (Step 27).
#
#  Usage:
#    ./build.sh              build app (default)
#    ./build.sh app          build app (explicit)
#    ./build.sh test         build + jalankan unit test
#    ./build.sh clean        hapus artefak
#    ./build.sh help         tampilkan bantuan ini
#
#  Cross-compile Windows target dari Linux:
#    CC_PROD=x86_64-w64-mingw32-gcc ./build.sh app
# ============================================================================
set -e

# --- Detect compiler ---
CC_PROD="${CC_PROD:-gcc}"
CC_TEST="${CC_TEST:-gcc}"

if ! command -v "$CC_PROD" >/dev/null 2>&1; then
    echo "[ERROR] Compiler '$CC_PROD' tidak ditemukan di PATH." >&2
    echo "        Install gcc atau set CC_PROD=x86_64-w64-mingw32-gcc." >&2
    exit 1
fi

# --- Detect make (opsional) ---
HAS_MAKE=0
if command -v make >/dev/null 2>&1; then
    HAS_MAKE=1
fi

# --- Parse command (default = app) ---
CMD="${1:-app}"

case "$CMD" in
    help|-h|--help) DO=help   ;;
    clean)          DO=clean  ;;
    app)            DO=app    ;;
    test|tests)     DO=test   ;;
    *)
        echo "[ERROR] Perintah tidak dikenal: $CMD" >&2
        DO=help
        ;;
esac

# ============================================================================
#  Target: build app produksi
# ============================================================================
do_app() {
    if [ "$HAS_MAKE" = "1" ]; then
        echo "[build.sh] Delegasi ke make app"
        make app CC_PROD="$CC_PROD"
        return $?
    fi

    echo "[build.sh] Building schematic_win.exe (langsung via $CC_PROD)"

    # Sumber produksi (harus sinkron dengan Makefile SRC_PROD)
    local SRC_CORE="src/core/model.c src/core/sim.c"
    local SRC_IO="src/io/fileio.c src/io/png_writer.c src/io/export_png.c"
    local SRC_AUDIO="src/audio/buzzer_audio.c"
    local SRC_UI="src/ui/window.c \
        src/ui/canvas/draw.c src/ui/canvas/events.c \
        src/ui/canvas/minimap.c src/ui/canvas/viewport.c \
        src/ui/toolbar/menubar.c src/ui/toolbar/symbols.c \
        src/ui/toolbar/actions.c src/ui/toolbar/palette.c \
        src/ui/panels/oscilloscope.c src/ui/panels/splash.c \
        src/ui/dialogs/file_dialogs.c src/ui/dialogs/value_editor.c"
    local SRC_COMP
    SRC_COMP=$(ls src/components/*.c 2>/dev/null || true)

    "$CC_PROD" -O2 -Wall -mwindows -municode \
        -DUNICODE -D_UNICODE -DSCHEMATIC_PRODUCTION=1 \
        -Iinclude \
        src/main.c \
        $SRC_CORE $SRC_IO $SRC_AUDIO $SRC_UI $SRC_COMP \
        -o schematic_win.exe \
        -lgdi32 -luser32 -lcomdlg32 -lwinmm -lm
    echo "[done ] schematic_win.exe"
}

# ============================================================================
#  Target: build + jalankan tests
# ============================================================================
do_test() {
    if [ "$HAS_MAKE" = "1" ]; then
        echo "[build.sh] Delegasi ke make test"
        make test CC_TEST="$CC_TEST"
        return $?
    fi

    echo "[build.sh] Building run_tests (langsung via $CC_TEST)"

    "$CC_TEST" -O1 -Wall -Wextra -Wno-unused-parameter \
        -DUNICODE -D_UNICODE -DSCHEMATIC_TEST_STUBS=1 \
        -Iinclude -include tests/platform_compat.h \
        tests/test_main.c tests/test_model.c tests/test_sim.c \
        tests/test_selection.c tests/test_undo.c tests/test_support.c \
        tests/stubs_audio.c \
        src/core/model.c src/core/sim.c \
        src/io/fileio.c src/io/png_writer.c \
        -o run_tests \
        -lm

    echo "[build.sh] Menjalankan test suite..."
    echo
    local rc=0
    ./run_tests || rc=$?
    echo
    if [ $rc -eq 0 ]; then
        echo "[done ] semua test pass."
    else
        echo "[FAIL ] ada test yang gagal (exit $rc)."
    fi
    return $rc
}

# ============================================================================
#  Target: clean
# ============================================================================
do_clean() {
    if [ "$HAS_MAKE" = "1" ]; then
        make clean
        return $?
    fi
    echo "[build.sh] Membersihkan artefak..."
    rm -f schematic_win.exe run_tests.exe run_tests *.o
    rm -f src/*.o
    rm -f src/core/*.o src/io/*.o src/audio/*.o
    rm -f src/ui/*.o src/ui/canvas/*.o src/ui/toolbar/*.o
    rm -f src/ui/panels/*.o src/ui/dialogs/*.o
    rm -f src/components/*.o tests/*.o
    rm -rf build_tmp
    echo "[done ] bersih."
}

# ============================================================================
#  Help
# ============================================================================
do_help() {
    cat <<'EOF'

  Schematic Editor — build.sh
  ===========================

  Usage:  ./build.sh [command]

    (tanpa argumen)  build app produksi  -> schematic_win.exe
    app              idem (eksplisit)
    test             build + jalankan unit test  -> run_tests
    clean            hapus hasil build
    help             tampilkan pesan ini

  Cross-compile Windows target dari Linux:
    CC_PROD=x86_64-w64-mingw32-gcc ./build.sh app

  Catatan:
    - Kalau `make` ada di PATH, wrapper delegasi ke Makefile (Step 27).
    - Kalau tidak, memanggil gcc langsung dengan flag yang sama.
    - `test` target jalan native (Linux/macOS), tanpa Win32.

EOF
}

# ============================================================================
#  Dispatch
# ============================================================================
case "$DO" in
    app)   do_app ;;
    test)  do_test ;;
    clean) do_clean ;;
    help)  do_help ;;
esac

exit $?