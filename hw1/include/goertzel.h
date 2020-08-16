/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef GOERTZEL_H
#define GOERTZEL_H

#include <stdint.h>

/*
 * Structure representing the state of an instance of the Goertzel algorithm.
 * Basically follows Figure 4 of:
 *
 *   Sysel and Rajmic,
 *   "Goertzel algorithm generalized to non-integer multiples of fundamental frequency"
 *   EURASIP Journal on Advances in Signal Processing 2012, 2012:56
 *   https://link.springer.com/article/10.1186/1687-6180-2012-56
 * 
 * The variables C, and D in the cited paper are not actually state variables,
 * so they have been been omitted from this structure.  An iteration counter
 * variable i has been added.  The iteration counter is not logically necessary
 * for the algorithm, but its presence may be useful for debugging.
 */
typedef struct goertzel_state {
    uint32_t N;      // Number of samples in the signal to be analyzed.
    double k;        // Real-valued "index" of the frequency component
                     // whose strength is to be computed.
    double A;	     // Intermediate value used to compute B and C.
    double B;        // Multiplicative constant applied at each iteration.
    double s0;       // Goertzel filter state variables.
    double s1;
    double s2;
} GOERTZEL_STATE;

/*
 * Initialize the state of an instance of the Goertzel algorithm.
 *
 *   @param gp  Pointer to structure to be initialized.
 *   @param N  Number of samples in the signal to be analyzed.
 *   @param k  Real-valued "index" of the frequency component whose strength
 *   is to be computed.  The idea is that k/N specifies the (possibly fractional)
 *   number of cycles of the frequency component whose strength is to be determined,
 *   that occur within the N samples representing one fundamental period of the signal
 *   being analyzed.  So, k = 0 represents the "DC" or constant component,
 *   k = 1 represents the fundamental frequency, k = 2 represents two times the
 *   fundamental frequency, and so on.  Similarly, a value of k = 1.5 represents
 *   a frequency 1.5 times the fundamental frequency.  The primary useful range of
 *   values of k are positive values in the range [0, N/2); for a real-valued signal
 *   values outside this range are essentially redundant.
 *   As an example of how to determine k, if a signal is being sampled at a rate of
 *   R samples per second, we are applying the Goertzel algorithm to a sequence of N
 *   samples, and frequency component whose strength is to be determined is F Hz,
 *   then we should take k = F * N / R.
 */
void goertzel_init(GOERTZEL_STATE *gp, uint32_t N, double k);

/*
 * Perform one iteration of the main loop of the Goertzel algorithm.
 *   @param gp  Pointer to structure containing the algorithm state.
 *   @param x is the sample of the signal at the current iteration.
 */
void goertzel_step(GOERTZEL_STATE *gp, double x);

/*
 * Perform the final iteration of the main loop of the Goertzel algorithm,
 * returning the real-valued "strength" of the frequency component being
 * analyzed.
 *
 *   @param gp  Pointer to structure containing the algorithm state.
 *   @param x  The last sample of the signal; i.e. the sample at index N-1.
 *   @return  The "strength" of the signal at the frequency corresponding
 *   to the specified index k.  In more precise terms, the generalized Goertzel
 *   algorithm described in the cited paper computes a value y which is the value
 *   of the discrete-time Fourier transform (DTFT) of the input signal,
 *   for the specified frequency "index" k.  This value y is a complex value that
 *   encodes both magnitude and phase information.  We are only interested in the
 *   squared magnitude |y|^2, which can be interpreted as the relative energy of the
 *   specified frequency component.  Furthermore, the DTFT of a real-valued signal
 *   is symmetric with respect to negation of the frequency index k, so to
 *   account for total amount of energy at a particular frequency and to enable
 *   meaningful comparison with a fixed energy threshold, we return the
 *   value 2|y|^2.
 */
double goertzel_strength(GOERTZEL_STATE *gp, double x);

#endif
