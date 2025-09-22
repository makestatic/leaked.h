# leaked.h
a tiny memory leak detector for C that wraps malloc, calloc, realloc, free and keeps track of them in a linked list. when the program exits it prints what was never freed.  

_useful for debugging in environments where ASan nor Valgrind like-software is available._

## notes

- include this header before any code that touches malloc.  
- if realloc fails the old pointer stays tracked.  
- freeing an unknown pointer is ignored.  

- not thread safe  
- adds performance overhead → **don’t use in production**  
- C90+ only  


## usage

```c
#include "leaked.h"

int main(void) {
    leaked_init(); /* call once at top of main */

    void *a = malloc(42);
    void *b = calloc(1, 128);

    free(a);

    return 0; /* report runs at exit automatically */
}
```

```
Copyright (C) 2023 Ali Almalki <github@makestatic>.
Released under the MIT License.
```
