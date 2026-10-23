/* ============================================================================
   tests/test_selection.c — Test seleksi tunggal + multi (blok).
   ============================================================================ */
#include "test_support.h"
#include "model.h"
#include "sim.h"

/* -------------------------------------------- add_to_selection */
void test_add_to_selection(void) {
    test_reset_model();
    test_reset_sim();
    model_add_component(COMP_RESISTOR, 100, 100);
    int id1 = selected_id;
    model_add_component(COMP_RESISTOR, 300, 100);
    int id2 = selected_id;

    clear_selection();
    TEST_ASSERT_INT_EQ(sel_count, 0);
    TEST_ASSERT_INT_EQ(selected_id, -1);

    TEST_ASSERT_INT_EQ(add_to_selection(id1), 1);
    TEST_ASSERT_INT_EQ(sel_count, 1);
    TEST_ASSERT_INT_EQ(selected_id, id1);

    TEST_ASSERT_INT_EQ(add_to_selection(id2), 1);
    TEST_ASSERT_INT_EQ(sel_count, 2);
    TEST_ASSERT_INT_EQ(selected_id, id2);

    /* Duplikat → return 0, tidak menambah */
    TEST_ASSERT_INT_EQ(add_to_selection(id1), 0);
    TEST_ASSERT_INT_EQ(sel_count, 2);

    /* ID invalid ditolak */
    TEST_ASSERT_INT_EQ(add_to_selection(9999), 0);
    TEST_ASSERT_INT_EQ(add_to_selection(-5), 0);
    TEST_ASSERT_INT_EQ(sel_count, 2);
}

/* ------------------------------------------- toggle_selection */
void test_toggle_selection(void) {
    test_reset_model();
    test_reset_sim();
    model_add_component(COMP_RESISTOR, 100, 100);
    int id1 = selected_id;
    model_add_component(COMP_RESISTOR, 300, 100);
    int id2 = selected_id;

    clear_selection();
    toggle_selection(id1);
    TEST_ASSERT_INT_EQ(sel_count, 1);

    toggle_selection(id2);
    TEST_ASSERT_INT_EQ(sel_count, 2);

    toggle_selection(id1);   /* hapus id1 */
    TEST_ASSERT_INT_EQ(sel_count, 1);
    TEST_ASSERT_INT_EQ(selection[0], id2);

    toggle_selection(id2);
    TEST_ASSERT_INT_EQ(sel_count, 0);
    TEST_ASSERT_INT_EQ(selected_id, -1);
}

/* -------------------------------------------- clear_selection */
void test_clear_selection(void) {
    test_reset_model();
    test_reset_sim();
    model_add_component(COMP_RESISTOR, 100, 100);
    add_to_selection(selected_id);
    add_to_selection(selected_id);

    clear_selection();
    TEST_ASSERT_INT_EQ(sel_count, 0);
    TEST_ASSERT_INT_EQ(selected_id, -1);
}

/* ----------------------------------------- model_select_rect */
void test_model_select_rect(void) {
    test_reset_model();
    test_reset_sim();
    model_add_component(COMP_RESISTOR, 100, 100);
    int id1 = selected_id;
    model_add_component(COMP_RESISTOR, 300, 100);
    int id2 = selected_id;
    model_add_component(COMP_RESISTOR, 100, 300);
    int id3 = selected_id;

    clear_selection();

    /* Rect kecil (50,50)-(150,150) hanya kena id1. */
    int n = model_select_rect(50, 50, 150, 150, 0);
    TEST_ASSERT_INT_EQ(n, 1);
    TEST_ASSERT_INT_EQ(sel_count, 1);
    TEST_ASSERT_INT_EQ(selection[0], id1);

    /* Rect besar (0,0)-(500,500) kena semua 3. */
    n = model_select_rect(0, 0, 500, 500, 0);
    TEST_ASSERT_INT_EQ(n, 3);
    TEST_ASSERT_INT_EQ(sel_count, 3);

    /* Add mode: rect kecil meng-add, tidak mengganti. */
    n = model_select_rect(50, 50, 150, 150, 1);
    TEST_ASSERT_INT_EQ(n, 0);   /* id1 sudah ada di seleksi */
    TEST_ASSERT_INT_EQ(sel_count, 3);
}

/* ------------------------------------- model_delete_selection */
void test_model_delete_selection(void) {
    test_reset_model();
    test_reset_sim();
    model_add_component(COMP_RESISTOR, 100, 100);
    int id1 = selected_id;
    model_add_component(COMP_RESISTOR, 300, 100);
    int id2 = selected_id;
    model_add_component(COMP_RESISTOR, 100, 300);
    int id3 = selected_id;

    TEST_ASSERT_INT_EQ(comp_count, 3);

    clear_selection();
    add_to_selection(id1);
    add_to_selection(id3);

    int removed = model_delete_selection();
    TEST_ASSERT_INT_EQ(removed, 2);
    TEST_ASSERT_INT_EQ(comp_count, 1);
    TEST_ASSERT_INT_EQ(sel_count, 0);

    /* id2 masih ada, id1 & id3 hilang. */
    TEST_ASSERT_NOTNULL(find_comp_by_id(id2));
    TEST_ASSERT_NULL(find_comp_by_id(id1));
    TEST_ASSERT_NULL(find_comp_by_id(id3));
}