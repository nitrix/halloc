#include "halloc.h"

struct header {
    struct header *parent, *child;
    struct header *prev, *next;
    void (*destructor)(void *, void *);
    void *custom;
};

#define ALIGNED_HEADER_SIZE (sizeof (struct { struct header h; void *data[]; }))
#define USER_TO_HEADER(ptr) ((struct header *)((char *)ptr - ALIGNED_HEADER_SIZE))
#define HEADER_TO_USER(ptr) ((void *)((char *)ptr + ALIGNED_HEADER_SIZE))

static void halloc_dummy_destructor(void *ptr, void *custom) {
    (void) ptr;
    (void) custom;
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
        }
    }

    return HEADER_TO_USER(header);
}

void *hrealloc(void *ptr, size_t size) {
    if (!ptr) {
        return halloc(NULL, size);
    }

    struct header *header = USER_TO_HEADER(ptr);

    // WIP
}

void hfree(void *ptr) {}

void *halloc_get_parent(void *ptr) {}
void halloc_set_parent(void *ptr, void *parent) {}
void halloc_steal(void *ptr, void *parent) {}