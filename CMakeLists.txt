# ============================================================================
#  CMakeLists.txt — Schematic Editor (Win32 GDI) + Unit Test Suite
#
#  Paralel dengan Makefile (lihat Step 27). Pilih salah satu:
#      make app / make test              (Unix/Make)
#      cmake -B build && cmake --build build   (portable / MSVC / VS Code)
#
#  Target:
#      schematic_win  → aplikasi produksi (hanya di Windows; WIN32 GUI)
#      run_tests      → unit test (pure modules; cross-platform)
#
#  Konvensi sama dengan Makefile:
#    * Produksi:  -DSCHEMATIC_PRODUCTION=1, TIDAK PERNAH menyentuh tests/
#    * Test:      -DSCHEMATIC_TEST_STUBS=1, HANYA src/core/ + src/io/(pure)
#                 + tests/. -include tests/platform_compat.h untuk shim.
# ============================================================================
cmake_minimum_required(VERSION 3.15)
project(schematic_editor C)

# Tolak in-source build — CMake cache & artefak berantakan di source tree.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source build tidak diizinkan.\n"
        "Gunakan:  cmake -B build  &&  cmake --build build")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ============================================================================
#  Kumpulan sumber (dipakai oleh kedua target)
# ============================================================================
set(SRC_CORE
    src/core/model.c
    src/core/sim.c
)

# Pure I/O: JSON save/load + PNG encoder — nol <windows.h>.
set(SRC_IO_PURE
    src/io/fileio.c
    src/io/png_writer.c
)

# Butuh GDI (render canvas → bitmap) — hanya untuk produksi.
set(SRC_IO_GDI
    src/io/export_png.c
)

set(SRC_AUDIO
    src/audio/buzzer_audio.c
)

set(SRC_UI
    src/ui/window.c
    src/ui/canvas/draw.c
    src/ui/canvas/events.c
    src/ui/canvas/minimap.c
    src/ui/canvas/viewport.c
    src/ui/toolbar/menubar.c
    src/ui/toolbar/symbols.c
    src/ui/toolbar/actions.c
    src/ui/toolbar/palette.c
    src/ui/panels/oscilloscope.c
    src/ui/panels/splash.c
    src/ui/dialogs/file_dialogs.c
    src/ui/dialogs/value_editor.c
)

# Wildcard komponen: penambahan simbol baru otomatis ikut build.
# CONFIGURE_DEPENDS agar cmake re-glob saat ada file baru (CMake 3.12+).
file(GLOB SRC_COMP CONFIGURE_DEPENDS "src/components/*.c")

# ============================================================================
#  Target 1: aplikasi produksi (hanya Windows)
# ============================================================================
if(WIN32)
    add_executable(schematic_win WIN32
        src/main.c
        ${SRC_CORE}
        ${SRC_IO_PURE}
        ${SRC_IO_GDI}
        ${SRC_AUDIO}
        ${SRC_UI}
        ${SRC_COMP}
    )

    target_include_directories(schematic_win PRIVATE include)

    target_compile_definitions(schematic_win PRIVATE
        UNICODE
        _UNICODE
        SCHEMATIC_PRODUCTION=1
    )

    target_link_libraries(schematic_win PRIVATE
        gdi32 user32 comdlg32 winmm m
    )

    if(MINGW)
        # MinGW: -municode memilih wWinMainCRTStartup + efek _UNICODE.
        target_compile_options(schematic_win PRIVATE -municode -O2 -Wall)
        target_link_options(schematic_win PRIVATE -municode -mwindows)
    elseif(MSVC)
        # MSVC: /SUBSYSTEM:WINDOWS sudah otomatis via WIN32 flag +
        # entry dipilih otomatis (wWinMain) karena UNICODE/_UNICODE.
        target_compile_options(schematic_win PRIVATE /W3)
    endif()
endif()

# ============================================================================
#  Target 2: unit test (cross-platform — Windows & Linux)
# ============================================================================
set(SRC_TEST
    tests/test_main.c
    tests/test_model.c
    tests/test_sim.c
    tests/test_selection.c
    tests/test_undo.c
    tests/test_support.c
    tests/stubs_audio.c
    ${SRC_CORE}
    ${SRC_IO_PURE}
)

add_executable(run_tests ${SRC_TEST})

target_include_directories(run_tests PRIVATE include)

target_compile_definitions(run_tests PRIVATE
    UNICODE
    _UNICODE
    SCHEMATIC_TEST_STUBS=1
)

# Shim _wfopen/_vsnwprintf — hanya di non-Windows (MinGW/MSVC native sudah
# punya). `-include` memaksa pre-include ke setiap TU tanpa mengubah source.
if(NOT WIN32)
    target_compile_options(run_tests PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/tests/platform_compat.h
    )
endif()

target_link_libraries(run_tests PRIVATE m)

if(MSVC)
    target_compile_options(run_tests PRIVATE /W4)
else()
    target_compile_options(run_tests PRIVATE
        -O1 -Wall -Wextra -Wno-unused-parameter)
endif()

# ============================================================================
#  CTest integration
# ============================================================================
enable_testing()
add_test(NAME unit COMMAND run_tests)

# ============================================================================
#  Guard: mode test tidak boleh bocor ke produksi
# ============================================================================
if(CMAKE_C_FLAGS MATCHES "SCHEMATIC_TEST_STUBS")
    message(FATAL_ERROR
        "[ERROR] CMAKE_C_FLAGS memuat SCHEMATIC_TEST_STUBS — "
        "itu mode TES, bukan produksi.")
endif()