#include <stdio.h>

#include "audio.h"
#include "debug.h"

int audio_read_header(FILE *in, AUDIO_HEADER *hp) {
	//read first 24 bytes of file header and place into AUDIO_HEADER struct
	char ch;
	(*hp).magic_number = 0;
    for (int i = 0; i < 4; i++){
    	if ((ch = fgetc(in)) == EOF){
    		return EOF;
    	}
    	(*hp).magic_number = (ch | ((*hp).magic_number<<8));
    }
    (*hp).data_offset = 0;
    for (int i = 0; i < 4; i++){
    	if ((ch = fgetc(in)) == EOF){
    		return EOF;
    	}
    	(*hp).data_offset = (ch | ((*hp).data_offset<<8));
    }
    (*hp).data_size = 0;
    for (int i = 0; i < 4; i++){
    	if ((ch = fgetc(in)) == EOF){
    		return EOF;
    	}
    	(*hp).data_size = (ch | ((*hp).data_size<<8));
    }
    (*hp).encoding = 0;
    for (int i = 0; i < 4; i++){
    	if ((ch = fgetc(in)) == EOF){
    		return EOF;
    	}
    	(*hp).encoding = (ch | ((*hp).encoding<<8));
    }
    (*hp).sample_rate = 0;
    for (int i = 0; i < 4; i++){
    	if ((ch = fgetc(in)) == EOF){
    		return EOF;
    	}
    	(*hp).sample_rate = (ch | ((*hp).sample_rate<<8));
    }
    (*hp).channels = 0;
    for (int i = 0; i < 4; i++){
    	if ((ch = fgetc(in)) == EOF){
			return EOF;
		}
    	(*hp).channels = (ch | ((*hp).channels<<8));
    }
    //check for correct values of header encoding
    if ((*hp).magic_number != AUDIO_MAGIC){
    	return EOF;
    }
    if ((*hp).encoding != PCM16_ENCODING){
    	return EOF;
    }
    if ((*hp).channels != AUDIO_CHANNELS){
    	return EOF;
    }
    return 0;
}

int audio_write_header(FILE *out, AUDIO_HEADER *hp) {
	//read first 24 bytes of header to standard output file
	for (int i = 24; i >=0; i = i-8){
		if(fputc((*hp).magic_number >>i, out) == EOF) {
			return EOF;
		}
	}
	for (int i = 24; i >=0; i = i-8){
		if(fputc((*hp).data_offset >>i, out) == EOF) {
			return EOF;
		}
	}
	for (int i = 24; i >=0; i = i-8){
		if(fputc((*hp).data_size >>i, out) == EOF) {
			return EOF;
		}
	}
	for (int i = 24; i >=0; i = i-8){
		if(fputc((*hp).encoding >>i, out) == EOF) {
			return EOF;
		}
	}
	for (int i = 24; i >=0; i = i-8){
		if(fputc((*hp).sample_rate >>i, out) == EOF) {
			return EOF;
		}
	}
	for (int i = 24; i >=0; i = i-8){
		if(fputc((*hp).channels >>i, out) == EOF) {
			return EOF;
		}
	}
    return 0;
}

int audio_read_sample(FILE *in, int16_t *samplep) {
    int ch;
    *samplep = 0;
    if ((ch = fgetc(in)) == EOF) {
        return EOF;
    }
    *samplep = *samplep + ch;
    *samplep = *samplep<<8;
    if ((ch = fgetc(in)) == EOF) {
        return EOF;
    }
    *samplep = *samplep + ch;
    return 0;
}

int audio_write_sample(FILE *out, int16_t sample) {
    int16_t mask = 0xff; //right 8 bit mask
    for (int i = 8; i >= 0; i = i-8){
    	if(fputc((sample >>i) & mask, out) == EOF){
    		return EOF;
    	}
    }
    return 0;
}
