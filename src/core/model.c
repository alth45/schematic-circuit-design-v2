/* ============================================================================
   src/core/model.c — Data model + operasi edit + undo/redo + viewport.
   
   File ini PURE: nol <windows.h>, nol Win32 API. Bisa di-compile di Linux
   untuk unit test tanpa MinGW. Semua fungsi yang butuh Win32 (dialog,
   zoom_center) sudah dipindah ke:
       edit_value_dialog    → src/ui/dialogs/value_editor.c
       canvas_zoom_center   → src/ui/canvas/viewport.c
   
   Include: <math.h>, <stdarg.h>, <stdio.h>, <wchar.h>, "model.h", "sim.h".
   ============================================================================ */

#include <math.h>
#include <stdarg.h>   /* va_list untuk set_status() */
#include <stdio.h>    /* _vsnwprintf */
#include <wchar.h>    /* swprintf, wcstod, wcschr */

#include "model.h"
#include "sim.h"

#include<stdio.h>


/* ===================== State Global ===================== */
Component comps[MAX_COMP];
int comp_count = 0;
int next_id = 1;
ToolKind current_tool = TOOL_SELECT;
int selected_id = -1;
int dragging_id = -1;
int drag_dx = 0, drag_dy = 0;

Wire wires[MAX_WIRES];
int wire_count = 0;
/* ---- Junctions (T-junction wire-to-wire) ---- */
Junction junctions[MAX_JUNCTIONS];
int junction_count = 0;

int wiring_active = 0;
PinRef wiring_from = {-1, -1};
int wiring_mx = 0, wiring_my = 0;

/* ---- Multi-select / blok rangkaian ---- */
int selection[MAX_SEL];
int sel_count = 0;
int box_selecting = 0;
int box_x1 = 0, box_y1 = 0, box_x2 = 0, box_y2 = 0;
int group_dragging = 0;
GroupDragItem group_drag_items[MAX_GROUP_DRAG];
int group_drag_n = 0;
int group_drag_ox = 0, group_drag_oy = 0;

/* Preferensi tampilan (diubah lewat menu View) */
int g_show_grid = 1;
int g_show_status = 1;

/* ===================== Undo / Redo (snapshot stack) =====================
   Snapshot = comps + wires + next_id + selection. Bounded UNDO_DEPTH=64;
   setiap push baru menghapus stack redo. Semua restore diikuti clear
   status drag/box/wiring + sim_recompute agar tampilan & simulasi konsisten.

   ATURAN BATCH:
     - undo_push() di dalam batch (undo_batch > 0) hanya mengambil snapshot
       pada push PERTAMA. Push berikutnya di-skip.
     - undo_begin_batch()/undo_end_batch() HARUS seimbang. Jangan panggil
       undo_end_batch() tanpa pasangan undo_begin_batch() — itu akan
       membocorkan state batch ke pemanggil lain.
     - Fungsi "remove_*" internal boleh memanggil undo_push(); bila
       dipanggil dari dalam batch (mis. model_delete_selection) push
       otomatis di-skip. */
#define UNDO_DEPTH 64
typedef struct {
  Component comps[MAX_COMP];
  int comp_count;
  Wire wires[MAX_WIRES];
  int wire_count;
  int next_id;
  int selection[MAX_SEL];
  int sel_count;
  int selected_id;
} UndoSnap;
static UndoSnap undo_stack[UNDO_DEPTH];
static int undo_top = 0;
static UndoSnap redo_stack[UNDO_DEPTH];
static int redo_top = 0;
static int undo_batch = 0;
static int undo_batch_pushed = 0;

static void undo_capture(UndoSnap *s) {
  for (int i = 0; i < comp_count; i++)
    s->comps[i] = comps[i];
  s->comp_count = comp_count;
  for (int i = 0; i < wire_count; i++)
    s->wires[i] = wires[i];
  s->wire_count = wire_count;
  s->next_id = next_id;
  for (int i = 0; i < sel_count && i < MAX_SEL; i++)
    s->selection[i] = selection[i];
  s->sel_count = sel_count;
  s->selected_id = selected_id;
}

static void undo_restore(const UndoSnap *s) {
  for (int i = 0; i < s->comp_count && i < MAX_COMP; i++)
    comps[i] = s->comps[i];
  comp_count = s->comp_count;
  for (int i = 0; i < s->wire_count && i < MAX_WIRES; i++)
    wires[i] = s->wires[i];
  wire_count = s->wire_count;
  next_id = s->next_id;
  for (int i = 0; i < s->sel_count && i < MAX_SEL; i++)
    selection[i] = s->selection[i];
  sel_count = s->sel_count;
  selected_id = s->selected_id;
  dragging_id = -1;
  drag_dx = drag_dy = 0;
  box_selecting = 0;
  group_dragging = 0;
  group_drag_n = 0;
  wiring_active = 0;
  sim_recompute();
}

void undo_push(void) {
  if (sim_running)
    return;
  if (undo_batch > 0) {
    if (undo_batch_pushed)
      return;
    undo_batch_pushed = 1;
  }
  if (undo_top >= UNDO_DEPTH) {
    for (int i = 1; i < UNDO_DEPTH; i++)
      undo_stack[i - 1] = undo_stack[i];
    undo_top = UNDO_DEPTH - 1;
  }
  undo_capture(&undo_stack[undo_top++]);
  redo_top = 0;
}

void undo_begin_batch(void) {
  undo_batch++;
  undo_batch_pushed = 0;
}

void undo_end_batch(void) {
  if (undo_batch > 0)
    undo_batch--;
  if (undo_batch == 0)
    undo_batch_pushed = 0;
}

int undo_can_undo(void) { return undo_top > 0; }
int undo_can_redo(void) { return redo_top > 0; }

int undo_undo(void) {
  if (undo_top <= 0 || sim_running)
    return 0;
  if (redo_top >= UNDO_DEPTH) {
    for (int i = 1; i < UNDO_DEPTH; i++)
      redo_stack[i - 1] = redo_stack[i];
    redo_top = UNDO_DEPTH - 1;
  }
  undo_capture(&redo_stack[redo_top++]);
  undo_restore(&undo_stack[--undo_top]);
  return 1;
}

int undo_redo(void) {
  if (redo_top <= 0 || sim_running)
    return 0;
  if (undo_top >= UNDO_DEPTH) {
    for (int i = 1; i < UNDO_DEPTH; i++)
      undo_stack[i - 1] = undo_stack[i];
    undo_top = UNDO_DEPTH - 1;
  }
  undo_capture(&undo_stack[undo_top++]);
  undo_restore(&redo_stack[--redo_top]);
  return 1;
}

void undo_reset(void) {
  undo_top = 0;
  redo_top = 0;
}

/* ===================== Zoom & Pan Canvas ===================== */
double g_zoom = 1.0;
int g_pan_x = 0;
int g_pan_y = 0;
int g_show_minimap = 1;
int g_minimap_mode = 0;

#define ZOOM_MIN 0.25
#define ZOOM_MAX 8.0

static double clamp_zoom(double z) {
  if (z < ZOOM_MIN)
    return ZOOM_MIN;
  if (z > ZOOM_MAX)
    return ZOOM_MAX;
  return z;
}

void canvas_screen_to_canvas(int sx, int sy, int *cx, int *cy) {
  if (cx)
    *cx = (int)(sx / g_zoom + g_pan_x);
  if (cy)
    *cy = (int)(sy / g_zoom + g_pan_y);
}

void canvas_canvas_to_screen(int cx, int cy, int *sx, int *sy) {
  if (sx)
    *sx = (int)((cx - g_pan_x) * g_zoom);
  if (sy)
    *sy = (int)((cy - g_pan_y) * g_zoom);
}

void canvas_zoom_at(int sx, int sy, double factor) {
  if (factor <= 0.0)
    return;
  int cx, cy;
  canvas_screen_to_canvas(sx, sy, &cx, &cy);
  double nz = clamp_zoom(g_zoom * factor);
  if (nz == g_zoom)
    return;
  g_zoom = nz;
  g_pan_x = cx - (int)(sx / g_zoom);
  g_pan_y = cy - (int)(sy / g_zoom);
}


void canvas_zoom_fit(int view_w, int view_h) {
  if (view_w < 1)
    view_w = 1;
  if (view_h < 1)
    view_h = 1;
  if (comp_count == 0) {
    canvas_zoom_reset();
    return;
  }

  int minx = 0, miny = 0, maxx = 0, maxy = 0;
  for (int i = 0; i < comp_count; i++) {
    int hw, hh;
    comp_bounding_half(&comps[i], &hw, &hh);
    int x0 = comps[i].x - hw, y0 = comps[i].y - hh;
    int x1 = comps[i].x + hw, y1 = comps[i].y + hh;
    if (i == 0) {
      minx = x0;
      miny = y0;
      maxx = x1;
      maxy = y1;
    } else {
      if (x0 < minx)
        minx = x0;
      if (y0 < miny)
        miny = y0;
      if (x1 > maxx)
        maxx = x1;
      if (y1 > maxy)
        maxy = y1;
    }
  }
  int bw = maxx - minx;
  if (bw < 1)
    bw = 1;
  int bh = maxy - miny;
  if (bh < 1)
    bh = 1;

  int margin = 40;
  double zx = (double)(view_w - 2 * margin) / bw;
  double zy = (double)(view_h - 2 * margin) / bh;
  double z = (zx < zy) ? zx : zy;
  g_zoom = clamp_zoom(z);

  int ccx = (minx + maxx) / 2;
  int ccy = (miny + maxy) / 2;
  g_pan_x = ccx - (int)(view_w / 2.0 / g_zoom);
  g_pan_y = ccy - (int)(view_h / 2.0 / g_zoom);
}

void canvas_zoom_reset(void) {
  g_zoom = 1.0;
  g_pan_x = 0;
  g_pan_y = 0;
}

/* Tombol toolbar. Fixed-left (0..3), scrollable (4..BTN_COUNT-1). */
const ToolKind btnTools[BTN_COUNT] = {
    /* Fixed-left group (indices 0..3): */
    TOOL_SELECT, TOOL_BOX, TOOL_WIRE, TOOL_DELETE,
    /* Scrollable components (indices 4..BTN_COUNT-1): */
    TOOL_RESISTOR, TOOL_SWITCH, TOOL_BATTERY, TOOL_ACSOURCE, TOOL_LED,
    TOOL_GROUND, TOOL_CAPACITOR, TOOL_TRANSISTOR, TOOL_DIODE, TOOL_CAPOLAR,
    TOOL_LDR, TOOL_BUZZER, TOOL_VOLTMETER, TOOL_AMMETER, TOOL_OSCPROBE,
    TOOL_INDUCTOR, TOOL_ZENER, TOOL_MOSFET, TOOL_POT, TOOL_AND, TOOL_OR,
    TOOL_NOT, TOOL_NAND, TOOL_NOR, TOOL_XOR};
const wchar_t *btnLabels[BTN_COUNT] = {L"Select / Geser (V)",
                                       L"Blok / Multi-Select (B)",
                                       L"Kabel / Wire (W)",
                                       L"Hapus / Delete (Del)",
                                       L"Resistor (1)",
                                       L"Saklar / Switch (2)",
                                       L"Baterai (3)",
                                       L"Sumber AC (0)",
                                       L"LED (4)",
                                       L"Ground (5)",
                                       L"Kapasitor (6)",
                                       L"Transistor NPN (7)",
                                       L"Dioda (8)",
                                       L"Kapasitor Polar (9)",
                                       L"LDR / Sensor Cahaya (L)",
                                       L"Buzzer (U)",
                                       L"Voltmeter (M)",
                                       L"Ammeter (I)",
                                       L"Probe Osiloskop (P)",
                                       L"Induktor (J)",
                                       L"Dioda Zener (Z)",
                                       L"MOSFET N-Channel (F)",
                                       L"Potensiometer (T)",
                                       L"AND (A)",
                                       L"OR (O)",
                                       L"NOT (N)",
                                       L"NAND",
                                       L"NOR",
                                       L"XOR"};
/* ===================== Status message (cross-cutting notification) =====
Dipindah dari fileio.c lama. Alasan:
 * sim.c & core/model.c memanggil set_status tanpa ingin ikut menyeret
   <windows.h> (yang dibawa io/ui header).
 * Status bukan urusan persistensi/file IO.
Implementasi murni CRT — tidak memanggil Win32 API.
======================================================================== */
wchar_t g_status[256] = L"Siap. Klik ikon di toolbar, lalu klik canvas.";

void set_status(const wchar_t *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _vsnwprintf(g_status, 256, fmt, ap);
  g_status[255] = L'\0';
  va_end(ap);
}

/* ===================== Forward declarations ===================== */
static void cleanup_orphan_junctions(void);

/* ===================== Utilitas ===================== */
int snap(int v) { return (int)(round((double)v / GRID) * GRID); }

Component *find_comp_by_id(int id) {
  if (id < 0)
    return NULL;
  for (int i = 0; i < comp_count; i++)
    if (comps[i].id == id)
      return &comps[i];
  return NULL;
}

Component *find_comp_at(int x, int y) {
  for (int i = comp_count - 1; i >= 0; i--) {
    int hw, hh;
    comp_bounding_half(&comps[i], &hw, &hh);
    int dx = abs(comps[i].x - x);
    int dy = abs(comps[i].y - y);
    if (dx < hw && dy < hh)
      return &comps[i];
  }
  return NULL;
}

void comp_bounding_half(const Component *c, int *hw, int *hh) {
  int w = 48, h = 38;
  if (c->type == COMP_TRANSISTOR || c->type == COMP_MOSFET) {
    w = 52;
    h = 42;
  }
  if (c->rot % 2 != 0) {
    int t = w;
    w = h;
    h = t;
  }
  *hw = w;
  *hh = h;
}

int comp_pin_count(const Component *c) {
  if (!c)
    return 0;
  if (c->type == COMP_GROUND)
    return 1;
  if (c->type == COMP_TRANSISTOR || c->type == COMP_MOSFET ||
      c->type == COMP_POT)
    return 3;
  if (c->type == COMP_AND || c->type == COMP_OR || c->type == COMP_NAND ||
      c->type == COMP_NOR || c->type == COMP_XOR)
    return 3;
  if (c->type == COMP_NONE)
    return 0;
  return 2; /* termasuk NOT */
}

int comp_pin_valid(const Component *c, int pin_idx) {
  if (!c || pin_idx < 0)
    return 0;
  return pin_idx < comp_pin_count(c);
}

Junction *find_junction_by_id(int id) {
  if (id < 0 || id >= MAX_JUNCTIONS)
    return NULL;
  if (!junctions[id].used)
    return NULL;
  return &junctions[id];
}

void pinref_pos(PinRef p, int *x, int *y) {
  if (!x || !y)
    return;
  if (p.comp_id < 0) {
    Junction *j = find_junction_by_id(p.pin_idx);
    if (j) {
      *x = j->x;
      *y = j->y;
      return;
    }
    *x = 0;
    *y = 0;
    return;
  }
  Component *c = find_comp_by_id(p.comp_id);
  if (c)
    comp_pin_pos(c, p.pin_idx, x, y);
  else {
    *x = 0;
    *y = 0;
  }
}

void comp_local_to_world(const Component *c, int lx, int ly, int *wx, int *wy) {
  if (!c || !wx || !wy)
    return;
  int r = ((c->rot % 4) + 4) % 4;
  int rx = lx, ry = ly;
  if (r == 1) {
    rx = -ly;
    ry = lx;
  } else if (r == 2) {
    rx = -lx;
    ry = -ly;
  } else if (r == 3) {
    rx = ly;
    ry = -lx;
  }
  *wx = c->x + rx;
  *wy = c->y + ry;
}

void comp_pin_pos(const Component *c, int pin_idx, int *px, int *py) {
  if (!c || !px || !py)
    return;
  if (c->type == COMP_GROUND) {
    comp_local_to_world(c, 0, -30, px, py);
  } else if (c->type == COMP_TRANSISTOR || c->type == COMP_MOSFET) {
    /* pin0 = C/D (kanan-atas), pin1 = E/S (kanan-bawah), pin2 = B/G (kiri) */
    if (pin_idx == 2)
      comp_local_to_world(c, -40, 0, px, py);
    else if (pin_idx == 1)
      comp_local_to_world(c, 40, 30, px, py);
    else
      comp_local_to_world(c, 40, -30, px, py);
  } else if (c->type == COMP_POT) {
    if (pin_idx == 2)
      comp_local_to_world(c, 0, -35, px, py); /* W */
    else if (pin_idx == 0)
      comp_local_to_world(c, -40, 0, px, py); /* A */
    else
      comp_local_to_world(c, 40, 0, px, py); /* B */
  } else if (c->type == COMP_AND || c->type == COMP_OR ||
             c->type == COMP_NAND || c->type == COMP_NOR ||
             c->type == COMP_XOR) {
    if (pin_idx == 0)
      comp_local_to_world(c, -40, -10, px, py); /* A */
    else if (pin_idx == 1)
      comp_local_to_world(c, -40, +10, px, py); /* B */
    else
      comp_local_to_world(c, +40, 0, px, py); /* OUT */
  } else if (c->type == COMP_NOT) {
    comp_local_to_world(c, (pin_idx == 0) ? -40 : 40, 0, px, py);
  } else {
    comp_local_to_world(c, (pin_idx == 0) ? -40 : 40, 0, px, py);
  }
}

int find_pin_at(int x, int y, PinRef *out, int *px, int *py) {
  int best_d2 = (PIN_R + 9) * (PIN_R + 9);
  int found = 0;
  PinRef best = {-1, -1};
  int bx = 0, by = 0;
  for (int i = 0; i < comp_count; i++) {
    int n = comp_pin_count(&comps[i]);
    for (int p = 0; p < n; p++) {
      int qx, qy;
      comp_pin_pos(&comps[i], p, &qx, &qy);
      int dx = qx - x, dy = qy - y;
      int d2 = dx * dx + dy * dy;
      if (d2 <= best_d2) {
        best_d2 = d2;
        best.comp_id = comps[i].id;
        best.pin_idx = p;
        bx = qx;
        by = qy;
        found = 1;
      }
    }
  }
  /* Junction juga merupakan target wiring yang valid. */
  for (int j = 0; j < MAX_JUNCTIONS; j++) {
    if (!junctions[j].used)
      continue;
    int dx = junctions[j].x - x;
    int dy = junctions[j].y - y;
    int d2 = dx * dx + dy * dy;
    if (d2 <= best_d2) {
      best_d2 = d2;
      best.comp_id = -1; /* tanda junction */
      best.pin_idx = j;
      bx = junctions[j].x;
      by = junctions[j].y;
      found = 1;
    }
  }
  if (found) {
    if (out)
      *out = best;
    if (px)
      *px = bx;
    if (py)
      *py = by;
  }
  return found;
}

static int same_pin(PinRef a, PinRef b) {
  return a.comp_id == b.comp_id && a.pin_idx == b.pin_idx;
}

/* ===================== Multi-Select / Blok Rangkaian ===================== */

void clear_selection(void) {
  sel_count = 0;
  selected_id = -1;
}

int add_to_selection(int id) {
  if (id < 0 || !find_comp_by_id(id))
    return 0;
  for (int i = 0; i < sel_count; i++)
    if (selection[i] == id)
      return 0;
  if (sel_count >= MAX_SEL)
    return 0;
  selection[sel_count++] = id;
  selected_id = id;
  return 1;
}

void toggle_selection(int id) {
  for (int i = 0; i < sel_count; i++) {
    if (selection[i] == id) {
      for (int j = i; j < sel_count - 1; j++)
        selection[j] = selection[j + 1];
      sel_count--;
      selected_id = (sel_count > 0) ? selection[sel_count - 1] : -1;
      return;
    }
  }
  add_to_selection(id);
}

int model_select_rect(int x1, int y1, int x2, int y2, int add) {
  int lx = (x1 < x2) ? x1 : x2, rx = (x1 < x2) ? x2 : x1;
  int ty = (y1 < y2) ? y1 : y2, by = (y1 < y2) ? y2 : y1;
  if (!add) {
    sel_count = 0;
    selected_id = -1;
  }
  int n = 0;
  for (int i = 0; i < comp_count; i++) {
    int hw, hh;
    comp_bounding_half(&comps[i], &hw, &hh);
    if (comps[i].x - hw >= lx && comps[i].x + hw <= rx &&
        comps[i].y - hh >= ty && comps[i].y + hh <= by) {
      if (add) {
        if (add_to_selection(comps[i].id))
          n++;
      } else if (sel_count < MAX_SEL) {
        selection[sel_count++] = comps[i].id;
        selected_id = comps[i].id;
        n++;
      }
    }
  }
  return n;
}

void model_translate_selection(int dx, int dy) {
  if (dx == 0 && dy == 0)
    return;
  undo_push();
  for (int i = 0; i < sel_count; i++) {
    Component *c = find_comp_by_id(selection[i]);
    if (c) {
      c->x += dx;
      c->y += dy;
    }
  }
}

int model_delete_selection(void) {
    if (sim_running) return 0;
    if (sel_count == 0) return 0;

    undo_begin_batch();
    undo_push();   /* snapshot awal */

    /* Hitung removed dari delta comp_count — bukan dari mutasi selection[],
       karena model_remove_at() sudah menghapus item dari selection[] secara
       internal. Loop lama salah menghitung (selalu 0). */
    int count_before = comp_count;

    while (sel_count > 0) {
        Component *c = find_comp_by_id(selection[0]);
        if (!c) {
            /* Item stale — buang dari selection[] tanpa hitung removed
               (memang sudah tidak ada di comps[]). */
            for (int j = 0; j < sel_count - 1; j++)
                selection[j] = selection[j + 1];
            sel_count--;
            continue;
        }
        int cx = c->x, cy = c->y;
        model_remove_at(cx, cy);
    }

    int removed = count_before - comp_count;

    sel_count = 0;
    selected_id = -1;
    sim_recompute();
    undo_end_batch();
    return removed;
}

int model_duplicate_selection(int dx, int dy) {
  if (sim_running)
    return 0;
  if (sel_count == 0)
    return 0;
  if (comp_count + sel_count > MAX_COMP)
    return 0;

  undo_begin_batch();
  undo_push();

  int old_ids[MAX_SEL], new_ids[MAX_SEL];
  int n = sel_count;
  for (int i = 0; i < n; i++)
    old_ids[i] = selection[i];

  for (int i = 0; i < n; i++) {
    Component *c = find_comp_by_id(old_ids[i]);
    if (!c) {
      new_ids[i] = -1;
      continue;
    }
    CompType t = c->type;
    double v = c->v;
    int rot = c->rot, on = c->on;
    WaveKind wave = c->wave;
    int led_color = c->led_color;
    model_add_component(t, c->x + dx, c->y + dy);
    Component *nc = find_comp_by_id(selected_id);
    if (nc) {
      nc->v = v;
      nc->rot = rot;
      nc->on = on;
      nc->wave = wave;
      nc->led_color = led_color;
    }
    new_ids[i] = selected_id;
  }

  int nw = wire_count;
  for (int i = 0; i < wire_count && i < nw; i++) {
    int ai = -1, bi = -1;
    for (int k = 0; k < n; k++) {
      if (wires[i].a.comp_id == old_ids[k])
        ai = k;
      if (wires[i].b.comp_id == old_ids[k])
        bi = k;
    }
    if (ai >= 0 && bi >= 0 && new_ids[ai] > 0 && new_ids[bi] > 0) {
      PinRef a = {new_ids[ai], wires[i].a.pin_idx};
      PinRef b = {new_ids[bi], wires[i].b.pin_idx};
      model_add_wire(a, b);
    }
  }

  sel_count = 0;
  for (int i = 0; i < n; i++)
    if (new_ids[i] > 0)
      add_to_selection(new_ids[i]);
  sim_recompute();
  undo_end_batch();
  return n;
}

/* ===================== Operasi Model ===================== */

double comp_default_value(CompType t) {
  switch (t) {
  case COMP_RESISTOR:
    return 220.0;
  case COMP_BATTERY:
    return 9.0;
  case COMP_ACSOURCE:
    return 220.0;
  case COMP_LED:
    return VF_LED_DEFAULT;
  case COMP_DIODE:
    return VF_DIODE;
  case COMP_CAPACITOR:
    return 100e-9;
  case COMP_CAPOLAR:
    return 100e-6;
  case COMP_LDR:
    return LDR_DARK_DEFAULT;
  case COMP_BUZZER:
    return BUZZER_FREQ_DEFAULT;
  case COMP_AMMETER:
    return 0.0;
  case COMP_OSCPROBE:
    return 0.0;
  case COMP_INDUCTOR:
    return IND_DEFAULT_H;
  case COMP_ZENER:
    return VZENER_DEFAULT;
  case COMP_MOSFET:
    return MOSFET_VTH_DEFAULT;
  case COMP_POT:
    return POT_R_DEFAULT;
  default:
    return 0.0;
  }
}

int comp_has_value(CompType t) {
  return t == COMP_RESISTOR || t == COMP_BATTERY || t == COMP_ACSOURCE ||
         t == COMP_LED || t == COMP_DIODE || t == COMP_CAPACITOR ||
         t == COMP_CAPOLAR || t == COMP_LDR || t == COMP_BUZZER ||
         t == COMP_INDUCTOR || t == COMP_ZENER || t == COMP_MOSFET ||
         t == COMP_POT;
}

const wchar_t *comp_value_unit(CompType t) {
  switch (t) {
  case COMP_RESISTOR:
    return L"\x03A9";
  case COMP_BATTERY:
    return L"V";
  case COMP_ACSOURCE:
    return L"VAC";
  case COMP_LED:
    return L"Vf";
  case COMP_DIODE:
    return L"Vf";
  case COMP_CAPACITOR:
    return L"F";
  case COMP_CAPOLAR:
    return L"F";
  case COMP_LDR:
    return L"Rgelap";
  case COMP_BUZZER:
    return L"Hz";
  case COMP_INDUCTOR:
    return L"H";
  case COMP_ZENER:
    return L"Vz";
  case COMP_MOSFET:
    return L"Vth";
  case COMP_POT:
    return L"\x03A9";
  default:
    return NULL;
  }
}

static int parse_value_core(const wchar_t *s, double *out) {
  if (!s || !out)
    return 0;
  while (*s == L' ' || *s == L'\t')
    s++;
  if (!*s)
    return 0;
  wchar_t *end = NULL;
  double v = wcstod(s, &end);
  if (end == s)
    return 0;
  if (!(v >= 0.0))
    return 0;
  while (*end == L' ' || *end == L'\t')
    end++;
  wchar_t mult = L'\0';
  if (*end && wcschr(L"\x03A9"
                     L"vVfFuUnN",
                     *end)) {
    const wchar_t *p2 = end;
    if ((p2[0] == L'O' || p2[0] == L'o') && (p2[1] == L'h' || p2[1] == L'H') &&
        (p2[2] == L'm' || p2[2] == L'M'))
      end += 3;
    else if (*p2 == L'\x03A9')
      end += 1;
  }
  while (*end == L' ')
    end++;
  if (*end) {
    wchar_t c = *end;
    if (c == L'k' || c == L'K')
      mult = L'k';
    else if (c == L'M')
      mult = L'M';
    else if (c == L'm')
      mult = L'm';
    else if (c == L'u' || c == L'\xB5' || c == L'\u03BC')
      mult = L'u';
    else if (c == L'n' || c == L'N')
      mult = L'n';
    else if (c == L'p' || c == L'P')
      mult = L'p';
    else if (c == L'e' || c == L'E')
      return 0;
    else
      return 0;
    end++;
    while (*end && (*end == L'F' || *end == L'f' || *end == L'V' ||
                    *end == L'v' || *end == L'\x03A9' || *end == L' '))
      end++;
    while (*end) {
      if (*end != L' ')
        return 0;
      end++;
    }
  } else {
    end = (wchar_t *)end;
  }
  switch (mult) {
  case L'k':
    v *= 1e3;
    break;
  case L'M':
    v *= 1e6;
    break;
  case L'u':
    v *= 1e-6;
    break;
  case L'n':
    v *= 1e-9;
    break;
  case L'p':
    v *= 1e-12;
    break;
  default:
    break;
  }
  *out = v;
  return 1;
}

int parse_value_text(const wchar_t *s, double *out) {
  return parse_value_core(s, out);
}

int parse_value_text_a(const char *s, double *out) {
  wchar_t w[64];
  if (!s || !out)
    return 0;
  int i = 0;
  for (; i < 63 && s[i]; i++)
    w[i] = (wchar_t)(unsigned char)s[i];
  w[i] = 0;
  return parse_value_core(w, out);
}

void format_value(double v, const wchar_t *unit, wchar_t *out, int cap) {
  const wchar_t *pre = L"";
  double a = fabs(v);
  if (a >= 1e6) {
    v /= 1e6;
    pre = L"M";
  } else if (a >= 1e3) {
    v /= 1e3;
    pre = L"k";
  } else if (a >= 1.0) {
  } else if (a >= 1e-3) {
    v /= 1e-3;
    pre = L"m";
  } else if (a >= 1e-6) {
    v /= 1e-6;
    pre = L"u";
  } else if (a >= 1e-9) {
    v /= 1e-9;
    pre = L"n";
  } else if (a > 0.0) {
    v /= 1e-12;
    pre = L"p";
  }
  wchar_t num[32];
  if (v == floor(v) && fabs(v) < 1e15)
    swprintf(num, 32, L"%.0f", v);
  else
    swprintf(num, 32, L"%g", v);
  swprintf(out, cap, L"%ls %ls%ls", num, pre, unit ? unit : L"");
}

void model_add_component(CompType type, int x, int y) {
  if (type == COMP_NONE)
    return;
  if (comp_count >= MAX_COMP)
    return;
  if (sim_running)
    return;
  undo_push();
  Component c;
  c.type = type;
  c.x = snap(x);
  c.y = snap(y);
  c.id = next_id++;
  c.rot = 0;
  c.on = (type == COMP_POT) ? POT_WIPER_DEFAULT : 0;
  c.v = comp_default_value(type);
  c.wave = WAVE_SINE;
  c.led_color = 0;
  comps[comp_count++] = c;
  selected_id = c.id;
  sim_recompute();
}

/* Hapus komponen/wire/junction di titik (x,y).
   Undo: SETIAP path memanggil undo_push() sendiri. Bila dipanggil dari
   dalam batch (mis. model_delete_selection), undo_push() otomatis skip. */
void model_remove_at(int x, int y) {
  if (sim_running)
    return;

  /* 1) Junction (prioritas tertinggi — titik kecil, harus presisi). */
  for (int j = 0; j < MAX_JUNCTIONS; j++) {
    if (!junctions[j].used)
      continue;
    int dx = junctions[j].x - x, dy = junctions[j].y - y;
    if (dx * dx + dy * dy <= 100) {
      model_remove_junction(j);
      return;
    }
  }

  /* 2) Wire. */
  if (model_remove_wire_at(x, y)) {
    cleanup_orphan_junctions();
    return;
  }

  /* 3) Komponen. */
  Component *hit = find_comp_at(x, y);
  if (!hit)
    return;
  undo_push();

  int dead_id = hit->id;
  int idx = (int)(hit - comps);
  for (int i = idx; i < comp_count - 1; i++)
    comps[i] = comps[i + 1];
  comp_count--;
  for (int i = wire_count - 1; i >= 0; i--) {
    if (wires[i].a.comp_id == dead_id || wires[i].b.comp_id == dead_id) {
      for (int j = i; j < wire_count - 1; j++)
        wires[j] = wires[j + 1];
      wire_count--;
    }
  }
  if (selected_id == dead_id)
    selected_id = -1;
  if (dragging_id == dead_id)
    dragging_id = -1;
  if (wiring_active && wiring_from.comp_id == dead_id)
    wiring_active = 0;
  for (int i = 0; i < sel_count;) {
    if (selection[i] == dead_id) {
      for (int j = i; j < sel_count - 1; j++)
        selection[j] = selection[j + 1];
      sel_count--;
    } else
      i++;
  }
  if (sel_count > 0 && selected_id < 0)
    selected_id = selection[sel_count - 1];
  cleanup_orphan_junctions();
  sim_recompute();
}

void model_clear_all(void) {
  if (comp_count == 0 && wire_count == 0 && junction_count == 0)
    return;
  undo_push();
  comp_count = 0;
  wire_count = 0;
  selected_id = -1;
  dragging_id = -1;
  wiring_active = 0;
  sel_count = 0;
  group_dragging = 0;
  group_drag_n = 0;
  box_selecting = 0;
  for (int i = 0; i < MAX_COMP; i++) {
    comps[i].on = 0;
    comps[i].v = 0.0;
  }
  for (int i = 0; i < MAX_JUNCTIONS; i++)
    junctions[i].used = 0;
  junction_count = 0;
  sim_recompute();
}

void model_rotate_selected(void) {
  Component *c = find_comp_by_id(selected_id);
  if (!c)
    return;
  if (sim_running)
    return;
  undo_push();
  c->rot = (c->rot + 1) % 4;
  sim_recompute();
}

void model_set_component_value(int comp_id, double v) {
  Component *c = find_comp_by_id(comp_id);
  if (!c || sim_running)
    return;
  if (c->v == v)
    return;
  undo_push();
  c->v = v;
  sim_recompute();
}

/* ===================== Junction ===================== */

int model_add_junction(int x, int y) {
  for (int i = 0; i < MAX_JUNCTIONS; i++) {
    if (junctions[i].used && junctions[i].x == x && junctions[i].y == y)
      return i;
  }
  for (int i = 0; i < MAX_JUNCTIONS; i++) {
    if (!junctions[i].used) {
      junctions[i].x = x;
      junctions[i].y = y;
      junctions[i].used = 1;
      junction_count++;
      return i;
    }
  }
  return -1;
}

void model_remove_junction(int id) {
  if (id < 0 || id >= MAX_JUNCTIONS || !junctions[id].used)
    return;
  if (sim_running)
    return;
  undo_push();

  for (int i = wire_count - 1; i >= 0; i--) {
    if ((wires[i].a.comp_id == -1 && wires[i].a.pin_idx == id) ||
        (wires[i].b.comp_id == -1 && wires[i].b.pin_idx == id)) {
      for (int j = i; j < wire_count - 1; j++)
        wires[j] = wires[j + 1];
      wire_count--;
    }
  }
  junctions[id].used = 0;
  if (junction_count > 0)
    junction_count--;
  selected_id = -1;
  sim_recompute();
}

static void cleanup_orphan_junctions(void) {
  for (int j = 0; j < MAX_JUNCTIONS; j++) {
    if (!junctions[j].used)
      continue;
    int refs = 0;
    for (int i = 0; i < wire_count; i++) {
      if (wires[i].a.comp_id == -1 && wires[i].a.pin_idx == j)
        refs++;
      if (wires[i].b.comp_id == -1 && wires[i].b.pin_idx == j)
        refs++;
    }
    if (refs == 0) {
      junctions[j].used = 0;
      if (junction_count > 0)
        junction_count--;
    }
  }
}

/* ===================== Wire ===================== */

int model_add_wire(PinRef a, PinRef b) {
  if (same_pin(a, b))
    return 0;

  /* Validasi endpoint: pin komponen (comp_id > 0) atau junction (comp_id < 0).
   */
  if (a.comp_id < 0) {
    if (!find_junction_by_id(a.pin_idx))
      return 0;
  } else {
    Component *ca = find_comp_by_id(a.comp_id);
    if (!ca || !comp_pin_valid(ca, a.pin_idx))
      return 0;
  }
  if (b.comp_id < 0) {
    if (!find_junction_by_id(b.pin_idx))
      return 0;
  } else {
    Component *cb = find_comp_by_id(b.comp_id);
    if (!cb || !comp_pin_valid(cb, b.pin_idx))
      return 0;
  }

  for (int i = 0; i < wire_count; i++) {
    if ((same_pin(wires[i].a, a) && same_pin(wires[i].b, b)) ||
        (same_pin(wires[i].a, b) && same_pin(wires[i].b, a)))
      return 0;
  }
  if (wire_count >= MAX_WIRES)
    return 0;

  undo_push();
  wires[wire_count].a = a;
  wires[wire_count].b = b;
  wire_count++;
  sim_recompute();
  return 1;
}

static double dist_pt_seg(int px, int py, int x1, int y1, int x2, int y2) {
  double dx = (double)(x2 - x1), dy = (double)(y2 - y1);
  double L2 = dx * dx + dy * dy;
  double t = 0.0;
  if (L2 > 0.0) {
    t = ((px - x1) * dx + (py - y1) * dy) / L2;
    if (t < 0.0)
      t = 0.0;
    if (t > 1.0)
      t = 1.0;
  }
  double cx = x1 + t * dx, cy = y1 + t * dy;
  double ex = px - cx, ey = py - cy;
  return sqrt(ex * ex + ey * ey);
}

static int wire_hit(int x, int y, int x1, int y1, int x2, int y2) {
  if (dist_pt_seg(x, y, x1, y1, x2, y1) <= WIRE_HIT)
    return 1;
  if (dist_pt_seg(x, y, x2, y1, x2, y2) <= WIRE_HIT)
    return 1;
  return 0;
}

/* Cari wire yang terdekat dengan (x,y) pada interior-nya.
   Return 1 + isi out_wi (index wire), out_jx/out_jy (titik terdekat). */
int find_wire_segment_at(int x, int y, int *out_wi, int *out_jx, int *out_jy) {
  const int tol = WIRE_HIT;
  int best_d2 = tol * tol;
  int found = 0, bwi = -1, bjx = 0, bjy = 0;
  for (int i = 0; i < wire_count; i++) {
    int ax, ay, bx, by;
    pinref_pos(wires[i].a, &ax, &ay);
    pinref_pos(wires[i].b, &bx, &by);

    /* Segmen 1 (horizontal) & 2 (vertikal), persis seperti stroke_ortho. */
    int segs[2][4] = {{ax, ay, bx, ay}, {bx, ay, bx, by}};
    for (int s = 0; s < 2; s++) {
      int x1 = segs[s][0], y1 = segs[s][1];
      int x2 = segs[s][2], y2 = segs[s][3];
      double dx = x2 - x1, dy = y2 - y1;
      double L2 = dx * dx + dy * dy;
      double t = 0;
      if (L2 > 0) {
        t = ((x - x1) * dx + (y - y1) * dy) / L2;
        if (t < 0)
          t = 0;
        if (t > 1)
          t = 1;
      }
      double qx = x1 + t * dx, qy = y1 + t * dy;
      int ix = (int)lround(qx), iy = (int)lround(qy);
      /* Skip kalau titik = endpoint wire (itu bukan interior). */
      if ((ix == ax && iy == ay) || (ix == bx && iy == by))
        continue;
      int ddx = x - ix, ddy = y - iy;
      int d2 = ddx * ddx + ddy * ddy;
      if (d2 < best_d2) {
        best_d2 = d2;
        bwi = i;
        bjx = ix;
        bjy = iy;
        found = 1;
      }
    }
  }
  if (found) {
    if (out_wi)
      *out_wi = bwi;
    if (out_jx)
      *out_jx = bjx;
    if (out_jy)
      *out_jy = bjy;
  }
  return found;
}

/* Hapus wire terdekat dengan (x,y). Wire yang endpoint-nya hilang
   (komponen/junction sudah tidak ada) dihapus sebagai "orphan".
   Junction endpoint (comp_id < 0) DIPERLAKUKAN SEBAGAI VALID — cek
   via find_junction_by_id, bukan find_comp_by_id. */
int model_remove_wire_at(int x, int y) {
  int removed_orphan = 0;
  for (int i = wire_count - 1; i >= 0; i--) {
    /* Validasi endpoint: pin komponen atau junction. */
    int valid_a = (wires[i].a.comp_id < 0)
                      ? (find_junction_by_id(wires[i].a.pin_idx) != NULL)
                      : (find_comp_by_id(wires[i].a.comp_id) != NULL);
    int valid_b = (wires[i].b.comp_id < 0)
                      ? (find_junction_by_id(wires[i].b.pin_idx) != NULL)
                      : (find_comp_by_id(wires[i].b.comp_id) != NULL);

    if (!valid_a || !valid_b) {
      /* Orphan wire: bersihkan tanpa hit-test. */
      if (!removed_orphan) {
        undo_push();
        removed_orphan = 1;
      }
      for (int j = i; j < wire_count - 1; j++)
        wires[j] = wires[j + 1];
      wire_count--;
      continue;
    }

    int ax, ay, bx, by;
    pinref_pos(wires[i].a, &ax, &ay);
    pinref_pos(wires[i].b, &bx, &by);

    if (wire_hit(x, y, ax, ay, bx, by)) {
      undo_push();
      for (int j = i; j < wire_count - 1; j++)
        wires[j] = wires[j + 1];
      wire_count--;
      sim_recompute();
      return 1;
    }
  }
  if (removed_orphan)
    sim_recompute();
  return removed_orphan ? 1 : 0;
}



/* ============================================================================
   Test-only hooks

   Dikompilasi HANYA jika SCHEMATIC_TEST_STUBS didefinisikan. Di build
   produksi, simbol test_reset_model() tidak ada sama sekali.
   ============================================================================
 */
#ifdef SCHEMATIC_TEST_STUBS
void test_reset_model(void) {
    /* ---- Data model (yang paling penting — reset counter & array) ---- */
    fprintf(stderr, "[DBG] test_reset_model: comp_count_before=%d\n", comp_count);
    comp_count     = 0;
    wire_count     = 0;
    junction_count = 0;
    next_id        = 1;

    for (int i = 0; i < MAX_COMP; i++) {
        comps[i].type      = COMP_NONE;
        comps[i].x         = 0;
        comps[i].y         = 0;
        comps[i].id        = 0;
        comps[i].rot       = 0;
        comps[i].on        = 0;
        comps[i].v         = 0.0;
        comps[i].wave      = WAVE_SINE;
        comps[i].led_color = 0;
    }
    for (int i = 0; i < MAX_JUNCTIONS; i++) {
        junctions[i].x    = 0;
        junctions[i].y    = 0;
        junctions[i].used = 0;
    }
    for (int i = 0; i < MAX_WIRES; i++) {
        wires[i].a.comp_id = -1;  wires[i].a.pin_idx = -1;
        wires[i].b.comp_id = -1;  wires[i].b.pin_idx = -1;
    }

    /* ---- Interaksi ---- */
    current_tool   = TOOL_SELECT;
    selected_id    = -1;
    dragging_id    = -1;
    drag_dx        = 0;
    drag_dy        = 0;
    sel_count      = 0;
    for (int i = 0; i < MAX_SEL; i++) selection[i] = 0;
    box_selecting  = 0;
    box_x1 = box_y1 = box_x2 = box_y2 = 0;
    group_dragging = 0;
    group_drag_n   = 0;
    group_drag_ox  = 0;
    group_drag_oy  = 0;
    wiring_active  = 0;
    wiring_from.comp_id = -1;
    wiring_from.pin_idx = -1;
    wiring_mx      = 0;
    wiring_my      = 0;

    /* ---- Undo/redo (static file-scope di model.c — aman diakses) ---- */
    undo_top          = 0;
    redo_top          = 0;
    undo_batch        = 0;
    undo_batch_pushed = 0;

    /* ---- Viewport ---- */
    g_zoom         = 1.0;
    g_pan_x        = 0;
    g_pan_y        = 0;
    g_show_grid    = 1;
    g_show_status  = 1;
    g_show_minimap = 1;
    g_minimap_mode = 0;
}
#endif /* SCHEMATIC_TEST_STUBS */