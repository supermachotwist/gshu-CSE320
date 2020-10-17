/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"

/* Basic constant and macros for manipulating free lists*/
#define WSIZE 8 /*Header and footer size (bytes) */
#define DSIZE 16 /*Double word size (bytes) */

#define MAX(x,y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size | alloc))

/* Read and write a word at address p */
#define GET(p) ((*(size_t *)(p)))
#define PUT(p, val) (*(size_t *)(p) = (val)^MAGIC)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) ((GET(p)^MAGIC) & ~0x7)
#define GET_ALLOC(p) ((GET(p)^MAGIC) & 0x4)
#define GET_PRV_ALLOC(p) ((GET(p)^MAGIC) & 0x2)

/* Given block ptr bp, computer address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, get next and previous address of current free block  */
#define NEXT_LINK(bp) ((char *)(bp) + WSIZE)
#define PREV_LINK(bp) ((char *)(bp) + DSIZE)

/* Given block ptr bp, computer address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/*Global Variables */
void *last_bp = NULL; //Points to the last block in the heap

void remove_main_list(sf_block* sp) {
	sp->body.links.prev->body.links.next = sp->body.links.next; //Set next of prev block to next of current block
	sp->body.links.next->body.links.prev = sp->body.links.prev; //Set prev of next block to prev of current block
}

static void *coalesce(void *bp) {
	size_t next_alloc;
	size_t prev_alloc;
	//Set prev_alloc

	prev_alloc = GET_PRV_ALLOC(HDRP(bp));

	//Set next_alloc
	if ((void *)NEXT_BLKP(bp) < sf_mem_end()){
		next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	}
	else { //Pretend memory outside heap is allocated to avoid coalesing
		next_alloc = 1;
	}

	size_t size = GET_SIZE(HDRP(bp));

	/* Case 1: Previous and next blocks are allocated */
	if (prev_alloc && next_alloc) {
		return bp;
	}

	/* Case 2: Only next block is free */
	else if (prev_alloc && !next_alloc) {
		sf_block *sp = (void *)NEXT_BLKP(bp) - DSIZE; //Get struct ptr of free block
		remove_main_list(sp);
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size,0x2)); //Obfuscate the header data
		PUT(FTRP(bp), PACK(size,0x2));
		/* Check whether created block is last block */
		if ((GET_SIZE(HDRP(bp)) + bp) == sf_mem_end()) {
			last_bp = bp;
		}
	}

	/* Case 3: Only previous block is free */
	else if (!prev_alloc && next_alloc) {
		sf_block *sp = (void *)PREV_BLKP(bp) - DSIZE; //Get struct ptr of free block
		remove_main_list(sp);
		/* Find previous allocated bit from previous block */
		prev_alloc = GET_PRV_ALLOC(HDRP(PREV_BLKP(bp)));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size,(prev_alloc)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,prev_alloc));
		bp = PREV_BLKP(bp);
		/* Check whether created block is last block */
		if ((GET_SIZE(HDRP(bp)) + bp) == sf_mem_end()) {
			last_bp = bp;
		}
	}

	/* Case 4: Previous and next blocks are free */
	else if (!prev_alloc && !next_alloc) {
		sf_block *sp = (void *)NEXT_BLKP(bp) - DSIZE; //Get struct ptr of free block
		remove_main_list(sp);
		sp = (void *)PREV_BLKP(bp) - DSIZE; //Get struct ptr of free block
		remove_main_list(sp);
		prev_alloc = GET_PRV_ALLOC(HDRP(PREV_BLKP(bp)));
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, prev_alloc));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, prev_alloc));
		bp = PREV_BLKP(bp);
		/* Check whether created block is last block */
		if ((GET_SIZE(HDRP(bp)) + bp) == sf_mem_end()) {
			last_bp = bp;
		}
	}
	return bp;

}

/* Given head and size, find free block in quick list that can hold block */
sf_block *find_quick_fit(struct sf_block *sp, size_t size) {
	sf_block *current = sp;

	while (current != NULL) {
		sf_block *next = current->body.links.next;

		/* Find block of sufficient size and return struct pointer to that */
		void *bp = (void *)current + DSIZE;
		if (GET_SIZE(HDRP(bp)) >= size){
			return current;
		}

		current = next;
	}
	/* When search fails */
	return NULL;
}

/* Given head and size, find free block in main list that can hold block */
sf_block *find_main_fit(struct sf_block *sp, size_t size) {
	sf_block *current = sp->body.links.next;
	sf_block *head = sp; //Address to head to be used to check completion of full loop
	sf_block *next;

	while (current != head) {
		next = current->body.links.next;

		/* Find block of sufficient size and return struct pointer to that */
		void *bp = (void *)current + DSIZE;
		size_t rsize = GET_SIZE(HDRP(bp));
		if (rsize >= size){
			return current;
		}

		current = next;
	}
	/* When search fails */
	return NULL;
}

/* Return correct class size for quick lists */
int find_quick_class(size_t size){
	int index = -1;
	if (size == 32){
		index = 0;
	}
	else if (size == 48) {
		index = 1;
	}
	else if (size == 64) {
		index = 2;
	}
	else if (size == 80) {
		index = 3;
	}
	else if (size == 96) {
		index = 4;
	}
	else if (size == 112) {
		index = 5;
	}
	else if (size == 128) {
		index = 6;
	}
	else if (size == 144) {
		index = 7;
	}
	else if (size == 160) {
		index = 8;
	}
	else if (size == 176) {
		index = 9;
	}
	return index;
}

/* Return the correct class size index for given block size */
int find_main_class(size_t size){
	int index = -1;
	if (size == 32){
		index = 0;
	}
	else if (size > 32 && size <= 64) {
		index = 1;
	}
	else if (size > 64 && size <= 128) {
		index = 2;
	}
	else if (size > 128 && size <= 256) {
		index = 3;
	}
	else if (size > 256 && size <= 512) {
		index = 4;
	}
	else if (size > 512 && size <= 1024) {
		index = 5;
	}
	else if (size > 1024 && size <= 2048) {
		index = 6;
	}
	else if (size > 2048 && size <= 4096) {
		index = 7;
	}
	else if (size > 4096 && size <= 8192) {
		index = 8;
	}
	else if (size > 8192) {
		index = 9;
	}
	return index;
}

/* Given a ptr to body bp, add block to the front of main list of correct size */
void add_main_list(void *bp) {
	size_t size = GET_SIZE(HDRP(bp));

	/* Check main list class to add to */
	int index = find_main_class(size);
	size_t prev_alloc = GET_PRV_ALLOC(HDRP(bp));

	sf_block *sp = (bp - DSIZE);
	sp->header = GET(HDRP(bp));
	if (prev_alloc == 0){ //If previous block is free, set foot of current struct to footer of previous block
		sp->prev_footer = GET(FTRP(PREV_BLKP(bp)));
	}
	sp->body.links.next = sf_free_list_heads[index].body.links.next; //Set next of current block to next of head
	sf_free_list_heads[index].body.links.next = sp; //Set next of head to current block
	sp->body.links.prev = &sf_free_list_heads[index]; //Set prev of current block to head
	sp->body.links.next->body.links.prev = sp; //Set next of current block's prev to be current block
}

/* Try to place requested block at the beginning of the free block
   splitting only if the size of remainder would be less than than the
   minimum block size
   2nd arg = size of block being allocated*/
void allocate(struct sf_block *sp, size_t size) {
	size_t prev_alloc;
	void *bp = (void *)sp + DSIZE;

	prev_alloc = GET_PRV_ALLOC(HDRP(bp));

	size_t ssize = GET_SIZE(HDRP(bp)); //size of the whole free block

	if ((ssize - size) < 32) { //if a splinter is created
		PUT(HDRP(bp), PACK(ssize, (prev_alloc | 0x4))); // Set as allocated with size as is
		/* Check whether created block is last block */
		if ((GET_SIZE(HDRP(bp)) + bp) == sf_mem_end()) {
			last_bp = bp;
		}
	}
	else { //
		PUT(HDRP(bp), PACK(size, (prev_alloc | 0x4))); // Set as allocated with new size
		/* Setting up remaining space as new free block */
		void *bp1 = NEXT_BLKP(bp); //body of remaining block
		PUT(HDRP(bp1), PACK((ssize - size), 0x2)); //Current is free, prev is allocated
		PUT(FTRP(bp1), PACK((ssize - size), 0x2));
		bp1 = coalesce(bp1);
		add_main_list(bp1);
		/* Check whether created block is last block */
		if ((GET_SIZE(HDRP(bp1)) + bp1) == sf_mem_end()) {
			last_bp = bp1;
		}
	}

	sp->header = GET(HDRP(bp)); //Set header of struct. Footer not set and used as payload

	if (prev_alloc == 0) { //If previous block is free, set foot of current struct to footer of previous block
		sp->prev_footer = GET(FTRP(PREV_BLKP(bp)));
	}

}

void *sf_malloc(size_t size) {
    if (size == 0) {
    	return NULL;
    }
    /* Set new size including overhead and alignment reqs */
    size += 8; //Add header overhead
    while (size % 16 != 0) { //Add padding till the first multiple of 16
    	size++;
    }
    if (size < 32) {
    	size = 32;
    }
    void * bp;
    /* Initialize the heap and add all space to corresponding main class list */
    if (sf_mem_start() == sf_mem_end()){
    	/* Initialize main free list and quick list */
    	for (int i = 0; i < 10; i++) {
    		/* Initialize main free list */
    		sf_block *sp = &sf_free_list_heads[i];
    		sp->body.links.prev = sp; //Initialize head to point both ways to itself
    		sp->body.links.next = sp;

    		/* Initialize quick list */
    		sf_quick_lists[i].length = 0;
    	}

    	size_t space = 4080; //Size of block that takes up entire heap exluding unused prologue/epilogue space
    	bp = sf_mem_grow();
    	bp = bp + DSIZE; //Leave first row unused and set bp ptr to body

    	PUT(HDRP(bp), PACK(space, 0x2)); //Set size of obfuscated header
    	PUT(FTRP(bp), PACK(space, 0x2));

    	add_main_list(bp);
    	last_bp = bp;
    }

    /* TODO:Reconnect doubly linked list after allocating from main or quick list */
    /* Check quick list for free block */
    int index = find_quick_class(size);
    if (index != -1) { // When size is small enough to fit quick list
    	sf_block *sp = sf_quick_lists[index].first;
    	sp = find_quick_fit(sp, size); //We can assume sp is head of current quick class list
    	if (sp != NULL) { // When free block is found in list
    		sf_quick_lists[index].first = sp->body.links.next; //Pop out of list
    		sf_quick_lists[index].length--;

    		/* Set quick block as allocated */
    		allocate(sp, size);
    		bp = (void *)sp + DSIZE;

    		return bp; //Return usable address to caller
    	}
    }

    /* Check main list for free block */
    index = find_main_class(size);
	sf_block *sp = &sf_free_list_heads[index];
	sp = find_main_fit(sp, size);
	while (sp == NULL) {
		index++;
		if (index == 10) {
			bp = sf_mem_grow();
			if (bp == NULL) { //If we run out of total memory and heap cannot get larger
				return NULL;
			}
			size_t space = 4096;
			PUT(HDRP(bp), PACK(space, GET_ALLOC(HDRP(last_bp))>>1)); //Set block to free
			PUT(FTRP(bp), PACK(space, GET_ALLOC(HDRP(last_bp))>>1)); //Set block to free
			bp = coalesce(bp);
			add_main_list(bp);
			last_bp = bp;
			index = find_main_class(size);
		}
		/* Search next class size */
		sp = &sf_free_list_heads[index];
		sp = find_main_fit(sp, size);
	}
	/* Sp holds the ptr to struct that is large enough to hold requested size */
	/* Remove block from main list */
	remove_main_list(sp); //Remove pointer from main list
	allocate(sp, size);
	bp = (void *)sp + DSIZE;
	return bp;
}

/*Flush a list given pointer to the head of the list */
void flush_list(struct sf_block *sp){
	struct sf_block *current = sp;
	struct sf_block *next = sp->body.links.next;

	while (current != NULL) {
		next = current->body.links.next;

		size_t *bp = coalesce(&(current->body.links));
		add_main_list(bp);

		current = next;
	}
	/* Deference the head of the list */
	sp = NULL;
}

void sf_free(void *bp) {
	/* Invalid pointer cases */
	/* Null pointer*/
	if (bp == NULL) {
		abort();
	}
	/* Misaligned pointer */
	if (((size_t)bp%16) != 0) {
		abort();
	}
	/* Block size too small */
	if (GET_SIZE(HDRP(bp)) < 32) {
		abort();
	}
	/* Block size if not a multiple of 16 */
	if ((GET_SIZE(HDRP(bp))%16) != 0) {
		abort();
	}
	/* Header is before the start of heap */
	if ((char *)HDRP(bp) < (char *)sf_mem_start()){
		abort();
	}
	/* Footer is after the end of heap */
	if ((char *)FTRP(bp) > (char *)sf_mem_end()){
		abort();
	}
	/* Allocated bit in the header is 0 */
	if (GET_ALLOC(HDRP(bp)) == 0) {
		abort();
	}
	/* Previous alloc and acutal previous alloc are mismatched */
	if ((GET_PRV_ALLOC(HDRP(bp)) == 0) && (HDRP(PREV_BLKP(bp)) < (char *)sf_mem_start)){
		if ((GET_ALLOC(HDRP(PREV_BLKP(bp))) != 0)) {
			abort();
		}
	}

	size_t size = GET_SIZE(HDRP(bp));
	int index = find_quick_class(size);

	/* Previously allocated bit */
	size_t prev_alloc = GET_PRV_ALLOC(HDRP(bp));

	/* When the size of block is quick list valid, add to smallest fit quick list*/
	if (index != -1){
		/* Initialize sf_block struct */
		sf_block *sp = (bp - DSIZE); //pointer to block stucture in sfmm.h
		sp->header = (size_t)NULL;
		if (prev_alloc == 0){ //If previous block is free, set foot of current struct to footer of previous block
			sp->prev_footer = GET(FTRP(PREV_BLKP(bp)));
		}
		PUT(HDRP(bp), PACK(size, (prev_alloc | 0x4))); //Set as allocated
		PUT(FTRP(bp), PACK(size, (prev_alloc | 0x4))); //Set as allocated
		/* Set prev_alloc bit of next block to 1 if within heap */
		if ((void *)NEXT_BLKP(bp) < sf_mem_end()){
			PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), (GET_ALLOC(HDRP(NEXT_BLKP(bp))) | 0x2)));
			PUT(FTRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), (GET_ALLOC(HDRP(NEXT_BLKP(bp))) | 0x2)));
		}
		/* When the list needs to be flushed */
		if (sf_quick_lists[index].length == QUICK_LIST_MAX){
			sf_quick_lists[index].length = 1;
			sp->body.links.prev = NULL;
			sp->body.links.next = NULL;
			flush_list(sf_quick_lists[index].first);
			sf_quick_lists[index].first = sp;
		}
		else if (sf_quick_lists[index].length == 0) {
			sf_quick_lists[index].length = 1;
			sp->body.links.prev = NULL;
			sp->body.links.next = NULL;
			sf_quick_lists[index].first = sp;
		}
		else {
			sf_quick_lists[index].length++;
			sp->body.links.prev = NULL;
			sp->body.links.next = sf_quick_lists[index].first;
			sf_quick_lists[index].first = sp;
		}
	}
	/* When the block doesn't fit into a quick list, add to main free list*/
	else {
		PUT(HDRP(bp), PACK(size, (prev_alloc))); //Set as free
		PUT(FTRP(bp), PACK(size, (prev_alloc))); //Set as free
		/* Set prev_alloc bit of next block to 0 if within heap */
		if ((void *)NEXT_BLKP(bp) < sf_mem_end()){
			PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), (GET_ALLOC(HDRP(NEXT_BLKP(bp))))));
			PUT(FTRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), (GET_ALLOC(HDRP(NEXT_BLKP(bp))))));
		}
		bp = coalesce(bp);
		if ((void *)NEXT_BLKP(bp) < sf_mem_end()){
			PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), (GET_ALLOC(HDRP(NEXT_BLKP(bp))))));
		}
		add_main_list(bp);
	}
}

void *sf_realloc(void *pp, size_t rsize) {
	/* Invalid pointer cases */
	/* Null pointer*/
	if (pp == NULL) {
		abort();
	}
	/* Misaligned pointer */
	if (((size_t)pp%16) != 0) {
		abort();
	}
	/* Block size too small */
	if (GET_SIZE(HDRP(pp)) < 32) {
		abort();
	}
	/* Block size if not a multiple of 16 */
	if ((GET_SIZE(HDRP(pp))%16) != 0) {
		abort();
	}
	/* Header is before the start of heap */
	if ((char *)HDRP(pp) < (char *)sf_mem_start()){
		abort();
	}
	/* Footer is after the end of heap */
	if ((char *)FTRP(pp) > (char *)sf_mem_end()){
		abort();
	}
	/* Allocated bit in the header is 0 */
	if (GET_ALLOC(HDRP(pp)) == 0) {
		abort();
	}
	/* Previous alloc and acutal previous alloc are mismatched */
	if ((GET_PRV_ALLOC(HDRP(pp)) == 0) && (HDRP(PREV_BLKP(pp)) < (char *)sf_mem_start)){
		if ((GET_ALLOC(HDRP(PREV_BLKP(pp))) != 0)) {
			abort();
		}
	}
	if (rsize == 0){
		sf_free(pp);
		return NULL;
	}
	size_t size = GET_SIZE(HDRP(pp)); //Size of current block

	/* Set new size including overhead and alignment reqs */
    size_t ssize = rsize + 8; //Add header overhead
    while (ssize % 16 != 0) { //Add padding till the first multiple of 16
    	ssize++;
    }
    if (ssize < 32) {
    	ssize = 32;
    }

	/* If block is same size as resizing block */
	if (size == ssize) {
		return pp;
	}

	/* TODO:Resize to larger memory */
	if (ssize > size) {
		void *lp = sf_malloc(rsize); //lp = larger pointer
		if (lp == NULL) { //sf_errno is already set by sf_malloc
			return NULL;
		}
		lp = memcpy(lp, pp, rsize);
		sf_free(pp);
		return lp;
	}


	/* TODO:Resize to smaller memroy */
	if (ssize < size){
		/* Set new size including overhead and alignment reqs */
	    rsize = rsize + 8; //Add header overhead
	    while (rsize % 16 != 0) { //Add padding till the first multiple of 16
	    	rsize++;
	    }
	    if (rsize < 32) {
	    	rsize = 32;
	    }

		sf_block *sp = (void *)pp - DSIZE;
		allocate(sp, rsize);
		return pp;
	}


    return NULL;
}
