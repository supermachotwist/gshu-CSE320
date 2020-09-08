/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef CONST_H
#define CONST_H

#include <stdio.h>
#include <stdint.h>

#include "audio.h"
#include "dtmf.h"
#include "goertzel.h"

#define USAGE(program_name, retcode) do { \
fprintf(stderr, "USAGE: %s %s\n", program_name, \
"[-h] -g|-d [-t MSEC] [-n NOISE_FILE] [-l LEVEL] [-b BLOCKSIZE]\n" \
"   -h       Help: displays this help menu.\n" \
"   -g       Generate: read DTMF events from standard input, output audio data to standard output.\n" \
"   -d       Detect: read audio data from standard input, output DTMF events to standard output.\n\n" \
"            Optional additional parameters for -g (not permitted with -d):\n" \
"               -t MSEC         Time duration (in milliseconds, default 1000) of the audio output.\n" \
"               -n NOISE_FILE   specifies the name of an audio file containing \"noise\" to be combined\n" \
"                               with the synthesized DTMF tones.\n" \
"               -l LEVEL        specifies the loudness ratio (in dB, positive or negative) of the\n" \
"                               noise to that of the DTMF tones.  A LEVEL of 0 (the default) means the\n" \
"                               same level, negative values mean that the DTMF tones are louder than\n" \
"                               the noise, positive values mean that the noise is louder than the\n" \
"                               DTMF tones.\n\n" \
"            Optional additional parameter for -d (not permitted with -g):\n" \
"               -b BLOCKSIZE    specifies the number of samples (range [10, 1000], default 100)\n" \
"                                in each block of audio to be analyzed for the presence of DTMF tones.\n" \
); \
exit(retcode); \
} while(0)

/* Options info, set by validargs. */
#define HELP_OPTION (0x1)
#define GENERATE_OPTION (0x2)
#define DETECT_OPTION (0x4)

int global_options;  // Bitmap specifying mode of program operation.
char *noise_file;    // Name of noise file, or NULL if none.
int noise_level;     // Ratio (in dB) of noise level to DTMF tone level.
int block_size;      // Block size used in DTMF tone detection.
int audio_samples;   // Number of samples in generated audio file.

/*
 * Some fixed parameters that we use for this program.
 */
#define DEFAULT_BLOCK_SIZE 100
#define FOUR_DB 2.51188643151
#define SIX_DB 3.981071706
#define TEN_DB 10.0
#define MINUS_20DB 0.01
#define MIN_DTMF_DURATION 0.03  // in seconds

/*
 * The following global variables have been provided for you.
 * You MUST use them for their stated purposes, because you are not permitted
 * to declare any arrays (or use any array brackets at all) in your own code.
 * Also, some of the tests we make on your program may rely on being able to
 * inspect the contents of these variables.
 */

/*
 * Line buffer for use in reading DTMF events.
 */
#define LINE_BUF_SIZE 80
char line_buf[LINE_BUF_SIZE];

/*
 * Statically allocated state objects for Goertzel filter instances,
 * one for each DTMF frequency.
 */
GOERTZEL_STATE goertzel_state[NUM_DTMF_FREQS];

/*
 * Below this line are prototypes for functions that MUST occur in your program.
 * Non-functioning stubs for all these functions have been provided in the various source
 * files, where detailed specifications for the required behavior of these functions have
 * been given.
 *
 * Your implementations of these functions MUST have exactly the specified interfaces and
 * behave exactly according to the specifications, because we may choose to test any or all
 * of them independently of your main program.
 */

int validargs(int argc, char **argv);

int audio_read_header(FILE *in, AUDIO_HEADER *hp);
int audio_write_header(FILE *out, AUDIO_HEADER *hp);
int audio_read_sample(FILE *in, int16_t *samplep);
int audio_write_sample(FILE *out, int16_t sample);

int dtmf_generate(FILE *events_in, FILE *audio_out, uint32_t len);
int dtmf_detect(FILE *audio_in, FILE *events_out);

#endif
