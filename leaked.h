/*
 *
 *								LEAKED.H
 * thread-safe memory bug catcher for C (C89+).  it tracks allocations and
 * frees, warns on invalid or double frees, and reports leaks at program exit
 * (it handle crash).  also it works across multiple files with a single global
 * tracker and colored output enabled by default.
 *
 * Copyright (c) 2023, Ali Almalki, All rights reserved. BSD-3-Clause.
 *
 * USAGE:
 *     // main.c
 *     #define LEAKED_IMPLEMENTATION
 *     #include "leaked.h"
 *
 *     int main(void)
 *     {
 *         leaked_init();           // call once at program start
 *		   ...
 *   	   return 0;
 *   	}
 *
 *
 *
 * OUTPUT EXAMPLE:
 *     [LEAKED] invalid free at 0x30000332a0 (main.c:42)
 *     [LEAKED] 1 alloc(s) 42 bytes at 0x30000332a0 (main.c:69)
 *     [LEAKED] 1 alloc(s) 20 bytes at 0x30000332a0 (main.c:420)
 *     [LEAKED] total (2) leaks, (62) bytes
 *
 * NOTES:
 *     - ‘#define LEAKED_IMPLEMENTATION’ in one .c file only (e.g main.c)
 *       include normally in other files
 *     - minimal overhead, intended for debugging only
 *     - enable thread-safety with: #define LEAKED_THREAD_SAFE
 *     - disable colors with: #define LEAKED_NO_COLOR
 *
 */

#define _POSIX_C_SOURCE 200809L

#ifndef LEAKED_H
#define LEAKED_H 1

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LEAKED_NO_COLOR
#define YEL "\033[33m"
#define RESET "\033[0m"
#else
#define YEL ""
#define RESET ""
#endif

#ifdef LEAKED_THREAD_SAFE
#include <pthread.h>
#endif

#define LEAKED_INITIAL_CAP 1024
#define LEAKED_LOAD_NUM 3
#define LEAKED_LOAD_DEN 4

typedef struct Blk
{
	void* ptr;
	size_t sz;
	const char* file;
	int line;
	struct Blk* next;
} Blk;

/* Global manager */
typedef struct
{
	Blk** table;
	size_t capacity;
	size_t alive;
#ifdef LEAKED_THREAD_SAFE
	pthread_mutex_t lock;
#endif
} Mgr;

#ifdef LEAKED_IMPLEMENTATION
static Mgr mgr = { NULL,
				   0,
				   0
#ifdef LEAKED_THREAD_SAFE
				   ,
				   PTHREAD_MUTEX_INITIALIZER
#endif
};
#else
extern Mgr mgr;
#endif

#ifdef LEAKED_THREAD_SAFE
#define LOCK() pthread_mutex_lock(&mgr.lock)
#define UNLOCK() pthread_mutex_unlock(&mgr.lock)
#else
#define LOCK() ((void)0)
#define UNLOCK() ((void)0)
#endif

/* a simple pointer hash for table index
 * it do the job
 * */
static unsigned int _hash_ptr(void* p, size_t cap)
{
	if (!cap) return 0;
	uintptr_t v = (uintptr_t)p;
	v = (v >> 4) ^ (v << 5);
	return (unsigned int)(v % (uintptr_t)cap);
}

/* ensure table is allocated */
static void _ensure_table_ext(void)
{
	if (!mgr.table) {
		mgr.capacity = (size_t)LEAKED_INITIAL_CAP;
		mgr.table = (Blk**)calloc(mgr.capacity, sizeof(Blk*));
	}
}

/* rehash table when resizing */
static void _rehash(size_t new_cap)
{
	if (!mgr.table || new_cap == 0) return;
	Blk** new_table = (Blk**)calloc(new_cap, sizeof(Blk*));
	if (!new_table) return;
	for (size_t i = 0; i < mgr.capacity; i++) {
		Blk* cur = mgr.table[i];
		while (cur) {
			Blk* next = cur->next;
			unsigned int idx = _hash_ptr(cur->ptr, new_cap);
			cur->next = new_table[idx];
			new_table[idx] = cur;
			cur = next;
		}
	}
	free(mgr.table);
	mgr.table = new_table;
	mgr.capacity = new_cap;
}

/* resize table if load factor exceeded */
static void _maybe_resize(void)
{
	if (!mgr.table) return;
	if (mgr.alive >
		(mgr.capacity * (size_t)LEAKED_LOAD_NUM) / (size_t)LEAKED_LOAD_DEN)
		_rehash(mgr.capacity * 2);
}

/* add block to the table */
static void _add_blk(void* p, size_t sz, const char* f, int l)
{
	if (!p) return;
	LOCK();
	_ensure_table_ext();
	_maybe_resize();
	unsigned int idx = _hash_ptr(p, mgr.capacity);
	Blk* b = (Blk*)malloc(sizeof(Blk));
	if (b) {
		b->ptr = p;
		b->sz = sz;
		b->file = f;
		b->line = l;
		b->next = mgr.table[idx];
		mgr.table[idx] = b;
		mgr.alive++;
	}
	UNLOCK();
}

/* remove block, (if) report invalid frees */
static int _del_blk(void* p, const char* f, int l)
{
	if (!p) return 0;
	int ok = 0;
	LOCK();
	if (mgr.table) {
		unsigned int idx = _hash_ptr(p, mgr.capacity);
		Blk** pp;
		for (pp = &mgr.table[idx]; *pp; pp = &(*pp)->next) {
			if ((*pp)->ptr == p) {
				Blk* tmp = *pp;
				*pp = tmp->next;
				free(tmp);
				mgr.alive--;
				ok = 1;
				break;
			}
		}
	}
	UNLOCK();
	if (!ok)
		fprintf(stderr,
				YEL "[LEAKED]" RESET " invalid free at %p (%s:%d)\n",
				p,
				f,
				l);
	return ok;
}

static void* _xmalloc(size_t n, const char* f, int l)
{
	void* p = malloc(n);
	if (p) _add_blk(p, n, f, l);
	return p;
}

static void* _xcalloc(size_t nm, size_t s, const char* f, int l)
  __attribute__((unused));
static void* _xcalloc(size_t nm, size_t s, const char* f, int l)
{
	if (nm && s > ((size_t)-1) / nm) return NULL;
	void* p = calloc(nm, s);
	if (p) _add_blk(p, nm * s, f, l);
	return p;
}

static void* _xrealloc(void* old, size_t n, const char* f, int l)
  __attribute__((unused));
static void* _xrealloc(void* old, size_t n, const char* f, int l)
{
	void* p = realloc(old, n);
	if (!p) return NULL;

	if (old && old != p) _del_blk(old, f, l);

	_add_blk(p, n, f, l);
	return p;
}

static void _xfree(void* p, const char* f, int l)
{
	if (p && _del_blk(p, f, l)) free(p);
}

/* report to stderr at this point */
static void show_leaks(void)
{
	LOCK();
	if (!mgr.table || mgr.alive == 0) {
		UNLOCK();
		return;
	}
	Blk** snapshot = mgr.table;
	size_t cap_snapshot = mgr.capacity;
	mgr.table = NULL;
	mgr.capacity = 0;
	mgr.alive = 0;
	UNLOCK();

	long total_count = 0;
	size_t total_bytes = 0;

	for (size_t i = 0; i < cap_snapshot; i++) {
		for (Blk* b = snapshot[i]; b; b = b->next) {
			fprintf(stderr,
					YEL "[LEAKED]" RESET " leak: %lu bytes at %p (%s:%d)\n",
					(unsigned long)b->sz,
					b->ptr,
					b->file,
					b->line);
			total_count++;
			total_bytes += b->sz;
		}
	}

	if (total_count > 0)
		fprintf(stderr,
				YEL "[LEAKED]" RESET " total (%ld) leaks, (%lu) bytes\n",
				total_count,
				(unsigned long)total_bytes);

	/* free snapshot */
	for (size_t i = 0; i < cap_snapshot; i++) {
		Blk* b = snapshot[i];
		while (b) {
			Blk* tmp = b;
			b = b->next;
			free(tmp);
		}
	}
	free(snapshot);
}

/*
 * simple crash handler
 * TODO: better handler (maybe using async signal??)
 */
static void _crash_handler(int sig)
{
	fprintf(stderr, "\n[LEAKED] caught signal %d, dumping leaks...\n", sig);
	show_leaks();
	signal(sig, SIG_DFL);
	raise(sig);
}

/* print to stderr on program exit or crash */
static void leaked_init(void)
{
	static int done = 0;
	if (!done) {
		done = 1;
		atexit(show_leaks);
		signal(SIGSEGV, _crash_handler);
		signal(SIGABRT, _crash_handler);
		signal(SIGILL, _crash_handler);
		signal(SIGFPE, _crash_handler);
	}
}

/* remap stooodss */
#undef malloc
#undef calloc
#undef realloc
#undef free
#define malloc(n) _xmalloc(n, __FILE__, __LINE__)
#define calloc(n, s) _xcalloc(n, s, __FILE__, __LINE__)
#define realloc(p, n) _xrealloc(p, n, __FILE__, __LINE__)
#define free(p) _xfree(p, __FILE__, __LINE__)

#endif /* LEAKED_H */
