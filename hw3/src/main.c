#include <stdio.h>
#include "sfmm.h"

/*
 * Define WEAK_MAGIC during compilation to use MAGIC of 0x0 for debugging purposes.
 * Note that this feature will be disabled during grading.
 */

#ifdef WEAK_MAGIC
int sf_weak_magic = 1;
#endif

/* Read and write a word at address p */
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val)^MAGIC)

int main(int argc, char const *argv[]) {
    double* ptr = sf_malloc(sizeof(double));

    *ptr = 320320320e-320;

    printf("%f\n", *ptr);

    sf_free(ptr);
    //printf("%lu", sizeof(sf_block));
    /*void *bp = sf_mem_grow();
    (*(size_t *)(bp) = (4082));
    printf("%lu", ((*(size_t *)(bp))));*/
    //sf_malloc(0);
    //sf_malloc(4066);
    //sf_malloc(4066);
	//void *bp = sf_malloc(64);
	//bp = sf_realloc(bp, 16);
	//bp = sf_realloc(bp, 100);
	//bp = sf_realloc(bp, 32);
	//bp = sf_realloc(bp, 600);
	//void *bp =sf_malloc(16);
	//sf_free(bp);
	//sf_malloc(16);
	//sf_free(bp);
	//bp = sf_realloc(bp, 4032);
	//bp = sf_realloc(bp, 4066);
	//sf_malloc(4066);
	//sf_free(bp);
	//sf_show_free_lists();
	//sf_show_quick_lists();
	//sf_malloc(-100);
	//x = sf_realloc(x, 32);
	//x = sf_realloc(x, 16);
	//x = sf_realloc(x, 32);
	//x = sf_realloc(x, 16);
	//x = sf_realloc(x, 32);
	//x = sf_realloc(x, 16);
	sf_show_heap();
	//sf_show_block((sf_block *)(bp - 16));


	/*size_t x = 5;
	size_t z ;
	PUT(&z, x);
	size_t y = GET(&z)^MAGIC;
	y = GET(&z)^MAGIC;
	printf("%lu",y);*/

    return EXIT_SUCCESS;
}
