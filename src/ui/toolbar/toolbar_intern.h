#ifndef TOOLBAR_INTERN_H
#define TOOLBAR_INTERN_H

/* ============================================================================
   src/ui/toolbar/toolbar_intern.h — Header internal modul toolbar.

   TIDAK diekspos ke include/ — pemakainya hanya file-file di dalam
   src/ui/toolbar/ sendiri (menubar.c, symbols.c, actions.c, palette.c).
   Konsumen luar (main.c, window.c, draw.c, dll) tidak boleh include ini.

   Include "ui.h" menarik <windows.h> (untuk HDC), ToolKind/CompType
   (types.h), dan deklarasi API publik UI. Tidak ada simbol baru yang
   dideklarasikan di sini selain helper internal modul toolbar.
   ============================================================================ */

#include "ui.h"

/* toolbar/symbols.c */
void toolbar_draw_symbol(HDC hdc, ToolKind tool, int cx, int cy, BOOL active);
const wchar_t *toolbar_tool_shortname(ToolKind tool);
int toolbar_tool_is_component(ToolKind tool);

/* toolbar/actions.c */
void palette_draw_clear(HDC hdc, int bx, int by);
void palette_draw_save(HDC hdc, int bx, int by);
void palette_draw_open(HDC hdc, int bx, int by);
void palette_draw_run(HDC hdc, int bx, int by);

#endif /* TOOLBAR_INTERN_H */