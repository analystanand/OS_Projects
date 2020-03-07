/* The following code is added/modified by your parmanand sahu(pxs180040) & Kushagra Gupta(kxg180031) */
#ifndef _RANDOM_H
#define _RANDOM_H


#include "types.h"
static unsigned random_seed = 1;
// helper function for random function
#define RANDOM_MAX ((1u << 31u) - 1u)
unsigned lcg_parkmiller(unsigned *state)
{
    const unsigned N = 0x7fffffff;
    const unsigned G = 48271u;

    unsigned div = *state / (N / G);
    unsigned rem = *state % (N / G);

    unsigned a = rem * G;
    unsigned b = div * (N % G);

    return *state = (a > b) ? (a - b) : (a + (N - b));
}

unsigned next_random(){
    return lcg_parkmiller(&random_seed);
}
#endif
/* End of code added/modified */