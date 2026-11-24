/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc/malloc.h>

typedef unsigned long long malloc_type_id_t;
typedef uint64_t kido_malloc_zone_malloc_options_t;

#define KIDO_MALLOC_ZONE_MALLOC_OPTION_CLEAR 1ull

void *malloc_type_malloc(size_t size, malloc_type_id_t type_id);
void *malloc_type_calloc(size_t count, size_t size, malloc_type_id_t type_id);
void malloc_type_free(void *ptr, malloc_type_id_t type_id);
void *malloc_type_realloc(void *ptr, size_t size, malloc_type_id_t type_id);
void *malloc_type_valloc(size_t size, malloc_type_id_t type_id);
void *malloc_type_aligned_alloc(size_t alignment, size_t size, malloc_type_id_t type_id);
int malloc_type_posix_memalign(void **memptr, size_t alignment, size_t size, malloc_type_id_t type_id);
void *malloc_type_zone_malloc(malloc_zone_t *zone, size_t size, malloc_type_id_t type_id);
void *malloc_type_zone_calloc(malloc_zone_t *zone, size_t count, size_t size, malloc_type_id_t type_id);
void malloc_type_zone_free(malloc_zone_t *zone, void *ptr, malloc_type_id_t type_id);
void *malloc_type_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size, malloc_type_id_t type_id);
void *malloc_type_zone_valloc(malloc_zone_t *zone, size_t size, malloc_type_id_t type_id);
void *malloc_type_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size, malloc_type_id_t type_id);
void *malloc_zone_malloc_with_options(malloc_zone_t *zone, size_t align, size_t size,
    kido_malloc_zone_malloc_options_t opts);
void *malloc_zone_malloc_with_options_np(malloc_zone_t *zone, size_t align, size_t size,
    kido_malloc_zone_malloc_options_t opts);
void *malloc_type_zone_malloc_with_options(malloc_zone_t *zone, size_t align, size_t size,
    malloc_type_id_t type_id, kido_malloc_zone_malloc_options_t opts);
void *malloc_type_zone_malloc_with_options_internal(malloc_zone_t *zone, size_t align, size_t size,
    malloc_type_id_t type_id, kido_malloc_zone_malloc_options_t opts);

void *
malloc_type_malloc(size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return malloc(size);
}

void *
malloc_type_calloc(size_t count, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return calloc(count, size);
}

void
malloc_type_free(void *ptr, malloc_type_id_t type_id)
{
	(void)type_id;
	free(ptr);
}

void *
malloc_type_realloc(void *ptr, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return realloc(ptr, size);
}

void *
malloc_type_valloc(size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return valloc(size);
}

void *
malloc_type_aligned_alloc(size_t alignment, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return aligned_alloc(alignment, size);
}

int
malloc_type_posix_memalign(void **memptr, size_t alignment, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return posix_memalign(memptr, alignment, size);
}

void *
malloc_type_zone_malloc(malloc_zone_t *zone, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return malloc_zone_malloc(zone, size);
}

void *
malloc_type_zone_calloc(malloc_zone_t *zone, size_t count, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return malloc_zone_calloc(zone, count, size);
}

void
malloc_type_zone_free(malloc_zone_t *zone, void *ptr, malloc_type_id_t type_id)
{
	(void)type_id;
	malloc_zone_free(zone, ptr);
}

void *
malloc_type_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return malloc_zone_realloc(zone, ptr, size);
}

void *
malloc_type_zone_valloc(malloc_zone_t *zone, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return malloc_zone_valloc(zone, size);
}

void *
malloc_type_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size, malloc_type_id_t type_id)
{
	(void)type_id;
	return malloc_zone_memalign(zone, alignment, size);
}

void *
malloc_zone_malloc_with_options(malloc_zone_t *zone, size_t align, size_t size,
    kido_malloc_zone_malloc_options_t opts)
{
	void *ptr;

	if (zone == NULL) {
		zone = malloc_default_zone();
	}
	if (align > sizeof(void *)) {
		ptr = malloc_zone_memalign(zone, align, size);
		if (ptr != NULL && (opts & KIDO_MALLOC_ZONE_MALLOC_OPTION_CLEAR)) {
			memset(ptr, 0, size);
		}
		return ptr;
	}
	if (opts & KIDO_MALLOC_ZONE_MALLOC_OPTION_CLEAR) {
		return malloc_zone_calloc(zone, 1, size);
	}
	return malloc_zone_malloc(zone, size);
}

void *
malloc_zone_malloc_with_options_np(malloc_zone_t *zone, size_t align, size_t size,
    kido_malloc_zone_malloc_options_t opts)
{
	return malloc_zone_malloc_with_options(zone, align, size, opts);
}

void *
malloc_type_zone_malloc_with_options(malloc_zone_t *zone, size_t align, size_t size,
    malloc_type_id_t type_id, kido_malloc_zone_malloc_options_t opts)
{
	(void)type_id;
	return malloc_zone_malloc_with_options(zone, align, size, opts);
}

void *
malloc_type_zone_malloc_with_options_internal(malloc_zone_t *zone, size_t align, size_t size,
    malloc_type_id_t type_id, kido_malloc_zone_malloc_options_t opts)
{
	(void)type_id;
	return malloc_zone_malloc_with_options(zone, align, size, opts);
}
