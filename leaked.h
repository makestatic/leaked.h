/*
 *                   LEAKED.H
 *
    Header-only memory leak detector for C .  It trackes
    allocations/frees, warns on double frees, and shows
    memory alive and leaks at program exit. also it supports
    multi-file projects with a single global tracker.


Copyright (c) 2023, 2024, 2025 Ali Almalki
Distributed under the MIT License

*/

#ifndef LEAKED_H
#define LEAKED_H 1

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ANSI colors for terminal output */
#define RED "\033[31m"
#define GRN "\033[32m"
#define YEL "\033[33m"
#define RESET "\033[0m"

/* One node per allocation */
typedef struct Blk
{
    void* ptr;        /* pointer returned by malloc/calloc/realloc */
    size_t sz;        /* bytes allocated */
    const char* file; /* file where allocated */
    int line;         /* line number of allocation */
    struct Blk* next;
} Blk;

/* Manager for tracking all allocations */
typedef struct Mgr
{
    Blk* head;          /* linked list of active allocations */
    long allocs;        /* total allocations */
    long frees;         /* total frees */
    size_t bytes_alloc; /* total bytes allocated */
    size_t bytes_free;  /* total bytes freed */
    long alive;         /* current live blocks */
} Mgr;

/* Singleton manager instance */
#ifdef LEAKED_IMPLEMENTATION
Mgr mgr = { NULL, 0, 0, 0, 0, 0 };
#else
extern Mgr mgr;
#endif

/* add a new allocation to tracking */
static inline void add_blk(void* p, size_t sz, const char* f, int l)
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
    b->next = mgr.head;
    mgr.head = b;

    mgr.allocs++;
    mgr.alive++;
    mgr.bytes_alloc += sz;
}

/* remove allocation from tracking */
static inline bool del_blk(void* p, const char* f, int l)
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

    /* pointer not found => double free or invalid free */
    fprintf(stderr, YEL "[WARN]" RESET " invalid free %p at %s:%d\n", p, f, l);
    return false;
}

/* print report at exit */
static inline void show_leaks(void)
{
    int leaks = 0;
    for (Blk* b = mgr.head; b; b = b->next) {
        fprintf(stderr,
                RED "[LEAK]" RESET " %p (%zu bytes) at %s:%d not freed\n",
                b->ptr,
                b->sz,
                b->file,
                b->line);
        leaks++;
    }

    if (leaks == 0)
        fprintf(stderr, GRN "[LEAKED] no leaks detected\n");
    else
        fprintf(stderr,
                YEL "[LEAKED] %d leak(s), allocs=%ld frees=%ld alive=%ld ;; "
                    "%zu bytes alive\n" RESET,
                leaks,
                mgr.allocs,
                mgr.frees,
                mgr.alive,
                mgr.bytes_alloc - mgr.bytes_free);
}

/* register atexit handler once */
static inline void leaked_init(void)
{
    static bool done = false;
    if (!done) {
        done = true;
        atexit(show_leaks);
    }
}

/* malloc wrapper */
static inline void* xmalloc(size_t n, const char* f, int l)
{
    void* p = malloc(n);
    add_blk(p, n, f, l);
    return p;
}

/* calloc wrapper */
static inline void* xcalloc(size_t n, size_t s, const char* f, int l)
{
    void* p = calloc(n, s);
    add_blk(p, n * s, f, l);
    return p;
}

/* realloc wrapper */
static inline void* xrealloc(void* old, size_t n, const char* f, int l)
{
    void* p = realloc(old, n);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
#endif
    if (old && old != p)
        del_blk(old, f, l);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

    if (p)
        add_blk(p, n, f, l);
    return p;
}

/* free wrapper */
static inline void xfree(void* p, const char* f, int l)
{
    if (!p)
        return;
    if (del_blk(p, f, l))
        free(p);
}

/* --- Replace standard API --- */
#undef malloc
#undef calloc
#undef realloc
#undef free
#define malloc(n) xmalloc(n, __FILE__, __LINE__)
#define calloc(n, s) xcalloc(n, s, __FILE__, __LINE__)
#define realloc(p, n) xrealloc(p, n, __FILE__, __LINE__)
#define free(p) xfree(p, __FILE__, __LINE__)

#endif /* LEAKED_H */
