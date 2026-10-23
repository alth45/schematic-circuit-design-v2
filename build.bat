@echo off
REM ============================================================================
REM  build.bat — Build wrapper untuk Windows (MinGW gcc).
REM
REM  Pengguna tidak butuh make atau CMake — cukup gcc.exe di PATH.
REM  Kalau make tersedia, wrapper ini delegasi ke Makefile (Step 27).
REM
REM  Usage:
REM    build.bat              build app (default)
REM    build.bat app          build app (explicit)
REM    build.bat test         build + jalankan unit test
REM    build.bat clean        hapus artefak
REM    build.bat help         tampilkan bantuan ini
REM ============================================================================

setlocal EnableDelayedExpansion

REM --- Detect gcc ---
where gcc >nul 2>nul
if errorlevel 1 (
    echo [ERROR] gcc tidak ditemukan di PATH.
    echo         Install MinGW-w64 + tambahkan folder bin ke PATH.
    exit /b 1
)

REM --- Detect make (opsional, delegasi kalau ada) ---
set HAS_MAKE=0
where make >nul 2>nul
if not errorlevel 1 set HAS_MAKE=1

REM --- Parse command (default = app) ---
set CMD=%1
if "%CMD%"=="" set CMD=app

if /I "%CMD%"=="help"   goto :help
if /I "%CMD%"=="-h"     goto :help
if /I "%CMD%"=="--help" goto :help
if /I "%CMD%"=="clean"  goto :clean
if /I "%CMD%"=="app"    goto :app
if /I "%CMD%"=="test"   goto :test
if /I "%CMD%"=="tests"  goto :test

echo [ERROR] Perintah tidak dikenal: %CMD%
goto :help

REM ============================================================================
REM  Target: build app produksi
REM ============================================================================
:app
if "%HAS_MAKE%"=="1" (
    echo [build.bat] Delegasi ke make app
    make app
    exit /b %errorlevel%
)

echo [build.bat] Building schematic_win.exe (langsung via gcc)

REM Sumber produksi (harus sinkron dengan Makefile SRC_PROD)
set SRC_CORE=src\core\model.c src\core\sim.c
set SRC_IO=src\io\fileio.c src\io\png_writer.c src\io\export_png.c
set SRC_AUDIO=src\audio\buzzer_audio.c
set SRC_UI=src\ui\window.c ^
    src\ui\canvas\draw.c src\ui\canvas\events.c ^
    src\ui\canvas\minimap.c src\ui\canvas\viewport.c ^
    src\ui\toolbar\menubar.c src\ui\toolbar\symbols.c ^
    src\ui\toolbar\actions.c src\ui\toolbar\palette.c ^
    src\ui\panels\oscilloscope.c src\ui\panels\splash.c ^
    src\ui\dialogs\file_dialogs.c src\ui\dialogs\value_editor.c

REM Komponen (wildcard via for loop)
set SRC_COMP=
for %%F in (src\components\*.c) do set SRC_COMP=!SRC_COMP! %%F

gcc -O2 -Wall -mwindows -municode ^
    -DUNICODE -D_UNICODE -DSCHEMATIC_PRODUCTION=1 ^
    -Iinclude ^
    src\main.c %SRC_CORE% %SRC_IO% %SRC_AUDIO% %SRC_UI% !SRC_COMP! ^
    -o schematic_win.exe ^
    -lgdi32 -luser32 -lcomdlg32 -lwinmm -lm

if errorlevel 1 (
    echo [ERROR] Build gagal.
    exit /b 1
)
echo [done ] schematic_win.exe
exit /b 0

REM ============================================================================
REM  Target: build + jalankan tests
REM ============================================================================
:test
if "%HAS_MAKE%"=="1" (
    echo [build.bat] Delegasi ke make test
    make test
    exit /b %errorlevel%
)

echo [build.bat] Building run_tests.exe (langsung via gcc)

REM Test hanya butuh pure modules — tidak butuh Win32.
gcc -O1 -Wall -Wextra -Wno-unused-parameter ^
    -DUNICODE -D_UNICODE -DSCHEMATIC_TEST_STUBS=1 ^
    -Iinclude -include tests\platform_compat.h ^
    tests\test_main.c tests\test_model.c tests\test_sim.c ^
    tests\test_selection.c tests\test_undo.c tests\test_support.c ^
    tests\stubs_audio.c ^
    src\core\model.c src\core\sim.c ^
    src\io\fileio.c src\io\png_writer.c ^
    -o run_tests.exe ^
    -lm

if errorlevel 1 (
    echo [ERROR] Build test gagal.
    exit /b 1
)

echo [build.bat] Menjalankan test suite...
echo.
run_tests.exe
set TEST_RC=%errorlevel%
echo.
if %TEST_RC%==0 (
    echo [done ] semua test pass.
) else (
    echo [FAIL ] ada test yang gagal ^(exit %TEST_RC%^).
)
exit /b %TEST_RC%

REM ============================================================================
REM  Target: clean
REM ============================================================================
:clean
if "%HAS_MAKE%"=="1" (
    make clean
    exit /b %errorlevel%
)
echo [build.bat] Membersihkan artefak...
del /Q schematic_win.exe run_tests.exe 2>nul
del /Q *.o 2>nul
del /Q src\*.o src\core\*.o src\io\*.o src\audio\*.o 2>nul
del /Q src\ui\*.o src\ui\canvas\*.o src\ui\toolbar\*.o 2>nul
del /Q src\ui\panels\*.o src\ui\dialogs\*.o 2>nul
del /Q src\components\*.o tests\*.o 2>nul
if exist build_tmp rmdir /S /Q build_tmp
echo [done ] bersih.
exit /b 0

REM ============================================================================
REM  Help
REM ============================================================================
:help
echo.
echo  Schematic Editor — build.bat
echo  ============================
echo.
echo  Usage:  build.bat [command]
echo.
echo    (tanpa argumen)  build app produksi  -^> schematic_win.exe
echo    app              idem ^(eksplisit^)
echo    test             build + jalankan unit test  -^> run_tests.exe
echo    clean            hapus hasil build
echo    help             tampilkan pesan ini
echo.
echo  Catatan:
echo    - Kalau `make` ada di PATH, wrapper delegasi ke Makefile ^(Step 27^).
echo    - Kalau tidak, memanggil gcc langsung dengan flag yang sama.
echo    - Butuh MinGW-w64 gcc ^(Windows target^).
echo.
exit /b 0