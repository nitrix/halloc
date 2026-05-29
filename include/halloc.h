#ifndef HALLOC_H
#define HALLOC_H

#include <stdlib.h>

// Lower level operations.
void *halloc(void *parent, size_t size);
void *hrealloc(void *ptr, size_t size);
void hfree(void *ptr);

// Higher level operations.
void halloc_set_destructor(void *ptr, void (*destructor)(void *, void *), void *custom);
void halloc_set_parent(void *ptr, void *parent);

#endif
