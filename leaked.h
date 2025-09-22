/*
  Copyright (C) 2023 Ali Almalki <github@makestatic>
  <https://github.com/makestatic/leaked.c>
  Released under the MIT License.

  Simple memory leak detector for C90+ useful for debugging in environments
  where ASan nor Valgrind like software supported. By Wraping malloc-family
  functions and add a step during requsting memory that way we will be able to
  track new and old allocations in a linked list and at end of program will
  check if any of it is still in the list i.e not freed.

        - This is not thread-safe.
        - This may add performance overhead to your program,
          so use it in debug mode only.
        - C90+ only
*/

#ifndef LEAKED_H
#define LEAKED_H

#include <stdio.h>
#include <stdlib.h>

#define COLOR_YELLOW "\x1b[33m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_RED "\x1b[31m"
#define COLOR_RESET "\x1b[0m"

/* Save originals */
#define __real_malloc malloc
#define __real_calloc calloc
#define __real_realloc realloc
#define __real_free free

/* Represent an allocation */
typedef struct LeakNode
{
    void* ptr;
    size_t size;
    const char* file;
    int line;
    struct LeakNode* next;
} LeakNode;

static LeakNode* _leak_head = NULL;

/* Report leaks if any found */
static void leaked_report(void)
{
    LeakNode* curr = _leak_head;
    int leaks = 0;
    size_t total = 0;

    while (curr) {
        fprintf(stderr,
                COLOR_RED "[LEAK]" COLOR_RESET
                          " %lu bytes at %p (allocated at %s:%d)\n",
                (unsigned long)curr->size,
                curr->ptr,
                curr->file,
                curr->line);
        total += curr->size;
        leaks++;
        curr = curr->next;
    }

    if (leaks) {
        fprintf(stderr,
                COLOR_YELLOW "[SUMMARY]" COLOR_RESET
                             " %d leaks, %lu bytes total\n",
                leaks,
                (unsigned long)total);
    } else {
        fprintf(stderr,
                COLOR_GREEN "[SUMMARY]" COLOR_RESET " no leaks detected\n");
    }
}

/* Add new allocation to tracking list and return it */
static void* _leaked_add(void* ptr, size_t size, const char* file, int line)
{
    if (!ptr)
        return NULL;

    LeakNode* node = (LeakNode*)__real_malloc(sizeof(LeakNode));
    if (!node) {
        fprintf(stderr, "[LEAKED] Warning: failed to allocate tracking node\n");
        return ptr; /* cannot be tracked altho still valid allocation */
    }

    node->ptr = ptr;
    node->size = size;
    node->file = file;
    node->line = line;
    node->next = _leak_head;
    _leak_head = node;

    return ptr;
}

/* Remove allocation from tracking list and free it */
static void _leaked_free(void* ptr)
{
    if (!ptr)
        return;

    LeakNode** curr = &_leak_head;
    while (*curr) {
        if ((*curr)->ptr == ptr) {
            LeakNode* tmp = *curr;
            *curr = (*curr)->next;
            __real_free(tmp);
            return;
        }
        curr = &(*curr)->next;
    }
    /* freeing untracked pointer → ignore */
}

static void* _leaked_realloc(void* old, size_t size, const char* file, int line)
{
    void* new_ptr = __real_realloc(old, size);

    if (new_ptr) {
        if (new_ptr != old) {
            _leaked_free(old);
        }
        return _leaked_add(new_ptr, size, file, line);
    } else {
        /* realloc failed → keep old allocation tracked */
        return NULL;
    }
}

/* Redefine malloc-family functions to use leaked.h functions internally */
#define malloc(s) _leaked_add(__real_malloc(s), (s), __FILE__, __LINE__)

#define calloc(n, s)                                                           \
    _leaked_add(__real_calloc((n), (s)), (n) * (s), __FILE__, __LINE__)

#define realloc(p, s) _leaked_realloc((p), (s), __FILE__, __LINE__)

#define free(p)                                                                \
    do {                                                                       \
        _leaked_free(p);                                                       \
        __real_free(p);                                                        \
    } while (0)

/* Initialize leak detection (report automatically at program exit) */
static void leaked_init(void)
{
    atexit(leaked_report);
}

#endif /* LEAKED_H */
