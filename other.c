#include "other.h"
#include "leaked.h"

/* Example function in another translation unit */
void use_other(int* x)
{
    int* tmp = malloc(5 * sizeof(int)); // tracked
    tmp = &x[1];                        // this is 0
    x[0] = ((int)(*tmp) + 10);          // edit passed pointer
    // free(tmp);                       // unfreed 'tmp' !
}
