/* ============================================================================
   tests/test_undo.c — Test undo/redo stack + batch behavior.
   ============================================================================ */
#include "test_support.h"
#include "model.h"
#include "sim.h"

/* ------------------------------------------ undo_push + undo */
void test_undo_push_undo(void) {
    test_reset_model();
    test_reset_sim();
    TEST_ASSERT_INT_EQ(comp_count, 0);
    TEST_ASSERT_INT_EQ(undo_can_undo(), 0);
    TEST_ASSERT_INT_EQ(undo_can_redo(), 0);

    /* model_add_component() memanggil undo_push() internal. */
    model_add_component(COMP_RESISTOR, 100, 100);
    TEST_ASSERT_INT_EQ(comp_count, 1);
    TEST_ASSERT_INT_EQ(undo_can_undo(), 1);
    TEST_ASSERT_INT_EQ(undo_can_redo(), 0);

    int r = undo_undo();
    TEST_ASSERT_INT_EQ(r, 1);
    TEST_ASSERT_INT_EQ(comp_count, 0);
    TEST_ASSERT_INT_EQ(undo_can_undo(), 0);
    TEST_ASSERT_INT_EQ(undo_can_redo(), 1);

    /* Undo lagi → no-op */
    TEST_ASSERT_INT_EQ(undo_undo(), 0);
    TEST_ASSERT_INT_EQ(comp_count, 0);
}

/* -------------------------------------------- undo_redo */
void test_undo_redo(void) {
    test_reset_model();
    test_reset_sim();
    model_add_component(COMP_RESISTOR, 100, 100);
    model_add_component(COMP_LED, 300, 100);
    TEST_ASSERT_INT_EQ(comp_count, 2);

    undo_undo();   /* hapus LED → count=1 */
    TEST_ASSERT_INT_EQ(comp_count, 1);
    TEST_ASSERT_INT_EQ(undo_can_redo(), 1);

    int r = undo_redo();
    TEST_ASSERT_INT_EQ(r, 1);
    TEST_ASSERT_INT_EQ(comp_count, 2);
    TEST_ASSERT_INT_EQ(undo_can_redo(), 0);

    /* Redo lagi → no-op */
    TEST_ASSERT_INT_EQ(undo_redo(), 0);
    TEST_ASSERT_INT_EQ(comp_count, 2);
}

/* -------------------------------------------- undo_batch */
void test_undo_batch(void) {
    test_reset_model();
    test_reset_sim();
    TEST_ASSERT_INT_EQ(comp_count, 0);

    undo_begin_batch();
    model_add_component(COMP_RESISTOR, 100, 100);  /* push snapshot (count=0) */
    model_add_component(COMP_RESISTOR, 300, 100);  /* skip push */
    model_add_component(COMP_RESISTOR, 500, 100);  /* skip push */
    undo_end_batch();

    TEST_ASSERT_INT_EQ(comp_count, 3);
    TEST_ASSERT_INT_EQ(undo_can_undo(), 1);

    /* Satu undo membatalkan seluruh batch → kembali ke count=0. */
    undo_undo();
    TEST_ASSERT_INT_EQ(comp_count, 0);
    TEST_ASSERT_INT_EQ(undo_can_undo(), 0);
}

/* -------------------------------------------- undo_reset */
void test_undo_reset(void) {
    test_reset_model();
    test_reset_sim();
    model_add_component(COMP_RESISTOR, 100, 100);
    model_add_component(COMP_RESISTOR, 300, 100);
    TEST_ASSERT_INT_EQ(undo_can_undo(), 1);

    undo_reset();
    TEST_ASSERT_INT_EQ(undo_can_undo(), 0);
    TEST_ASSERT_INT_EQ(undo_can_redo(), 0);

    /* comp_count tidak berubah oleh undo_reset. */
    TEST_ASSERT_INT_EQ(comp_count, 2);
}