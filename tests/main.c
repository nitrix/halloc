#include "halloc.h"
#include "testing.h"

ADD_TEST(single_halloc_and_hfree, {
    void *ptr = halloc(NULL, 100);
    MUST(ptr != NULL, "halloc returned NULL");
    hfree(ptr);
})

static void _write_callback(void *ptr, void *custom) {
    bool *b = custom;
    *b = true;
}

ADD_TEST(single_with_destructor, {
    void *ptr = halloc(NULL, 100);
    bool called = false;
    halloc_set_destructor(ptr, _write_callback, &called);
    MUST(!called, "destructor was called too early");
    hfree(ptr);
    MUST(called, "destructor was not called");
})

ADD_TEST(parent_and_child_bottomup, {
    void *parent = halloc(NULL, 100);
    bool called = false;
    halloc_set_destructor(parent, _write_callback, &called);
    void *child = halloc(parent, 100);
    MUST(halloc_get_parent(child) == parent, "child doesnt report the correct parent");
    MUST(!called, "destructor was called too early");
    hfree(child);
    MUST(!called, "freeing the child shouldn't have triggered the parent's destructor");
    hfree(parent);
    MUST(called, "parent's destructor should've been called");
})

ADD_TEST(parent_and_child_upbottom, {
    void *parent = halloc(NULL, 100);
    bool called = false;
    void *child = halloc(parent, 100);
    halloc_set_destructor(child, _write_callback, &called);
    MUST(halloc_get_parent(child) == parent, "child doesnt report the correct parent");
    MUST(!called, "destructor was called too early");
    hfree(parent);
    MUST(called, "freeing the parent should've also free'ed the child and triggered the child's destructor");
})

ADD_TEST(reparent_to_another, {
    void *parent1 = halloc(NULL, 100);
    void *parent2 = halloc(NULL, 100);
    void *child = halloc(parent1, 100);
    MUST(halloc_get_parent(child) == parent1, "child doesn't report the correct parent");
    halloc_set_parent(child, parent2);
    MUST(halloc_get_parent(child) == parent2, "child doesn't report the correct parent after relocation");
    bool called = false;
    halloc_set_destructor(child, _write_callback, &called);
    hfree(parent1);
    MUST(!called, "freeing the parent #1 shouldn't have triggered the child's destructor");
    hfree(parent2);
    MUST(called, "freeing the parent #2 should've triggered the child's destructor");
})

ADD_TEST(reparent_as_orphan, {
    void *parent = halloc(NULL, 100);
    void *child = halloc(parent, 100);
    MUST(halloc_get_parent(child) == parent, "child doesn't report the correct parent");
    halloc_set_parent(child, NULL);
    MUST(halloc_get_parent(child) == NULL, "child doesn't report the correct parent after reparenting to NULL");
    bool called = false;
    halloc_set_destructor(child, _write_callback, &called);
    hfree(parent);
    MUST(!called, "freeing the parent shouldn't have triggered the child's destructor");
    hfree(child);
    MUST(called, "freeing the child should've triggered the child's destructor");
})

MAIN({
    RUN_TEST(single_halloc_and_hfree);
    RUN_TEST(single_with_destructor);
    RUN_TEST(parent_and_child_bottomup);
    RUN_TEST(parent_and_child_upbottom);
    RUN_TEST(reparent_to_another);
    RUN_TEST(reparent_as_orphan);
})
