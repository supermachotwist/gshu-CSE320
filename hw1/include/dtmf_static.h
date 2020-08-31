/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef DTMF_STATIC_H
#define DTMF_STATIC_H

#include "dtmf.h"

/*
 * Table of DTMF frequencies, in Hz.
 */
int dtmf_freqs[NUM_DTMF_FREQS] = { 697, 770, 852, 941, 1209, 1336, 1477, 1633 };

/*
 * Table mapping (row freq, col freq) pairs to DTMF symbol names.
 */
uint8_t dtmf_symbol_names[NUM_DTMF_ROW_FREQS][NUM_DTMF_COL_FREQS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

#endif
