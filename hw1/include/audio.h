/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

/*
 * Sun audio file (.au) format:
 * All fields are stored in big-endian format, including the sample data.
 * The file begins with a header, consisting of six unsigned 32-bit words.
 *   The first field in the header is the "magic number", which must be
 *     equal to 0x2e736e64 (AUDIO_MAGIC, the four ASCII characters: ".snd").
 *   The second field in the header is the "data offset", which is the
 *     number of bytes from the beginning of the file that the audio sample
 *     data begins.  Although as far as I know, the format specifications
 *     require that this value must be divisible by 8, some applications
 *     ("sox", for example) violate this requirement.  For audio files that
 *     we create, this value will always be equal to 24 (AUDIO_DATA_OFFSET),
 *     which is the number of bytes in the header.  For audio files that we read,
 *     we must be prepared to accept larger values and to skip over the annotation
 *     characters in order to arrive at the start of the audio sample data.
 *   The third field in the header is the "data size", which is the number
 *     of bytes of audio sample data.  For audio files that we create,
 *     this field will contain the actual number of bytes of audio sample data.
 *     For audio files that we read, we will ignore this field and simply read
 *     sample data until EOF is seen.
 *   The fourth field in the header specifies the encoding used for the
 *     audio samples.  We will only support the following value:
 *        3  (PCM16_ENCODING, specifies 16-bit linear PCM encoding)
 *     This corresponds to a number of bytes per sample of 2 (AUDIO_BYTES_PER_SAMPLE).
 *   The fifth field in the header specifies the "sample rate", which is the
 *     number of frames per second.
 *     For us, this will always be 8000 (AUDIO_FRAME_RATE).
 *   The sixth field in the header specifies the number of audio channels.
 *     For us, this will always be 1 (AUDIO_CHANNELS), indicating monaural audio.
 */

#define AUDIO_MAGIC (0x2e736e64)
#define PCM16_ENCODING (3)
#define AUDIO_FRAME_RATE 8000
#define AUDIO_CHANNELS 1
#define AUDIO_BYTES_PER_SAMPLE 2
#define AUDIO_DATA_OFFSET 24

typedef struct audio_header {
    uint32_t magic_number;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t encoding;
    uint32_t sample_rate;
    uint32_t channels;
} AUDIO_HEADER;

/*
 * Following the header is an optional "annotation field", which can be used
 * to store additional information (metadata) in the audio file.
 * According to the specifications, the length of this field must be a multiple
 * of eight bytes and it must be terminated with at least one null ('\0') byte,
 * but since we are not using the annotation data we will just skip over it and
 * we will not validate that the length is a multiple of eight or that the annotation
 * data is null-terminated.
 *
 * Audio data begins immediately following the annotation field (or immediately
 * following the header, if there is no annotation field).  The audio data occurs
 * as a sequence of *frames*, where each frame contains data for one sample on each
 * of the audio channels.  The size of a frame therefore depends both on the sample
 * encoding and on the number of channels.  For example, if the sample encoding is
 * 16-bit PCM (i.e. two bytes per sample) and the number of channels is two,
 * then the number of bytes in a frame will be 2 * 2 = 4.  If the sample encoding is
 * 32-bit PCM (i.e. four bytes per sample) and the number of channels is two,
 * then the number of bytes in a frame will be 2 * 4 = 8.
 * For us, the number of bytes per frame will always be AUDIO_CHANNELS * AUDIO_BYTES_PER_SAMPLE;
 * that is, 2.
 *
 * Within a frame, the sample data for each channel occurs in sequence.
 * For example, in case of 16-bit PCM encoded stereo, the first two bytes of each
 * frame represents a single sample for channel 0 and the second two bytes
 * represents a single sample for channel 1.  Samples are signed values encoded
 * in two's-complement and are presented in big-endian (most significant byte first)
 * byte order.  For us, there will be just one two-byte sample per frame.
 */

/*
 * File format:
 *
 * +------------------+--------------------------+-------+     +-------+
 * + Header (24 bytes)| Annotation (optional) \0 | Frame | ... | Frame |
 * +------------------+--------------------------+-------+     +-------+
 *                                               ^
 * <----------- data offset -------------------->(8-byte boundary)
 *
 * Frame format:
 *
 * +-----------------------------------+-----------------------------------+
 * | Sample 0 MSB | ... | Sample 0 LSB | Sample 1 MSB | ... | Sample 1 LSB |
 * +-----------------------------------+-----------------------------------+
 */

/**
 * @brief Read the header of a Sun audio file and check it for validity.
 * @details  This function reads 24 bytes of data from the standard input and
 * interprets it as the header of a Sun audio file.  The data is decoded into
 * six unsigned int values, assuming big-endian byte order.   The decoded values
 * are stored into the AUDIO_HEADER structure pointed at by hp.
 * The header is then checked for validity, which means:  no error occurred
 * while reading the header data, the magic number is valid, the value of encoding
 * field is PCM16_ENCODING, and the value of the channels field is AUDIO_CHANNELS.
 * Once the header is read and validated, any annotation data that may be present
 * is skipped, so that when this function returns the input pointer is pointing
 * at the the start of the audio sample data.
 *
 * @param hp  A pointer to the AUDIO_HEADER structure that is to receive
 * the data.
 * @return  0 if a valid header was read, otherwise EOF.
 */
int audio_read_header(FILE *in, AUDIO_HEADER *hp);

/**
 * @brief  Write the header of a Sun audio file to the standard output.
 * @details  This function takes the pointer to the AUDIO_HEADER structure passed
 * as an argument, encodes this header into 24 bytes of data according to the Sun
 * audio file format specifications, and writes this data to the standard output.
 *
 * @param  hp  A pointer to the AUDIO_HEADER structure that is to be output.
 * @return  0 if the function is successful at writing the data; otherwise EOF.
 */
int audio_write_header(FILE *out, AUDIO_HEADER *hp);

/**
 * Read a single two-byte audio sample from an input stream.
 *
 *   @param in  Input stream from which sample is to be read.
 *   @param samplep  Pointer to variable into which to store the sample value.
 *   @return 0 on success, EOF otherwise.
 */
int audio_read_sample(FILE *in, int16_t *samplep);

/**
 * Write a single two-byte audio sample to an output stream.
 *
 *   @param out  Output stream to which sample is to be written.
 *   @param sample  Sample to be written.
 *   @return 0 on success, EOF otherwise.
 */
int audio_write_sample(FILE *out, int16_t sample);

#endif
