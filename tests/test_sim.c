/* ============================================================================
   tests/test_sim.c — Test core/sim.c (empty canvas, RUN guard, LED loop).

   Topologi LED seri untuk test_sim_led_series:

       BAT.pin0 (+) ---- R.pin0
                         R.pin1 ---- LED.pin0 (anoda)
                                     LED.pin1 (katoda) ---- BAT.pin1 (−)

   R = 470Ω (di-set setelah add, default 220).
   Loop: V=9V, R=470Ω, Vf_LED=2V.
   I = (9 - 2) / 470 = 7/470 ≈ 14.9 mA → LED menyala, di bawah 20 mA batas.
   ============================================================================ */
#include "test_support.h"
#include "model.h"
#include "sim.h"

/* --------------------------------------------- sim_empty_canvas */
void test_sim_empty_canvas(void) {
    test_reset_state();
    sim_recompute();

    TEST_ASSERT_INT_EQ(comp_count, 0);
    TEST_ASSERT_INT_EQ(sim_running, 0);
    TEST_ASSERT_INT_EQ(sim_led_count, 0);
    TEST_ASSERT_INT_EQ(sim_short, 0);
    TEST_ASSERT_INT_EQ(sim_has_battery, 0);
    TEST_ASSERT_INT_EQ(sim_has_ac, 0);
    TEST_ASSERT_INT_EQ(sim_exploded, 0);
    TEST_ASSERT_INT_EQ(sim_overcur, 0);
    TEST_ASSERT_DOUBLE_EQ(sim_iled_max, 0.0, 1e-12);
}

/* ----------------------------------------------- sim_run_guard */
void test_sim_run_guard(void) {
    test_reset_state();

    /* Canvas kosong → sim_set_running(1) harus ditolak. */
    sim_set_running(1);
    TEST_ASSERT_INT_EQ(sim_running, 0);

    /* Ada komponen → sim_set_running(1) boleh. */
    model_add_component(COMP_BATTERY, 100, 100);
    sim_set_running(1);
    TEST_ASSERT_INT_EQ(sim_running, 1);

    sim_set_running(0);
    TEST_ASSERT_INT_EQ(sim_running, 0);
}

/* -------------------------------------------- sim_led_series */
void test_sim_led_series(void) {
    test_reset_state();

    /* 1) Tambah 3 komponen, catat id. */
    model_add_component(COMP_BATTERY, 100, 100);
    int bat_id = selected_id;

    model_add_component(COMP_RESISTOR, 300, 100);
    int r_id = selected_id;

    model_add_component(COMP_LED, 500, 100);
    int led_id = selected_id;

    TEST_ASSERT(bat_id > 0);
    TEST_ASSERT(r_id   > 0);
    TEST_ASSERT(led_id > 0);
    TEST_ASSERT_INT_EQ(comp_count, 3);

    /* 2) Set R = 470Ω (default 220Ω overcurrent untuk V=9V). */
    model_set_component_value(r_id, 470.0);

    /* 3) Sambung kabel membentuk loop. */
    PinRef a, b;
    int ok;

    a.comp_id = bat_id; a.pin_idx = 0;  /* BAT+ */
    b.comp_id = r_id;   b.pin_idx = 0;  /* R.0  */
    ok = model_add_wire(a, b);
    TEST_ASSERT_INT_EQ(ok, 1);

    a.comp_id = r_id;   a.pin_idx = 1;  /* R.1   */
    b.comp_id = led_id; b.pin_idx = 0;  /* LED.A */
    ok = model_add_wire(a, b);
    TEST_ASSERT_INT_EQ(ok, 1);

    a.comp_id = led_id; a.pin_idx = 1;  /* LED.K */
    b.comp_id = bat_id; b.pin_idx = 1;  /* BAT-  */
    ok = model_add_wire(a, b);
    TEST_ASSERT_INT_EQ(ok, 1);

    TEST_ASSERT_INT_EQ(wire_count, 3);

    /* 4) sim_recompute otomatis dipanggil oleh model_add_wire → cek state. */
    Component *led = find_comp_by_id(led_id);
    TEST_ASSERT_NOTNULL(led);

    TEST_ASSERT_INT_EQ(sim_has_battery, 1);
    TEST_ASSERT_INT_EQ(sim_has_ac, 0);
    TEST_ASSERT_INT_EQ(sim_short, 0);
    TEST_ASSERT_INT_EQ(sim_led_count, 1);
    TEST_ASSERT_INT_EQ(sim_exploded, 0);
    TEST_ASSERT_INT_EQ(sim_overcur, 0);

    TEST_ASSERT_INT_EQ(sim_comp_lit(led), 1);
    TEST_ASSERT_INT_EQ(sim_comp_blown(led), 0);

    /* I = (9 - 2) / 470 ≈ 0.01489 A */
    double i = sim_comp_current(led);
    TEST_ASSERT_DOUBLE_EQ(i, 0.01489, 0.001);
}