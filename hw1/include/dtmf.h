/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef DTMF_H
#define DTMF_H

#define NUM_DTMF_FREQS     8
#define NUM_DTMF_ROW_FREQS 4
#define NUM_DTMF_COL_FREQS 4

/*
 * Table of DTMF frequencies, in Hz.
 * The frequencies are listed in increasing order, so that the first
 * half of the table corresponds to the row frequencies and the second half
 * to the column frequencies.
 */
int dtmf_freqs[NUM_DTMF_FREQS];

/*
 * Table mapping (row freq, col freq) pairs to DTMF symbol names.
 */
uint8_t dtmf_symbol_names[NUM_DTMF_ROW_FREQS][NUM_DTMF_COL_FREQS];

#endif
