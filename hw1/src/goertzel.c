#include <stdint.h>
#include <math.h>

#include "debug.h"
#include "goertzel.h"

void goertzel_init(GOERTZEL_STATE *gp, uint32_t N, double k) {
    (*gp).N = N;
    (*gp).k = k;
    (*gp).A = 2 * M_PI * (k/N);
    (*gp).B = 2 * cos((*gp).A);
    (*gp).s0 = 0;
    (*gp).s1 = 0;
    (*gp).s2 = 0;
}

void goertzel_step(GOERTZEL_STATE *gp, double x) {
    (*gp).s0 = x + ((*gp).B)*((*gp).s1) - (*gp).s2;
    (*gp).s2 = (*gp).s1;
    (*gp).s1 = (*gp).s0;
}

double goertzel_strength(GOERTZEL_STATE *gp, double x) {
    (*gp).s0 = x + ((*gp).B)*((*gp).s1) - (*gp).s2;
    return (double)(2 * (pow((*gp).s0,2) + pow((*gp).s1,2) - 2*(((*gp).s0) * ((*gp).s1) * cos((*gp).A)))) / (pow((*gp).N,2));
}
