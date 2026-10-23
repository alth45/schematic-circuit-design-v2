/* ============================================================================
   src/io/fileio.c — Serialisasi JSON (save/load state skematik).

   Modul ini PURE: tidak include <windows.h>, tidak butuh HWND/HDC.
   Satu-satunya dependensi platform adalah CRT: _wfopen, fwprintf, swprintf,
   strtol, strtod — semuanya tersedia di MinGW / MSVC / glibc.

   Yang TIDAK ada di file ini (sengaja dipisah):
     * g_status & set_status   → dipindah ke core/model.c (Step 11 patch).
     * model_save_dialog,
       model_load_dialog,
       model_export_png_dialog → src/ui/dialogs/file_dialogs.c
       (butuh <commdlg.h> + MessageBoxW).
   ============================================================================
 */
#include <stdio.h>  /* FILE, _wfopen, fwprintf, fread, fseek, ftell, swprintf */
#include <stdlib.h> /* malloc, free, strtol, strtod */
#include <string.h> /* strcmp, strlen, strncmp, memcpy */
#include <wchar.h>  /* swprintf, wcslen */

#include "io.h"
#include "model.h"
#include "sim.h"

/* ===================== Mapping nama tipe (internal, wide)
   ===================== Versi wchar_t dipakai khusus oleh model_save_json()
   untuk format %ls di fwprintf. Versi public (char *) ada di bawah — lihat
   io.h. */
static const wchar_t *comp_type_name_w(CompType t) {
  switch (t) {
  case COMP_RESISTOR:
    return L"resistor";
  case COMP_SWITCH:
    return L"switch";
  case COMP_BATTERY:
    return L"battery";
  case COMP_ACSOURCE:
    return L"acsource";
  case COMP_LED:
    return L"led";
  case COMP_GROUND:
    return L"ground";
  case COMP_CAPACITOR:
    return L"capacitor";
  case COMP_TRANSISTOR:
    return L"transistor";
  case COMP_DIODE:
    return L"diode";
  case COMP_CAPOLAR:
    return L"capolar";
  case COMP_VOLTMETER:
    return L"voltmeter";
  case COMP_AMMETER:
    return L"ammeter";
  case COMP_LDR:
    return L"ldr";
  case COMP_BUZZER:
    return L"buzzer";
  case COMP_OSCPROBE:
    return L"oscprobe";
  case COMP_INDUCTOR:
    return L"inductor";
  case COMP_ZENER:
    return L"zener";
  case COMP_MOSFET:
    return L"mosfet";
  case COMP_POT:
    return L"potentiometer";
  case COMP_AND:
    return L"and";
  case COMP_OR:
    return L"or";
  case COMP_NOT:
    return L"not";
  case COMP_NAND:
    return L"nand";
  case COMP_NOR:
    return L"nor";
  case COMP_XOR:
    return L"xor";
  default:
    return L"none";
  }
}

const char *comp_type_json_name(CompType t) {
  switch (t) {
  case COMP_RESISTOR:
    return "resistor";
  case COMP_SWITCH:
    return "switch";
  case COMP_BATTERY:
    return "battery";
  case COMP_ACSOURCE:
    return "acsource";
  case COMP_LED:
    return "led";
  case COMP_GROUND:
    return "ground";
  case COMP_CAPACITOR:
    return "capacitor";
  case COMP_TRANSISTOR:
    return "transistor";
  case COMP_DIODE:
    return "diode";
  case COMP_CAPOLAR:
    return "capolar";
  case COMP_VOLTMETER:
    return "voltmeter";
  case COMP_AMMETER:
    return "ammeter";
  case COMP_LDR:
    return "ldr";
  case COMP_BUZZER:
    return "buzzer";
  case COMP_OSCPROBE:
    return "oscprobe";
  case COMP_INDUCTOR:
    return "inductor";
  case COMP_ZENER:
    return "zener";
  case COMP_MOSFET:
    return "mosfet";
  case COMP_POT:
    return "potentiometer";
  case COMP_AND:
    return "and";
  case COMP_OR:
    return "or";
  case COMP_NOT:
    return "not";
  case COMP_NAND:
    return "nand";
  case COMP_NOR:
    return "nor";
  case COMP_XOR:
    return "xor";
  default:
    return "none";
  }
}

static int comp_type_from_name(const char *s) {
  if (!s)
    return COMP_NONE;
  if (strcmp(s, "resistor") == 0)
    return COMP_RESISTOR;
  if (strcmp(s, "switch") == 0)
    return COMP_SWITCH;
  if (strcmp(s, "battery") == 0)
    return COMP_BATTERY;
  if (strcmp(s, "acsource") == 0 || strcmp(s, "ACSource") == 0 ||
      strcmp(s, "ac_source") == 0 || strcmp(s, "ac") == 0 ||
      strcmp(s, "AC") == 0 || strcmp(s, "SumberAC") == 0)
    return COMP_ACSOURCE;
  if (strcmp(s, "led") == 0)
    return COMP_LED;
  if (strcmp(s, "ground") == 0)
    return COMP_GROUND;
  if (strcmp(s, "Resistor") == 0 || strcmp(s, "R") == 0)
    return COMP_RESISTOR;
  if (strcmp(s, "Switch") == 0 || strcmp(s, "SW") == 0)
    return COMP_SWITCH;
  if (strcmp(s, "Battery") == 0 || strcmp(s, "BAT") == 0)
    return COMP_BATTERY;
  if (strcmp(s, "LED") == 0)
    return COMP_LED;
  if (strcmp(s, "Ground") == 0 || strcmp(s, "GND") == 0)
    return COMP_GROUND;
  if (strcmp(s, "capacitor") == 0)
    return COMP_CAPACITOR;
  if (strcmp(s, "Capacitor") == 0 || strcmp(s, "C") == 0 ||
      strcmp(s, "CAP") == 0)
    return COMP_CAPACITOR;
  if (strcmp(s, "transistor") == 0)
    return COMP_TRANSISTOR;
  if (strcmp(s, "Transistor") == 0 || strcmp(s, "Q") == 0 ||
      strcmp(s, "NPN") == 0)
    return COMP_TRANSISTOR;
  if (strcmp(s, "diode") == 0 || strcmp(s, "Diode") == 0 || strcmp(s, "D") == 0)
    return COMP_DIODE;
  if (strcmp(s, "capolar") == 0 || strcmp(s, "Capolar") == 0 ||
      strcmp(s, "capacitor_polar") == 0 || strcmp(s, "electrolytic") == 0 ||
      strcmp(s, "CP") == 0)
    return COMP_CAPOLAR;
  if (strcmp(s, "voltmeter") == 0 || strcmp(s, "Voltmeter") == 0 ||
      strcmp(s, "VM") == 0)
    return COMP_VOLTMETER;
  if (strcmp(s, "ammeter") == 0 || strcmp(s, "Ammeter") == 0 ||
      strcmp(s, "AM") == 0)
    return COMP_AMMETER;
  if (strcmp(s, "ldr") == 0 || strcmp(s, "LDR") == 0 ||
      strcmp(s, "photoresistor") == 0)
    return COMP_LDR;
  if (strcmp(s, "buzzer") == 0 || strcmp(s, "Buzzer") == 0 ||
      strcmp(s, "BZR") == 0)
    return COMP_BUZZER;
  if (strcmp(s, "oscprobe") == 0 || strcmp(s, "OSCProbe") == 0 ||
      strcmp(s, "OSC") == 0)
    return COMP_OSCPROBE;
  if (strcmp(s, "inductor") == 0 || strcmp(s, "Inductor") == 0 ||
      strcmp(s, "L") == 0 || strcmp(s, "IND") == 0)
    return COMP_INDUCTOR;
  if (strcmp(s, "zener") == 0 || strcmp(s, "Zener") == 0 ||
      strcmp(s, "Z") == 0 || strcmp(s, "ZD") == 0 ||
      strcmp(s, "zenerdiode") == 0)
    return COMP_ZENER;
  if (strcmp(s, "mosfet") == 0 || strcmp(s, "MOSFET") == 0 ||
      strcmp(s, "nmos") == 0 || strcmp(s, "NMOS") == 0 || strcmp(s, "FET") == 0)
    return COMP_MOSFET;
  if (strcmp(s, "potentiometer") == 0 || strcmp(s, "Potentiometer") == 0 ||
      strcmp(s, "pot") == 0 || strcmp(s, "POT") == 0 ||
      strcmp(s, "trimmer") == 0 || strcmp(s, "rheostat") == 0)
    return COMP_POT;
  if (strcmp(s, "and") == 0 || strcmp(s, "AND") == 0)
    return COMP_AND;
  if (strcmp(s, "or") == 0 || strcmp(s, "OR") == 0)
    return COMP_OR;
  if (strcmp(s, "not") == 0 || strcmp(s, "NOT") == 0)
    return COMP_NOT;
  if (strcmp(s, "nand") == 0 || strcmp(s, "NAND") == 0)
    return COMP_NAND;
  if (strcmp(s, "nor") == 0 || strcmp(s, "NOR") == 0)
    return COMP_NOR;
  if (strcmp(s, "xor") == 0 || strcmp(s, "XOR") == 0)
    return COMP_XOR;
  return COMP_NONE;
}

int comp_type_from_json_name(const char *s) { return comp_type_from_name(s); }

/* ===================== SAVE ===================== */

int model_save_json(const wchar_t *path, wchar_t *err, int err_cap) {
  FILE *f = _wfopen(path, L"w, ccs=UTF-8");
  if (!f) {
    if (err)
      swprintf(err, err_cap, L"Tidak bisa menulis file.");
    return 0;
  }
  fwprintf(f, L"{\n");
  fwprintf(f, L"  \"app\": \"schematic-c\",\n");
  fwprintf(f, L"  \"version\": 1,\n");

  /* ---- components ---- */
  fwprintf(f, L"  \"components\": [\n");
  for (int i = 0; i < comp_count; i++) {
    wchar_t vnum[40];
    swprintf(vnum, 40, L"%.6g", comps[i].v);
    fwprintf(f,
             L"    {\"id\": %d, \"type\": \"%ls\", \"x\": %d, \"y\": %d, "
             L"\"rot\": %d, \"on\": %d, \"wave\": %d, \"led_color\": %d, "
             L"\"value\": %ls}%ls\n",
             comps[i].id, comp_type_name_w(comps[i].type), comps[i].x,
             comps[i].y, comps[i].rot % 4, comps[i].on, (int)comps[i].wave,
             comps[i].led_color, vnum, (i + 1 < comp_count) ? L"," : L"");
  }
  fwprintf(f, L"  ],\n");

  /* ---- junctions ---- */
  fwprintf(f, L"  \"junctions\": [\n");
  int first_j = 1;
  for (int i = 0; i < MAX_JUNCTIONS; i++) {
    if (!junctions[i].used)
      continue;
    if (!first_j)
      fwprintf(f, L",\n");
    fwprintf(f, L"    {\"id\": %d, \"x\": %d, \"y\": %d}", i, junctions[i].x,
             junctions[i].y);
    first_j = 0;
  }
  fwprintf(f, L"\n  ],\n");

  /* ---- wires ---- */
  fwprintf(f, L"  \"wires\": [\n");
  for (int i = 0; i < wire_count; i++) {
    wchar_t aw[64], bw[64];
    if (wires[i].a.comp_id < 0)
      swprintf(aw, 64, L"{\"j\": %d}", wires[i].a.pin_idx);
    else
      swprintf(aw, 64, L"{\"id\": %d, \"pin\": %d}", wires[i].a.comp_id,
               wires[i].a.pin_idx);
    if (wires[i].b.comp_id < 0)
      swprintf(bw, 64, L"{\"j\": %d}", wires[i].b.pin_idx);
    else
      swprintf(bw, 64, L"{\"id\": %d, \"pin\": %d}", wires[i].b.comp_id,
               wires[i].b.pin_idx);
    fwprintf(f, L"    {\"a\": %ls, \"b\": %ls}%ls\n", aw, bw,
             (i + 1 < wire_count) ? L"," : L"");
  }
  fwprintf(f, L"  ]\n");
  fwprintf(f, L"}\n");
  fclose(f);
  return 1;
}

/* ===================== PARSER JSON MINIMAL ===================== */

static void skip_ws(const char **p) {
  while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n')
    (*p)++;
}

static int expect_char(const char **p, char c) {
  skip_ws(p);
  if (**p != c)
    return 0;
  (*p)++;
  return 1;
}

static int parse_string(const char **p, char *out, int cap) {
  skip_ws(p);
  if (**p != '"')
    return 0;
  (*p)++;
  int n = 0;
  while (**p && **p != '"') {
    char c = **p;
    if (c == '\\') {
      (*p)++;
      c = **p;
      if (!c)
        break;
      if (c == 'n')
        c = '\n';
      else if (c == 't')
        c = '\t';
    }
    if (n + 1 < cap)
      out[n++] = c;
    (*p)++;
  }
  out[n < cap ? n : cap - 1] = '\0';
  if (**p != '"')
    return 0;
  (*p)++;
  return 1;
}

static int parse_int(const char **p, int *out) {
  skip_ws(p);
  char *end = NULL;
  long v = strtol(*p, &end, 10);
  if (end == *p)
    return 0;
  *p = end;
  *out = (int)v;
  return 1;
}

static int parse_double(const char **p, double *out) {
  skip_ws(p);
  char *end = NULL;
  double v = strtod(*p, &end);
  if (end == *p)
    return 0;
  *p = end;
  *out = v;
  return 1;
}

static int find_key(const char **p, const char *key) {
  const char *s = *p;
  int in_str = 0, esc = 0;
  size_t klen = strlen(key);
  while (*s) {
    if (in_str) {
      if (esc)
        esc = 0;
      else if (*s == '\\')
        esc = 1;
      else if (*s == '"')
        in_str = 0;
      s++;
      continue;
    }
    if (*s == '"') {
      if (strncmp(s, key, klen) == 0) {
        const char *t = s + klen;
        while (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n')
          t++;
        if (*t == ':') {
          *p = t + 1;
          return 1;
        }
      }
      in_str = 1;
      s++;
      continue;
    }
    s++;
  }
  return 0;
}

/* ===================== LOAD ===================== */

int model_load_json(const wchar_t *path, wchar_t *err, int err_cap) {
  FILE *f = _wfopen(path, L"rb");
  if (!f) {
    if (err)
      swprintf(err, err_cap, L"File tidak ditemukan.");
    return 0;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0 || sz > 4 * 1024 * 1024) {
    fclose(f);
    if (err)
      swprintf(err, err_cap, L"Ukuran file tidak valid.");
    return 0;
  }
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    if (err)
      swprintf(err, err_cap, L"Memori habis.");
    return 0;
  }
  size_t got = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[got] = '\0';
  const char *p = buf;
  if (got >= 3 && (unsigned char)buf[0] == 0xEF &&
      (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
    p += 3;

  Component tmp_comps[MAX_COMP];
  int tmp_n = 0, tmp_next = 1;
  Wire tmp_wires[MAX_WIRES];
  int tmp_wn = 0;
  Junction tmp_junc[MAX_JUNCTIONS];

  /* ---- Reset junction buffer ---- */
  for (int i = 0; i < MAX_JUNCTIONS; i++) {
    tmp_junc[i].used = 0;
    tmp_junc[i].x = 0;
    tmp_junc[i].y = 0;
  }

  const char *q = p;

  /* ===================== 1) COMPONENTS ===================== */
  if (!find_key(&q, "\"components\"") || !expect_char(&q, '[')) {
    free(buf);
    if (err)
      swprintf(err, err_cap, L"JSON rusak: 'components' hilang.");
    return 0;
  }
  skip_ws(&q);
  if (*q == ']') {
    q++;
  } else {
    while (1) {
      int id = tmp_next, x = 0, y = 0, rot = 0, on = 0, wave = 0, led_color = 0;
      double v = -1.0;
      char tname[32] = "";
      if (!expect_char(&q, '{'))
        goto bad;
      while (1) {
        char key[48] = "";
        if (!parse_string(&q, key, sizeof key))
          goto bad;
        if (!expect_char(&q, ':'))
          goto bad;
        if (strcmp(key, "id") == 0) {
          if (!parse_int(&q, &id))
            goto bad;
        } else if (strcmp(key, "type") == 0) {
          if (!parse_string(&q, tname, sizeof tname))
            goto bad;
        } else if (strcmp(key, "x") == 0) {
          if (!parse_int(&q, &x))
            goto bad;
        } else if (strcmp(key, "y") == 0) {
          if (!parse_int(&q, &y))
            goto bad;
        } else if (strcmp(key, "rot") == 0 || strcmp(key, "rotation") == 0) {
          if (!parse_int(&q, &rot))
            goto bad;
        } else if (strcmp(key, "value") == 0 || strcmp(key, "v") == 0) {
          if (*q == '\"') {
            char sv[64] = "";
            if (!parse_string(&q, sv, sizeof sv))
              goto bad;
            double tv = 0.0;
            if (parse_value_text_a(sv, &tv))
              v = tv;
          } else {
            if (!parse_double(&q, &v))
              goto bad;
          }
        } else if (strcmp(key, "wave") == 0) {
          if (!parse_int(&q, &wave))
            goto bad;
        } else if (strcmp(key, "led_color") == 0) {
          if (!parse_int(&q, &led_color))
            goto bad;
        } else if (strcmp(key, "on") == 0 || strcmp(key, "closed") == 0 ||
                   strcmp(key, "state") == 0) {
          if (*q == '"') {
            char sv[16] = "";
            if (!parse_string(&q, sv, sizeof sv))
              goto bad;
            if (strcmp(sv, "on") == 0 || strcmp(sv, "ON") == 0 ||
                strcmp(sv, "1") == 0 || strcmp(sv, "closed") == 0)
              on = 1;
          } else {
            if (!parse_int(&q, &on))
              goto bad;
          }
        } else {
          skip_ws(&q);
          if (*q == '"') {
            char d[8];
            parse_string(&q, d, sizeof d);
          } else {
            int d;
            if (!parse_int(&q, &d)) {
              q++;
            }
          }
        }
        skip_ws(&q);
        if (*q == ',') {
          q++;
          continue;
        }
        if (*q == '}') {
          q++;
          break;
        }
        goto bad;
      }
      if (tmp_n < MAX_COMP) {
        int tp = comp_type_from_name(tname);
        if (tp != COMP_NONE) {
          Component *dst = &tmp_comps[tmp_n];
          dst->type = (CompType)tp;
          dst->x = x;
          dst->y = y;
          dst->id = id;
          dst->rot = ((rot % 4) + 4) % 4;

          /* 'on': 0..100 untuk POT; 0/1 untuk SWITCH & LDR; 0 lainnya. */
          if (tp == COMP_POT) {
            if (on < 0)
              on = 0;
            if (on > 100)
              on = 100;
            dst->on = on;
          } else {
            dst->on = ((tp == COMP_SWITCH || tp == COMP_LDR) && on) ? 1 : 0;
          }

          dst->v = (v > 0.0) ? v : comp_default_value((CompType)tp);

          /* PENTING: wave HARUS di-set SEBELUM tmp_n++ — kalau
             ditaruh setelah, kita menulis ke slot berikutnya
             (out-of-bounds bila tmp_n == MAX_COMP). */
          dst->wave = (tp == COMP_ACSOURCE && wave >= 0 && wave <= 2)
                          ? (WaveKind)wave
                          : WAVE_SINE;
          dst->led_color = (tp == COMP_LED && led_color >= 0 && led_color <= 3)
                               ? led_color
                               : 0;

          tmp_n++;
          if (id >= tmp_next)
            tmp_next = id + 1;
        }
      }
      skip_ws(&q);
      if (*q == ',') {
        q++;
        continue;
      }
      if (*q == ']') {
        q++;
        break;
      }
      goto bad;
    }
  }

  /* ===================== 2) JUNCTIONS (opsional — file lama tanpa
     blok ini tetap valid; junction dianggap kosong) ===================== */
  if (find_key(&q, "\"junctions\"")) {
    if (!expect_char(&q, '[')) {
      free(buf);
      if (err)
        swprintf(err, err_cap, L"JSON rusak: 'junctions' bukan array.");
      return 0;
    }
    skip_ws(&q);
    if (*q == ']') {
      q++;
    } else {
      while (1) {
        int jid = -1, jx = 0, jy = 0;
        if (!expect_char(&q, '{'))
          goto bad;
        while (1) {
          char key[16] = "";
          if (!parse_string(&q, key, sizeof key))
            goto bad;
          if (!expect_char(&q, ':'))
            goto bad;
          if (strcmp(key, "id") == 0) {
            if (!parse_int(&q, &jid))
              goto bad;
          } else if (strcmp(key, "x") == 0) {
            if (!parse_int(&q, &jx))
              goto bad;
          } else if (strcmp(key, "y") == 0) {
            if (!parse_int(&q, &jy))
              goto bad;
          } else {
            int d;
            if (!parse_int(&q, &d))
              goto bad;
          }
          skip_ws(&q);
          if (*q == ',') {
            q++;
            continue;
          }
          if (*q == '}') {
            q++;
            break;
          }
          goto bad;
        }
        if (jid >= 0 && jid < MAX_JUNCTIONS) {
          tmp_junc[jid].x = jx;
          tmp_junc[jid].y = jy;
          tmp_junc[jid].used = 1;
        }
        skip_ws(&q);
        if (*q == ',') {
          q++;
          continue;
        }
        if (*q == ']') {
          q++;
          break;
        }
        goto bad;
      }
    }
  }

  /* ===================== 3) WIRES ===================== */
  if (!find_key(&q, "\"wires\"") || !expect_char(&q, '[')) {
    free(buf);
    if (err)
      swprintf(err, err_cap, L"JSON rusak: 'wires' hilang.");
    return 0;
  }
  skip_ws(&q);
  if (*q == ']') {
    q++;
  } else {
    while (1) {
      int aid = -1, apin = 0, bid = -1, bpin = 0;
      if (!expect_char(&q, '{'))
        goto bad;
      while (1) {
        char key[32] = "";
        if (!parse_string(&q, key, sizeof key))
          goto bad;
        if (!expect_char(&q, ':'))
          goto bad;
        if (strcmp(key, "a") == 0 || strcmp(key, "b") == 0) {
          int is_a = (key[0] == 'a');
          int endpoint_is_junction = 0;
          int id_val = -1, pin_val = 0;
          if (!expect_char(&q, '{'))
            goto bad;
          while (1) {
            char k2[32] = "";
            if (!parse_string(&q, k2, sizeof k2))
              goto bad;
            if (!expect_char(&q, ':'))
              goto bad;
            int v = 0;
            if (!parse_int(&q, &v))
              goto bad;
            if (strcmp(k2, "j") == 0) {
              endpoint_is_junction = 1;
              id_val = v;
            } else if (strcmp(k2, "pin") == 0) {
              pin_val = v;
            } else if (strcmp(k2, "id") == 0) {
              id_val = v;
            }
            skip_ws(&q);
            if (*q == ',') {
              q++;
              continue;
            }
            if (*q == '}') {
              q++;
              break;
            }
            goto bad;
          }
          if (endpoint_is_junction) {
            /* comp_id = -1 menandai junction; pin_idx = id junction. */
            if (is_a) {
              aid = -1;
              apin = id_val;
            } else {
              bid = -1;
              bpin = id_val;
            }
          } else {
            if (is_a) {
              aid = id_val;
              apin = pin_val;
            } else {
              bid = id_val;
              bpin = pin_val;
            }
          }
        } else {
          int d;
          if (*q == '"') {
            char s2[16];
            parse_string(&q, s2, sizeof s2);
          } else if (!parse_int(&q, &d))
            goto bad;
        }
        skip_ws(&q);
        if (*q == ',') {
          q++;
          continue;
        }
        if (*q == '}') {
          q++;
          break;
        }
        goto bad;
      }

      /* Validasi endpoint: pin komponen ATAU junction. */
      int a_valid = 0, b_valid = 0;
      if (aid < 0) {
        if (apin >= 0 && apin < MAX_JUNCTIONS && tmp_junc[apin].used)
          a_valid = 1;
      } else if (aid > 0) {
        for (int j = 0; j < tmp_n; j++)
          if (tmp_comps[j].id == aid) {
            a_valid = 1;
            break;
          }
      }
      if (bid < 0) {
        if (bpin >= 0 && bpin < MAX_JUNCTIONS && tmp_junc[bpin].used)
          b_valid = 1;
      } else if (bid > 0) {
        for (int j = 0; j < tmp_n; j++)
          if (tmp_comps[j].id == bid) {
            b_valid = 1;
            break;
          }
      }

      if (tmp_wn < MAX_WIRES && a_valid && b_valid) {
        tmp_wires[tmp_wn].a.comp_id = aid;
        tmp_wires[tmp_wn].a.pin_idx = apin;
        tmp_wires[tmp_wn].b.comp_id = bid;
        tmp_wires[tmp_wn].b.pin_idx = bpin;
        tmp_wn++;
      }
      skip_ws(&q);
      if (*q == ',') {
        q++;
        continue;
      }
      if (*q == ']') {
        q++;
        break;
      }
      goto bad;
    }
  }
  free(buf);

  /* ===================== 4) COMMIT ===================== */
  {
    /* Filter akhir wire — komponen harus ada, junction harus ada.
       (Validasi sudah dilakukan saat parse, ini hanya sanity net.) */
    Wire keep[MAX_WIRES];
    int kn = 0;
    for (int i = 0; i < tmp_wn; i++) {
      int fa = (tmp_wires[i].a.comp_id < 0);
      int fb = (tmp_wires[i].b.comp_id < 0);
      if (!fa) {
        for (int j = 0; j < tmp_n; j++)
          if (tmp_comps[j].id == tmp_wires[i].a.comp_id) {
            fa = 1;
            break;
          }
      }
      if (!fb) {
        for (int j = 0; j < tmp_n; j++)
          if (tmp_comps[j].id == tmp_wires[i].b.comp_id) {
            fb = 1;
            break;
          }
      }
      if (fa && fb)
        keep[kn++] = tmp_wires[i];
    }

    /* Commit junction state dulu (wires akan mereferensi). */
    for (int i = 0; i < MAX_JUNCTIONS; i++)
      junctions[i] = tmp_junc[i];
    junction_count = 0;
    for (int i = 0; i < MAX_JUNCTIONS; i++)
      if (junctions[i].used)
        junction_count++;

    for (int i = 0; i < tmp_n; i++)
      comps[i] = tmp_comps[i];
    comp_count = tmp_n;
    for (int i = 0; i < kn; i++)
      wires[i] = keep[i];
    wire_count = kn;
    next_id = tmp_next;
    selected_id = -1;
    dragging_id = -1;
    wiring_active = 0;
    undo_reset();
    sim_recompute();
  }
  return 1;

bad:
  free(buf);
  if (err)
    swprintf(err, err_cap, L"JSON rusak / format tak dikenal.");
  return 0;
}