/*
 *                     LEAKED.H
 *
 *    Simple memory leak detector for C. It tracks
 *    allocations/frees, warns on double frees/invalid use,
 *    and shows memory alive and leaks at program exit.
 *    Also supports multi-file projects with a single global tracker.
 *
 *    Copyright (c) 2023, Ali Almalki, All rights reserved.
 *                  (BSD-3-Clause)
 *
 *    EXAMPLE USAGE:
 *
 *        // main.c :
 *
 *        // must define this macro in one file (i.e main)
 *        #define LEAKED_IMPLEMENTATION
 *        #include "leaked.h"
 *        
 *        int main(void)
 *        {
 *            leaked_init();         // call once at start
 *        
 *            int* a = malloc(10);   // tracked
 *            int* b = calloc(5, 1); // tracked
 *            a = realloc(a, 20);    // tracked
 *        
 *            free(b); // free 'b'
 *            free(b); // double-free !
 *            // forgot to free 'a' !
 *        
 *            return 0;
 *        }
 *
 */

#ifndef LEAKED_H
#define LEAKED_H 1

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RED "\033[31m"
#define GRN "\033[32m"
#define YEL "\033[33m"
#define RESET "\033[0m"

typedef struct Blk
{
    void* ptr;
    size_t sz;
    const char* file;
    int line;
    const char* type;
    struct Blk* next;
} Blk;

typedef struct Mgr
{
    Blk* head;
    long allocs;
    long frees;
    size_t bytes_alloc;
    size_t bytes_free;
    long alive;
} Mgr;

/* Singleton global instance manager */
#ifdef LEAKED_IMPLEMENTATION
Mgr mgr = { NULL, 0, 0, 0, 0, 0 };
#else
extern Mgr mgr;
#endif

static inline void _add_blk(void* p,
                            size_t sz,
                            const char* f,
                            int l,
                            const char* t)
{
    if (!p)
        return;
    Blk* b = (Blk*)malloc(sizeof(*b));
    if (!b)
        return;
    b->ptr = p;
    b->sz = sz;
    b->file = f;
    b->line = l;
    b->type = t;
    b->next = mgr.head;
    mgr.head = b;
    mgr.allocs++;
    mgr.alive++;
    mgr.bytes_alloc += sz;
}

static inline bool _del_blk(void* p, const char* f, int l)
{
    Blk** pp = &mgr.head;
    while (*pp) {
        if ((*pp)->ptr == p) {
            Blk* tmp = *pp;
            *pp = tmp->next;
            mgr.bytes_free += tmp->sz;
            free(tmp);
            mgr.frees++;
            mgr.alive--;
            return true;
        }
        pp = &(*pp)->next;
    }
    fprintf(stderr, YEL "[WARN]" RESET " invalid free %p at %s:%d\n", p, f, l);
    return false;
}

/* Print leak report to stderr */
static inline void show_leaks(void)
{
    if (!mgr.head) {
        fprintf(stderr,
                GRN "Leaked allocations:\n  none\nTotal leaked: 0 bytes in 0 "
                    "allocation\n" RESET);
        return;
    }
    fprintf(stderr, RED "Leaked allocations:\n" RESET);
    int idx = 1;
    size_t total_bytes = 0;
    for (Blk* b = mgr.head; b; b = b->next) {
        fprintf(stderr,
                YEL "  %d) " RESET "ptr=%p size=%zu allocated at %s:%d (%s)\n",
                idx++,
                b->ptr,
                b->sz,
                b->file,
                b->line,
                b->type);
        total_bytes += b->sz;
    }
    fprintf(stderr,
            RED "Total leaked: " RESET "%zu bytes in %d allocation%s\n",
            total_bytes,
            idx - 1,
            (idx - 1 > 1 ? "s" : ""));
}

/* Initialize leak tracker and register show_leaks() to run at program exit
 * automatically */
static inline void leaked_init(void)
{
    static bool done = false;
    if (!done) {
        done = true;
        atexit(show_leaks);
    }
}

static inline void* _xmalloc(size_t n, const char* f, int l)
{
    void* p = malloc(n);
    _add_blk(p, n, f, l, "malloc");
    return p;
}

static inline void* _xcalloc(size_t n, size_t s, const char* f, int l)
{
    void* p = calloc(n, s);
    _add_blk(p, n * s, f, l, "calloc");
    return p;
}

static inline void* _xrealloc(void* old, size_t n, const char* f, int l)
{
    void* p = realloc(old, n);
    if (!p)
        return NULL;

    if (old) {
        if (old != p) {
            _del_blk(old, f, l);
            _add_blk(p, n, f, l, "realloc");
        } else {
            for (Blk* b = mgr.head; b; b = b->next) {
                if (b->ptr == old) {
                    b->sz = n;
                    b->file = f;
                    b->line = l;
                    b->type = "realloc";
                    break;
                }
            }
        }
    } else {
        _add_blk(p, n, f, l, "realloc");
    }

    return p;
}

static inline void _xfree(void* p, const char* f, int l)
{
    if (!p)
        return;
    if (_del_blk(p, f, l))
        free(p);
}

#undef malloc
#undef calloc
#undef realloc
#undef free
#define malloc(n) _xmalloc(n, __FILE__, __LINE__)
#define calloc(n, s) _xcalloc(n, s, __FILE__, __LINE__)
#define realloc(p, n) _xrealloc(p, n, __FILE__, __LINE__)
#define free(p) _xfree(p, __FILE__, __LINE__)

#endif /* LEAKED_H */
