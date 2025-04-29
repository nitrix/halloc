#include "halloc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct header {
    struct header *parent, *child;
    struct header *prev, *next;
    void (*destructor)(void *self, void *custom);
    void *custom;
};

#define ALIGNED_HEADER_SIZE (sizeof (struct { struct header h; void *data[]; }))
#define USER_TO_HEADER(ptr) ((struct header *)((char *)ptr - ALIGNED_HEADER_SIZE))
#define HEADER_TO_USER(ptr) ((void *)((char *)ptr + ALIGNED_HEADER_SIZE))
#define UNUSED(x) ((void)(x))

static void halloc_dummy_destructor(void *ptr, void *custom) {
    UNUSED(ptr);
    UNUSED(custom);
}

void *halloc(void *parent, size_t size) {
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

    if (header->parent) {
        header->parent->child = header->next;
        if (header->next) header->next->parent = header->parent;
    } else {
        if (header->prev) header->prev->next = header->next;
        if (header->next) header->next->prev = header->prev;
    }

    if (header->child) {
        struct header *child = header->child;
        while (child) {
            struct header *saved_next = child->next;
            if (saved_next) saved_next->prev = NULL;
            hfree(HEADER_TO_USER(child));
            child = saved_next;
        }
    }

    header->destructor(HEADER_TO_USER(ptr), header->custom);

    free(header);
}

void *halloc_get_parent(void *ptr) {
    struct header *header = USER_TO_HEADER(ptr);
    return header->parent ? HEADER_TO_USER(header->parent) : NULL;
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
