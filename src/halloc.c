#include "halloc.h"
#include <string.h>

struct header {
    struct header *parent, *child;
    struct header *prev, *next;
    void (*destructor)(void *, void *);
    void *custom;
};

#define ALIGNED_HEADER_SIZE (sizeof (struct { struct header h; void *data[]; }))
#define USER_TO_HEADER(ptr) ((struct header *)((char *)ptr - ALIGNED_HEADER_SIZE))
#define HEADER_TO_USER(ptr) ((void *)((char *)ptr + ALIGNED_HEADER_SIZE))

size_t active_allocations = 0;

static void halloc_dummy_destructor(void *ptr, void *custom) {
    (void) ptr;
    (void) custom;
}

void *halloc(void *parent, size_t size) {
    struct header *header = malloc(ALIGNED_HEADER_SIZE + size);
    if (!header) {
        return NULL;
    }

    active_allocations++;

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

    memcpy(replacement, old, ALIGNED_HEADER_SIZE);

    if (old->parent) old->parent->child = replacement;
    if (old->child) old->child->parent = replacement;
    if (old->prev) old->prev->next = replacement;
    if (old->next) old->next->prev = replacement;

    return HEADER_TO_USER(replacement);
}

void hfree(void *ptr) {
    if (!ptr) {
        return;
    }

    struct header *header = USER_TO_HEADER(ptr);

    if (header->next) header->next->prev = header->prev;
    if (header->prev) header->prev->next = header->next;

    if (header->parent) {
        if (header->parent->child == header) {
            header->parent->child = header->next;
        }
    }

    if (header->child) {
        header->child->parent = NULL;
        hfree(HEADER_TO_USER(header->child));
    }
    
    header->destructor(HEADER_TO_USER(ptr), header->custom);
    free(header);
    active_allocations--;
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
    // TODO: Implement this.
}

void halloc_steal(void *ptr, void *parent) {
    // TODO: Implement this.
}

size_t halloc_debug_active(void) {
    return active_allocations;
}