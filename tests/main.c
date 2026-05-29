#include "halloc.h"
#include "testing.h"

#include <string.h>

static void _write_callback(void *ptr, void *custom) {
    (void)ptr;
    bool *b = custom;
    *b = true;
}

static void _count_callback(void *ptr, void *custom) {
    (void)ptr;
    int *counter = custom;
    (*counter)++;
}

// Records the relative order in which destructors fire using a shared counter
// passed via a small struct.
struct order_probe {
    int *counter;
    int order;
};

static void _order_callback(void *ptr, void *custom) {
    (void)ptr;
    struct order_probe *probe = custom;
    probe->order = ++(*probe->counter);
}

ADD_TEST(single_halloc_and_hfree, {
    void *ptr = halloc(NULL, 100);
    MUST(ptr != NULL, "halloc returned NULL");
    hfree(ptr);
})

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
    MUST(child != NULL, "halloc child returned NULL");
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
    MUST(!called, "destructor was called too early");
    hfree(parent);
    MUST(called, "freeing the parent should've also free'ed the child and triggered the child's destructor");
})

ADD_TEST(reparent_to_another, {
    void *parent1 = halloc(NULL, 100);
    void *parent2 = halloc(NULL, 100);
    void *child = halloc(parent1, 100);
    halloc_set_parent(child, parent2);
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
    halloc_set_parent(child, NULL);
    bool called = false;
    halloc_set_destructor(child, _write_callback, &called);
    hfree(parent);
    MUST(!called, "freeing the parent shouldn't have triggered the child's destructor");
    hfree(child);
    MUST(called, "freeing the child should've triggered the child's destructor");
})

// The case that the original single-child tests could not reach: several
// children under one parent must all be freed when the parent is freed.
ADD_TEST(multiple_children_all_freed, {
    void *parent = halloc(NULL, 100);
    int freed = 0;
    for (int i = 0; i < 5; i++) {
        void *child = halloc(parent, 100);
        MUST(child != NULL, "halloc child returned NULL");
        halloc_set_destructor(child, _count_callback, &freed);
    }
    MUST(freed == 0, "no child destructor should've run yet");
    hfree(parent);
    MUST(freed == 5, "every child's destructor should've run exactly once");
})

ADD_TEST(grandchildren_all_freed, {
    void *root = halloc(NULL, 100);
    int freed = 0;

    // Build a tree: root -> 3 children, each with 2 children of their own.
    for (int i = 0; i < 3; i++) {
        void *child = halloc(root, 100);
        MUST(child != NULL, "halloc child returned NULL");
        halloc_set_destructor(child, _count_callback, &freed);
        for (int j = 0; j < 2; j++) {
            void *grandchild = halloc(child, 100);
            MUST(grandchild != NULL, "halloc grandchild returned NULL");
            halloc_set_destructor(grandchild, _count_callback, &freed);
        }
    }

    hfree(root);
    MUST(freed == 9, "all 3 children and 6 grandchildren should've been freed");
})

// Free a single non-head sibling individually, then free the parent and make
// sure the remaining siblings are still torn down correctly.
ADD_TEST(free_single_sibling, {
    void *parent = halloc(NULL, 100);

    // Allocation order a, b, c => c is the sibling-list head, a is the tail.
    void *a = halloc(parent, 100);
    void *b = halloc(parent, 100);
    void *c = halloc(parent, 100);

    bool a_freed = false, b_freed = false, c_freed = false;
    halloc_set_destructor(a, _write_callback, &a_freed);
    halloc_set_destructor(b, _write_callback, &b_freed);
    halloc_set_destructor(c, _write_callback, &c_freed);

    // b is a middle sibling (neither head nor tail).
    hfree(b);
    MUST(b_freed, "freeing b should've run b's destructor");
    MUST(!a_freed && !c_freed, "freeing b shouldn't have touched its siblings");

    hfree(parent);
    MUST(a_freed && c_freed, "the surviving siblings should be freed with the parent");
})

ADD_TEST(reparent_sibling_with_others, {
    void *parent1 = halloc(NULL, 100);
    void *parent2 = halloc(NULL, 100);

    // a is the tail sibling under parent1, b is the head.
    void *a = halloc(parent1, 100);
    void *b = halloc(parent1, 100);

    bool a_freed = false, b_freed = false;
    halloc_set_destructor(a, _write_callback, &a_freed);
    halloc_set_destructor(b, _write_callback, &b_freed);

    halloc_set_parent(a, parent2);

    hfree(parent1);
    MUST(b_freed, "b should've been freed with parent1");
    MUST(!a_freed, "a was reparented and shouldn't be freed with parent1");

    hfree(parent2);
    MUST(a_freed, "a should've been freed with parent2");
})

ADD_TEST(realloc_preserves_tree_and_data, {
    void *parent = halloc(NULL, 16);

    void *child = halloc(parent, 16);
    for (int i = 0; i < 16; i++) {
        ((unsigned char *)child)[i] = (unsigned char)i;
    }
    bool child_freed = false;
    halloc_set_destructor(child, _write_callback, &child_freed);

    // Grow aggressively to make a move likely.
    void *grown = hrealloc(child, 1 << 16);
    MUST(grown != NULL, "hrealloc returned NULL");
    child = grown;

    for (int i = 0; i < 16; i++) {
        MUST(((unsigned char *)child)[i] == (unsigned char)i, "hrealloc didn't preserve data");
    }

    // The child must still be wired into the parent after a potential move.
    hfree(parent);
    MUST(child_freed, "child should've been freed with the parent after hrealloc");
})

ADD_TEST(realloc_parent_keeps_child_link, {
    void *parent = halloc(NULL, 16);
    void *child = halloc(parent, 16);
    bool child_freed = false;
    halloc_set_destructor(child, _write_callback, &child_freed);

    // Grow the parent; if it moves, the child's parent back-link must follow.
    void *grown = hrealloc(parent, 1 << 16);
    MUST(grown != NULL, "hrealloc returned NULL");
    parent = grown;

    hfree(parent);
    MUST(child_freed, "child should've been freed after the parent was reallocated");
})

ADD_TEST(null_edge_cases, {
    // hfree(NULL) is a no-op and must not crash.
    hfree(NULL);

    // hrealloc(NULL, n) behaves like halloc(NULL, n).
    void *ptr = hrealloc(NULL, 100);
    MUST(ptr != NULL, "hrealloc(NULL, n) should allocate");
    hfree(ptr);
})

ADD_TEST(destructor_runs_after_children, {
    void *parent = halloc(NULL, 100);
    void *child = halloc(parent, 100);

    int counter = 0;
    struct order_probe parent_probe = { &counter, 0 };
    struct order_probe child_probe = { &counter, 0 };
    halloc_set_destructor(parent, _order_callback, &parent_probe);
    halloc_set_destructor(child, _order_callback, &child_probe);

    hfree(parent);
    MUST(child_probe.order != 0 && parent_probe.order != 0, "both destructors should've run");
    MUST(child_probe.order < parent_probe.order, "child's destructor must run before the parent's");
})

// Exercises the iterative teardown: a chain this deep would overflow the stack
// with a recursive implementation.
ADD_TEST(deep_chain_is_freed, {
    enum { DEPTH = 200000 };
    int freed = 0;

    void *node = halloc(NULL, 8);
    MUST(node != NULL, "halloc root returned NULL");
    void *root = node;
    for (int i = 0; i < DEPTH; i++) {
        node = halloc(node, 8);
        MUST(node != NULL, "halloc deep child returned NULL");
        halloc_set_destructor(node, _count_callback, &freed);
    }

    hfree(root);
    MUST(freed == DEPTH, "every node in the deep chain should've been freed");
})

MAIN({
    RUN_TEST(single_halloc_and_hfree);
    RUN_TEST(single_with_destructor);
    RUN_TEST(parent_and_child_bottomup);
    RUN_TEST(parent_and_child_upbottom);
    RUN_TEST(reparent_to_another);
    RUN_TEST(reparent_as_orphan);
    RUN_TEST(multiple_children_all_freed);
    RUN_TEST(grandchildren_all_freed);
    RUN_TEST(free_single_sibling);
    RUN_TEST(reparent_sibling_with_others);
    RUN_TEST(realloc_preserves_tree_and_data);
    RUN_TEST(realloc_parent_keeps_child_link);
    RUN_TEST(null_edge_cases);
    RUN_TEST(destructor_runs_after_children);
    RUN_TEST(deep_chain_is_freed);
})
