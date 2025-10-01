// must define this macro in one file (i.e main)
#define LEAKED_IMPLEMENTATION
#include "leaked.h"

int main(void)
{
    leaked_init(); // call once at start

    int* a = malloc(10);   // tracked
    int* b = calloc(5, 1); // tracked
    a = realloc(a, 20);    // tracked

    free(b); // free 'b'
    free(b); // double-free !
    // forgot to free 'a' !

    return 0;
}
