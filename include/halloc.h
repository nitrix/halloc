#ifndef HALLOC_H
#define HALLOC_H

#include <stdlib.h>

void *halloc(void *ptr, size_t size);
void *hrealloc(void *ptr, size_t size);
void hfree(void *ptr);

void *halloc_get_parent(void *ptr);
void halloc_set_parent(void *ptr, void *parent);
void halloc_steal(void *ptr, void *parent);

#endif
