#include <stdio.h>
#include <stdlib.h>

#include "const.h"
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

int main(int argc, char **argv)
{
    if (argc <= 1) {
        USAGE(*argv, EXIT_FAILURE); //no enough arguments
    }
    if (validargs(argc, argv) == 0) { //if arguments are valid
        if (global_options == HELP_OPTION){
            USAGE(*argv, EXIT_SUCCESS);
        }
        else if (global_options == GENERATE_OPTION){
            if(dtmf_generate(stdin, stdout, audio_samples) == EOF){
                exit(EXIT_FAILURE);
            }
        }
        else if (global_options == DETECT_OPTION) {
            if(dtmf_detect(stdin, stdout) == EOF){
                exit(EXIT_FAILURE);
            }
        }
	}
    else { //if arguments are invalid
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
