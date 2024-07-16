#include "halloc.h"
#include "testing.h"

ADD_TEST(single_halloc_and_hfree, {
    void *ptr = halloc(NULL, 100);
    MUST(ptr != NULL, "halloc returned NULL");
    hfree(ptr);
})

static void write_callback(void *ptr, void *custom) {
    *(bool *)custom = true;
}

ADD_TEST(single_with_destructor, {
    void *ptr = halloc(NULL, 100);
    MUST(halloc_debug_active() == 1, "active allocations should be 1");
    bool called = false;
    halloc_set_destructor(ptr, write_callback, &called);
    MUST(!called, "destructor was called too early");
    hfree(ptr);
    MUST(called, "destructor was not called");
    MUST(halloc_debug_active() == 0, "active allocations should be 0");
})

ADD_TEST(parent_and_child_bottomup, {
    void *parent = halloc(NULL, 100);
    bool called = false;
    halloc_set_destructor(parent, write_callback, &called);
    void *child = halloc(parent, 100);
    MUST(halloc_debug_active() == 2, "active allocations should be 2");
    MUST(halloc_get_parent(child) == parent, "child doesnt report the correct parent");
    MUST(!called, "destructor was called too early");
    hfree(child);
    MUST(halloc_debug_active() == 1, "active allocations should be 1");
    MUST(!called, "freeing the child shouldn't have triggered the parent's destructor");
    hfree(parent);
    MUST(halloc_debug_active() == 0, "active allocations should be 0");
    MUST(called, "parent's destructor should've been called");
})

ADD_TEST(parent_and_child_upbottom, {
    void *parent = halloc(NULL, 100);
    bool called = false;
    halloc_set_destructor(parent, write_callback, &called);
    void *child = halloc(parent, 100);
    MUST(halloc_debug_active() == 2, "active allocations should be 2");
    MUST(halloc_get_parent(child) == parent, "child doesnt report the correct parent");
    MUST(!called, "destructor was called too early");
    hfree(parent);
    MUST(halloc_debug_active() == 0, "active allocations should be 0");
    MUST(called, "freeing the child should've also free'ed the parent and triggered the parent's destructor");
})

MAIN({
    RUN_TEST(single_halloc_and_hfree);
    RUN_TEST(single_with_destructor);
    RUN_TEST(parent_and_child_bottomup);
    RUN_TEST(parent_and_child_upbottom);
})