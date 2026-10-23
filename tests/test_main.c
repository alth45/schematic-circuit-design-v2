/* ============================================================================
   tests/test_main.c — Test runner entry point.
   ============================================================================ */
#include "test_support.h"

/* test_model.c */
void test_snap(void);
void test_comp_default_value(void);
void test_comp_has_value(void);
void test_comp_pin_count(void);
void test_find_comp_by_id(void);
void test_model_add_component(void);
void test_parse_value(void);

/* test_sim.c */
void test_sim_empty_canvas(void);
void test_sim_run_guard(void);
void test_sim_led_series(void);

/* test_selection.c */
void test_add_to_selection(void);
void test_toggle_selection(void);
void test_clear_selection(void);
void test_model_select_rect(void);
void test_model_delete_selection(void);

/* test_undo.c */
void test_undo_push_undo(void);
void test_undo_redo(void);
void test_undo_batch(void);
void test_undo_reset(void);

int main(void) {
    printf("=== Schematic Editor Test Suite ===\n\n");

    printf("-- model --\n");
    TEST_RUN(test_snap);
    TEST_RUN(test_comp_default_value);
    TEST_RUN(test_comp_has_value);
    TEST_RUN(test_comp_pin_count);
    TEST_RUN(test_find_comp_by_id);
    TEST_RUN(test_model_add_component);
    TEST_RUN(test_parse_value);

    printf("\n-- sim --\n");
    TEST_RUN(test_sim_empty_canvas);
    TEST_RUN(test_sim_run_guard);
    TEST_RUN(test_sim_led_series);

    printf("\n-- selection --\n");
    TEST_RUN(test_add_to_selection);
    TEST_RUN(test_toggle_selection);
    TEST_RUN(test_clear_selection);
    TEST_RUN(test_model_select_rect);
    TEST_RUN(test_model_delete_selection);

    printf("\n-- undo/redo --\n");
    TEST_RUN(test_undo_push_undo);
    TEST_RUN(test_undo_redo);
    TEST_RUN(test_undo_batch);
    TEST_RUN(test_undo_reset);

    printf("\n=== Summary: %d passed, %d failed ===\n",
           test_pass_count, test_fail_count);
    return (test_fail_count > 0) ? 1 : 0;
}