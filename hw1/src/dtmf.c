
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "const.h"
#include "audio.h"
#include "dtmf.h"
#include "dtmf_static.h"
#include "goertzel.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 */

/**
 * DTMF generation main function.
 * DTMF events are read (in textual tab-separated format) from the specified
 * input stream and audio data of a specified duration is written to the specified
 * output stream.  The DTMF events must be non-overlapping, in increasing order of
 * start index, and must lie completely within the specified duration.
 * The sample produced at a particular index will either be zero, if the index
 * does not lie between the start and end index of one of the DTMF events, or else
 * it will be a synthesized sample of the DTMF tone corresponding to the event in
 * which the index lies.
 *
 *  @param events_in  Stream from which to read DTMF events.
 *  @param audio_out  Stream to which to write audio header and sample data.
 *  @param length  Number of audio samples to be written.
 *  @return 0 if the header and specified number of samples are written successfully,
 *  EOF otherwise.
 */

int stringToInt(char *str){
  int sum = 0;
  while (*str != '\0'){ //parse through string and check if an integer character
    if (*str >= '0' && *str <= '9'){
      sum = (sum *10) + (int)*str - 48;
      str++;
    }
    else {
      return -100000;
    }
  }
  return sum;
}

int dtmf_generate(FILE *events_in, FILE *audio_out, uint32_t length) {
   	int32_t start;
   	int32_t end;
   	char symbol;
   	int c;
   	double row;
   	double column;
   	int16_t sample;
   	int16_t noise_sample;
   	double w;
   	double fr; //frequency row for dtmf
   	double fc; //frequency column for dtmf
   	FILE *fp;
   	if (noise_file != NULL) {
   		fp = fopen(noise_file, "r");
   		for (int i = 0; i < 24; i++){
   			fgetc(fp);
   		}
   	}
   	//Set default values for header struct
   	AUDIO_HEADER hp;
   	hp.magic_number = AUDIO_MAGIC;
   	hp.data_offset = AUDIO_DATA_OFFSET;
   	hp.data_size = length * 2;
   	hp.encoding = PCM16_ENCODING;
   	hp.sample_rate = AUDIO_FRAME_RATE;
   	hp.channels = AUDIO_CHANNELS;
   	audio_write_header(audio_out, &hp);
   	for (int i = 0; i < length; i++){
   		if (i >= end) {
   			//Parse one dtmf event
        start = 0;
        end = 0;
        c = 0;
   			if ((c = fgetc(events_in)) == EOF){
          end = 0;
          goto Line;
        } //convert char into int
        c = c - 48;
   			while (c != '\t' - 48){
   				start = start * 10 + c;
   				if ((c = fgetc(events_in)) == EOF){
            end = 0;
            goto Line;
          }
          c = c - 48;
   			}
   			if (start < end) {
   				return EOF;
   			}
   			if ((c = fgetc(events_in)) == EOF) {
          end = 0;
          goto Line;
        }
        c = c - 48;
   			while (c != '\t' - 48){
   				end = end * 10 + c;
   				if ((c = fgetc(events_in)) == EOF){
            end = 0;
            break;
          }
          c = c - 48;
   			}
   			if (start > end){
   				return EOF;
   			}
        symbol = fgetc(events_in); //find symbol in dtmf table
        fgetc(events_in); //eat newline character
   		}
      Line:
   		if (i >= start && i < end){ //if between audio range
   			switch (symbol) {
   				case('1'):
   					fr = 697;
   					fc = 1209;
   					break;
   				case('2'):
   					fr = 697;
   					fc = 1336;
   					break;
   				case('3'):
   					fr = 697;
   					fc = 1477;
   					break;
   				case('A'):
   					fr = 697;
   					fc = 1633;
   					break;
   				case('4'):
   					fr = 770;
   					fc = 1209;
   					break;
   				case('5'):
   					fr = 770;
   					fc = 1336;
   					break;
   				case('6'):
   					fr = 770;
   					fc = 1477;
   					break;
   				case('B'):
   					fr = 770;
   					fc = 1633;
   					break;
   				case('7'):
   					fr = 852;
   					fc = 1209;
   					break;
   				case('8'):
   					fr = 852;
   					fc = 1336;
   					break;
   				case('9'):
   					fr = 852;
   					fc = 1477;
   					break;
   				case('C'):
   					fr = 852;
   					fc = 1633;
   					break;
   				case('*'):
   					fr = 941;
   					fc = 1209;
   					break;
   				case('0'):
   					fr = 941;
   					fc = 1336;
   					break;
   				case('#'):
   					fr = 941;
   					fc = 1477;
   					break;
   				case('D'):
   					fr = 941;
   					fc = 1633;
   					break;
   				default:
   					return EOF;
   			}
   			row = cos(2.0 * M_PI * fr * (double)i / (double)AUDIO_FRAME_RATE);
   			column = cos(2.0 * M_PI * fc * (double)i / (double)AUDIO_FRAME_RATE);
   			sample = (int16_t)(((0.5 * row) + (0.5 * column)) * INT16_MAX);
   		}
   		else {
   			sample = 0;
   		}
   		//synthesize noise sample from noise_file
   		if (noise_file == NULL) {
   			audio_write_sample(audio_out, sample);
   		}
   		else {
   			if (audio_read_sample(fp, &noise_sample) == EOF) {
   				noise_sample = 0;
   			}
   			w = (pow(10,noise_level/10.0))/(1+pow(10,noise_level/10.0));
   			sample = ((w * noise_sample) + ((1-w) * sample));
   			audio_write_sample(audio_out, sample);
   		}
   	}
    return 0;
}


/**
 * DTMF detection main function.
 * This function first reads and validates an audio header from the specified input stream.
 * The value in the data size field of the header is ignored, as is any annotation data that
 * might occur after the header.
 *
 * This function then reads audio sample data from the input stream, partititions the audio samples
 * into successive blocks of block_size samples, and for each block determines whether or not
 * a DTMF tone is present in that block.  When a DTMF tone is detected in a block, the starting index
 * of that block is recorded as the beginning of a "DTMF event".  As long as the same DTMF tone is
 * present in subsequent blocks, the duration of the current DTMF event is extended.  As soon as a
 * block is encountered in which the same DTMF tone is not present, either because no DTMF tone is
 * present in that block or a different tone is present, then the starting index of that block
 * is recorded as the ending index of the current DTMF event.  If the duration of the now-completed
 * DTMF event is greater than or equal to MIN_DTMF_DURATION, then a line of text representing
 * this DTMF event in tab-separated format is emitted to the output stream. If the duration of the
 * DTMF event is less that MIN_DTMF_DURATION, then the event is discarded and nothing is emitted
 * to the output stream.  When the end of audio input is reached, then the total number of samples
 * read is used as the ending index of any current DTMF event and this final event is emitted
 * if its length is at least MIN_DTMF_DURATION.
 *
 *   @param audio_in  Input stream from which to read audio header and sample data.
 *   @param events_out  Output stream to which DTMF events are to be written.
 *   @return 0  If reading of audio and writing of DTMF events is sucessful, EOF otherwise.
 */
int dtmf_detect(FILE *audio_in, FILE *events_out) {
	  int rowInd;
	  int columnInd;
	  int start;
	  int end;
	  int currIndex = 0;
	  double maxRow;
	  double maxColumn;
	  char symbol;
    AUDIO_HEADER header;
    audio_read_header(audio_in, &header);
    uint32_t N = block_size;
    double k0 = N * 697.0 / 8000.0;
    double k1 = N * 770.0 / 8000.0;
    double k2 = N * 852.0 / 8000.0;
    double k3 = N * 941.0 / 8000.0;
    double k4 = N * 1209.0 / 8000.0;
    double k5 = N * 1336.0 / 8000.0;
    double k6 = N * 1477.0 / 8000.0;
    double k7 = N * 1633.0 / 8000.0;
    GOERTZEL_STATE *g0, *g1, *g2, *g3, *g4, *g5, *g6, *g7;
    double y0, y1, y2, y3, y4, y5, y6, y7;
    g0 = goertzel_state;
    g1 = goertzel_state + 1;
    g2 = goertzel_state + 2;
    g3 = goertzel_state + 3;
    g4 = goertzel_state + 4;
    g5 = goertzel_state + 5;
    g6 = goertzel_state + 6;
    g7 = goertzel_state + 7;
    int failure = 0;
    while (failure == 0){
    	goertzel_init(g0, N, k0);
    	goertzel_init(g1, N, k1);
    	goertzel_init(g2, N, k2);
    	goertzel_init(g3, N, k3);
    	goertzel_init(g4, N, k4);
    	goertzel_init(g5, N, k5);
    	goertzel_init(g6, N, k6);
    	goertzel_init(g7, N, k7);
    	double x;
    	int16_t y = 0;
    	start = currIndex;
    	for (int i = 0; i <= N - 2; i++){
        	if (audio_read_sample(audio_in, &y) == EOF) {
        		failure = -1;
        		N = i + 1;
        		break;
        	}
        	x = (double)y / INT16_MAX;
        	goertzel_step(g0, x);
        	goertzel_step(g1, x);
        	goertzel_step(g2, x);
        	goertzel_step(g3, x);
        	goertzel_step(g4, x);
        	goertzel_step(g5, x);
        	goertzel_step(g6, x);
        	goertzel_step(g7, x);
        	currIndex++; //increment index in audio_in
    	}
    	if (audio_read_sample(audio_in, &y) == EOF){
    		failure = -1;
    		if ((N/8.0) < MIN_DTMF_DURATION) {
    			break;
    		}
    	}
    	x = (double)y / INT16_MAX;
    	y0 = goertzel_strength(g0, x);
    	y1 = goertzel_strength(g1, x);
    	y2 = goertzel_strength(g2, x);
    	y3 = goertzel_strength(g3, x);
    	y4 = goertzel_strength(g4, x);
    	y5 = goertzel_strength(g5, x);
    	y6 = goertzel_strength(g6, x);
    	y7 = goertzel_strength(g7, x);
    	currIndex++;
    	end = currIndex;
    	//find the max row and column frequencies
    	maxRow = 0;
    	maxColumn = 0;
    	if (y0 >= maxRow){
    		maxRow = y0;
    	}
    	if (y1 >= maxRow){
    		maxRow = y1;
    	}
    	if (y2 >= maxRow){
    		maxRow = y2;
    	}
    	if (y3 >= maxRow){
    		maxRow = y3;
    	}
    	if (y4 >= maxColumn) {
    		maxColumn = y4;
    	}
    	if (y5 >= maxColumn){
    		maxColumn = y5;
    	}
    	if (y6 >= maxColumn){
    		maxColumn = y6;
    	}
    	if (y7 >= maxColumn){
    		maxColumn = y7;
    	}
    	//check if minimum of -20dB is met
    	if ((maxRow + maxColumn) < MINUS_20DB) {
    		continue; //do not write anything
    	}
    	//check if twist between -4dB and 4dB
    	if ((maxRow/maxColumn) > FOUR_DB || (maxColumn/maxRow) > FOUR_DB){
    		continue; //do not write anything
    	}
    	if ((maxRow/y0) < SIX_DB && (maxRow/y0) != 1) {
    		continue;
    	}
    	if ((maxRow/y1) < SIX_DB && (maxRow/y1) != 1) {
    		continue;
    	}
    	if ((maxRow/y2) < SIX_DB && (maxRow/y2) != 1) {
    		continue;
    	}
    	if ((maxRow/y3) < SIX_DB && (maxRow/y3) != 1) {
    		continue;
    	}
		  if ((maxColumn/y4) < SIX_DB && (maxColumn != y4)) {
    		continue;
    	}
    	if ((maxColumn/y5) < SIX_DB && (maxColumn != y5)) {
    		continue;
    	}
    	if ((maxColumn/y6) < SIX_DB && (maxColumn != y6)) {
    		continue;
    	}
    	if ((maxColumn/y7) < SIX_DB && (maxColumn != y7)) {
    		continue;
    	}

    	if (maxRow == y0) {
    		rowInd = 0;
    	}
    	else if(maxRow == y1) {
    		rowInd = 1;
    	}
    	else if(maxRow == y2) {
    		rowInd = 2;
    	}
    	else if(maxRow == y3) {
    		rowInd = 3;
    	}
    	if (maxColumn == y4) {
    		columnInd = 0;
    	}
    	else if (maxColumn == y5) {
    		columnInd = 1;
    	}
    	else if (maxColumn == y6) {
    		columnInd = 2;
    	}
    	else if (maxColumn == y7) {
    		columnInd = 3;
    	}
    	symbol = *(*(dtmf_symbol_names + rowInd) + columnInd);
    	fprintf(events_out, "%d", start);
    	fputc('\t', events_out);
    	fprintf(events_out, "%d", end);
    	fputc('\t', events_out);
    	fputc(symbol, events_out);
    	fputc('\n', events_out);
	}
	return 0; //success
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the operation mode of the program (help, generate,
 * or detect) will be recorded in the global variable `global_options`,
 * where it will be accessible elsewhere in the program.
 * Global variables `audio_samples`, `noise file`, `noise_level`, and `block_size`
 * will also be set, either to values derived from specified `-t`, `-n`, `-l` and `-b`
 * options, or else to their default values.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected program operation mode, and global variables `audio_samples`,
 * `noise file`, `noise_level`, and `block_size` to contain values derived from
 * other option settings.
 */

int validargs(int argc, char **argv) {
	if (argc <= 1) {
		return -1; //no enough arguments
	}
	char *str ;
	size_t argInd = 1;
	str = *(argv + argInd);
	//check for -h flag, ignore rest
	if (*str == '-') {
		if (*(str + 1) == 'h' && *(str + 2) == '\0') {
			global_options = HELP_OPTION;
		return 0;
		}
		//check for -g flag pairing with -t, -n, -l
		else if (*(str + 1) == 'g' && *(str + 2) == '\0'){
			global_options = GENERATE_OPTION;
      audio_samples = AUDIO_FRAME_RATE; //default audio sample value
      noise_file = NULL; //default noise_file (NULL)
      noise_level = 0; //default noise level value
			for (argInd = 2; argInd < argc; argInd++) {

				str = *(argv + argInd); //access string in array

				if (*str == '-') {
					if (*(str + 1) == 't' && *(str + 2) == '\0') {
						argInd++; //move to MSEC parameter
						str = *(argv + argInd);
						int para = stringToInt(str);
						if (para >= 0 && para <= UINT32_MAX){ //check if between [0, UNIT32_MAX]
							audio_samples = para * 8; //convert msecs into audio_samples
						}
						else {
							return -1;
						}
					}
					else if (*(str + 1) == 'n' && *(str + 2) == '\0') {
						argInd++; //move to noise file parameter
						str = *(argv + argInd);
						noise_file = str;
					}

					else if (*(str + 1) == 'l' && *(str + 2) == '\0') {
						argInd++; //move to noise level parameter
						str = *(argv + argInd);
						int para = stringToInt(str);
						if (para >= -30 && para <= 30) { //check if between [-30, 30]
							noise_level = para;
						}
						else {
							return -1;
						}
					}
					else {
						return  -1;
					}
				}
			}
		}
		//check for -d flag pairing with -b
		else if (*(str + 1) == 'd'){
			global_options = DETECT_OPTION;
      block_size = DEFAULT_BLOCK_SIZE; //default block size value
			for (argInd = 2; argInd < argc; argInd++) {

				str = *(argv + argInd); //access string in array

				if (*str == '-') {
					if (*(str + 1) == 'b' && *(str + 2) == '\0') {
						argInd++; //move to block size parameter
						str = *(argv + argInd);
						int para = stringToInt(str);
						if (para >= 10 && para <= 1000) { //check if between [10, 1000]
							block_size = para;
						}
						else {
							return -1;
						}
					}
				}
				else {
					return -1;
				}
			}
		}
	}
	else {
		return -1; //no -dash before argument
	}
	return 0;
}
