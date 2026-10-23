/* ============================================================================
   tests/test_model.c — Test core/model.c (snap, default values, parser, add).
   ============================================================================ */
#include "test_support.h"
#include "model.h"
#include "sim.h"

/* ------------------------------------------------------------------ snap */
void test_snap(void) {
    test_reset_state();
    TEST_ASSERT_INT_EQ(snap(0),    0);
    TEST_ASSERT_INT_EQ(snap(9),    0);    /* round(0.45)*20 = 0    */
    TEST_ASSERT_INT_EQ(snap(11),   20);   /* round(0.55)*20 = 20   */
    TEST_ASSERT_INT_EQ(snap(20),   20);
    TEST_ASSERT_INT_EQ(snap(30),   40);   /* round(1.5)*20 = 40    */
    TEST_ASSERT_INT_EQ(snap(-9),   0);
    TEST_ASSERT_INT_EQ(snap(-11),  -20);
    TEST_ASSERT_INT_EQ(snap(-30),  -40);
}

/* --------------------------------------------------- comp_default_value */
void test_comp_default_value(void) {
    test_reset_state();
    TEST_ASSERT_DOUBLE_EQ(comp_default_value(COMP_RESISTOR),  220.0, 1e-9);
    TEST_ASSERT_DOUBLE_EQ(comp_default_value(COMP_BATTERY),   9.0,   1e-9);
    TEST_ASSERT_DOUBLE_EQ(comp_default_value(COMP_ACSOURCE),  220.0, 1e-9);
    TEST_ASSERT_DOUBLE_EQ(comp_default_value(COMP_LED),       2.0,   1e-9);
    TEST_ASSERT_DOUBLE_EQ(comp_default_value(COMP_CAPACITOR), 100e-9, 1e-15);
    TEST_ASSERT_DOUBLE_EQ(comp_default_value(COMP_POT),       10000.0, 1e-9);
    TEST_ASSERT_DOUBLE_EQ(comp_default_value(COMP_GROUND),    0.0,   1e-9);
}

/* ------------------------------------------------------ comp_has_value */
void test_comp_has_value(void) {
    test_reset_state();
    /* Punya nilai */
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_RESISTOR),  1);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_BATTERY),   1);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_ACSOURCE),  1);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_LED),       1);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_CAPACITOR), 1);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_POT),       1);
    /* Tidak punya nilai */
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_SWITCH),    0);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_GROUND),    0);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_TRANSISTOR),0);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_VOLTMETER), 0);
    TEST_ASSERT_INT_EQ(comp_has_value(COMP_AND),       0);
}

/* ----------------------------------------------------- comp_pin_count */
void test_comp_pin_count(void) {
    test_reset_state();
    Component c;
    c.type = COMP_GROUND;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 1);

    c.type = COMP_RESISTOR;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 2);

    c.type = COMP_NOT;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 2);

    c.type = COMP_TRANSISTOR;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 3);

    c.type = COMP_MOSFET;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 3);

    c.type = COMP_POT;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 3);

    c.type = COMP_AND;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 3);

    c.type = COMP_XOR;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 3);

    c.type = COMP_NONE;
    TEST_ASSERT_INT_EQ(comp_pin_count(&c), 0);

    TEST_ASSERT_INT_EQ(comp_pin_count(NULL), 0);
}

/* -------------------------------------------------- find_comp_by_id */
void test_find_comp_by_id(void) {
    test_reset_state();
    TEST_ASSERT_NULL(find_comp_by_id(-1));
    TEST_ASSERT_NULL(find_comp_by_id(0));
    TEST_ASSERT_NULL(find_comp_by_id(999));

    model_add_component(COMP_RESISTOR, 100, 100);
    int id1 = selected_id;
    TEST_ASSERT(id1 > 0);

    Component *c = find_comp_by_id(id1);
    TEST_ASSERT_NOTNULL(c);
    TEST_ASSERT_INT_EQ(c->type, COMP_RESISTOR);
    TEST_ASSERT_INT_EQ(c->x, 100);
    TEST_ASSERT_INT_EQ(c->y, 100);
    TEST_ASSERT_DOUBLE_EQ(c->v, 220.0, 1e-9);
}

/* ---------------------------------------------- model_add_component */
void test_model_add_component(void) {
    test_reset_state();
    TEST_ASSERT_INT_EQ(comp_count, 0);

    model_add_component(COMP_RESISTOR, 95, 105);
    TEST_ASSERT_INT_EQ(comp_count, 1);

    /* x,y di-snap ke grid */
    Component *c = find_comp_by_id(selected_id);
    TEST_ASSERT_NOTNULL(c);
    TEST_ASSERT_INT_EQ(c->x, 100);   /* snap(95) = 100  */
    TEST_ASSERT_INT_EQ(c->y, 100);   /* snap(105) = 100 */

    model_add_component(COMP_LED, 300, 300);
    TEST_ASSERT_INT_EQ(comp_count, 2);
    Component *c2 = find_comp_by_id(selected_id);
    TEST_ASSERT_NOTNULL(c2);
    TEST_ASSERT_INT_EQ(c2->type, COMP_LED);

    /* ID harus unik */
    TEST_ASSERT_INT_NE(c->id, c2->id);

    /* COMP_NONE ditolak */
    model_add_component(COMP_NONE, 0, 0);
    TEST_ASSERT_INT_EQ(comp_count, 2);
}

/* ---------------------------------------------- parse_value_text */
void test_parse_value(void) {
    test_reset_state();
    double v;

    TEST_ASSERT_INT_EQ(parse_value_text(L"220", &v), 1);
    TEST_ASSERT_DOUBLE_EQ(v, 220.0, 1e-9);

    TEST_ASSERT_INT_EQ(parse_value_text(L"4.7k", &v), 1);
    TEST_ASSERT_DOUBLE_EQ(v, 4700.0, 1e-9);

    TEST_ASSERT_INT_EQ(parse_value_text(L"1M", &v), 1);
    TEST_ASSERT_DOUBLE_EQ(v, 1e6, 1e-6);

    TEST_ASSERT_INT_EQ(parse_value_text(L"100n", &v), 1);
    TEST_ASSERT_DOUBLE_EQ(v, 100e-9, 1e-15);

    TEST_ASSERT_INT_EQ(parse_value_text(L"10uF", &v), 1);
    TEST_ASSERT_DOUBLE_EQ(v, 10e-6, 1e-12);

    TEST_ASSERT_INT_EQ(parse_value_text(L"5.1Vz", &v), 1);
    TEST_ASSERT_DOUBLE_EQ(v, 5.1, 1e-9);

    TEST_ASSERT_INT_EQ(parse_value_text(L"4.7 k", &v), 1);
    TEST_ASSERT_DOUBLE_EQ(v, 4700.0, 1e-9);

    /* Invalid */
    TEST_ASSERT_INT_EQ(parse_value_text(L"", &v), 0);
    TEST_ASSERT_INT_EQ(parse_value_text(L"abc", &v), 0);
    TEST_ASSERT_INT_EQ(parse_value_text(L"-5", &v), 0);   /* negatif ditolak */
    TEST_ASSERT_INT_EQ(parse_value_text(NULL, &v), 0);
}