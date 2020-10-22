/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */

#include <stdlib.h>

#include "legion.h"

int main(int argc, char const *argv[]) {
    sf_init();
    run_cli(stdin, stdout);
    sf_fini();
    return EXIT_SUCCESS;
}
