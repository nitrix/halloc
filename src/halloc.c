#include "halloc.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct header {
    struct header *parent, *child;
    struct header *prev, *next;
    void (*destructor)(void *self, void *custom);
    void *custom;
};

#define ALIGNED_HEADER_SIZE (sizeof (struct { struct header h; max_align_t a; }))
#define USER_TO_HEADER(ptr) ((struct header *)((char *)ptr - ALIGNED_HEADER_SIZE))
#define HEADER_TO_USER(ptr) ((void *)((char *)ptr + ALIGNED_HEADER_SIZE))
#define UNUSED(x) ((void)(x))

static void halloc_dummy_destructor(void *ptr, void *custom) {
    UNUSED(ptr);
    UNUSED(custom);
}

void *halloc(void *parent, size_t size) {
    if (size > SIZE_MAX - ALIGNED_HEADER_SIZE) {
        return NULL;
    }

    struct header *header = malloc(ALIGNED_HEADER_SIZE + size);
    if (!header) {
        return NULL;
    }

    header->parent = NULL;
    header->child = NULL;
    header->prev = NULL;
    header->next = NULL;
    header->destructor = halloc_dummy_destructor;
    header->custom = NULL;

    if (parent) {
        struct header *real_parent = USER_TO_HEADER(parent);
        struct header *saved_child = real_parent->child;

        // Attach ourself to the existing parent.
        header->parent = real_parent;
        real_parent->child = header;

        // Relocate the saved child as a sibling of ourself.
        header->next = saved_child;
        if (saved_child) {
            saved_child->prev = header;
            saved_child->parent = NULL;
        }
    }

    return HEADER_TO_USER(header);
}

void *hrealloc(void *ptr, size_t size) {
    if (!ptr) {
        return halloc(NULL, size);
    }

    if (size > SIZE_MAX - ALIGNED_HEADER_SIZE) {
        return NULL;
    }

    struct header *old = USER_TO_HEADER(ptr);

    struct header *replacement = realloc(old, ALIGNED_HEADER_SIZE + size);
    if (!replacement) {
        return NULL;
    }

    if (replacement == old) {
        return ptr;
    }

    if (replacement->parent) replacement->parent->child = replacement;
    if (replacement->child) replacement->child->parent = replacement;
    if (replacement->prev) replacement->prev->next = replacement;
    if (replacement->next) replacement->next->prev = replacement;

    return HEADER_TO_USER(replacement);
}

void hfree(void *ptr) {
    if (!ptr) {
        return;
    }

    struct header *header = USER_TO_HEADER(ptr);

    // Detach the subtree root from its parent and siblings so the surrounding
    // tree stays consistent, then isolate it as a standalone root.
    if (header->parent) {
        header->parent->child = header->next;
        if (header->next) header->next->parent = header->parent;
    } else {
        if (header->prev) header->prev->next = header->next;
        if (header->next) header->next->prev = header->prev;
    }
    header->parent = NULL;
    header->prev = NULL;
    header->next = NULL;

    // Tear the subtree down iteratively to keep teardown depth bounded by the
    // heap rather than the call stack. Each node's destructor runs only after
    // all of its descendants have been freed (post-order).
    struct header *cur = header;
    while (cur) {
        while (cur->child) {
            cur = cur->child;
        }

        struct header *next;
        if (cur->next) {
            // Promote the next sibling to head, preserving the invariant that
            // only the head of a sibling list stores the parent pointer.
            next = cur->next;
            next->prev = NULL;
            cur->parent->child = next;
            next->parent = cur->parent;
        } else {
            // Last child of its parent: the parent becomes a leaf next.
            next = cur->parent;
            if (next) next->child = NULL;
        }

        bool is_root = (cur == header);
        cur->destructor(HEADER_TO_USER(cur), cur->custom);
        free(cur);

        if (is_root) {
            break;
        }
        cur = next;
    }
}

void halloc_set_destructor(void *ptr, void (*destructor)(void *, void *), void *custom) {
    struct header *header = USER_TO_HEADER(ptr);
    header->destructor = destructor;
    header->custom = custom;
}

void halloc_set_parent(void *ptr, void *parent) {
    struct header *header = USER_TO_HEADER(ptr);

    // Remove from where it was.
    if (header->prev) header->prev->next = header->next;
    if (header->next) header->next->prev = header->prev;

    if (header->parent) {
        header->parent->child = header->next;
        if (header->next) header->next->parent = header->parent;
    }

    // Okay, we're a proper orphan now.
    header->parent = NULL;
    header->prev = NULL;
    header->next = NULL;

    if (parent) {
        struct header *real_parent = USER_TO_HEADER(parent);
        struct header *saved_child = real_parent->child;

        // Attach ourself to the existing parent.
        header->parent = real_parent;
        real_parent->child = header;

        // Relocate the saved child as a sibling of ourself.
        header->next = saved_child;
        if (saved_child) {
            saved_child->prev = header;
            saved_child->parent = NULL;
        }
    }
}
