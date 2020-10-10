/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef SFMM_H
#define SFMM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*

                                 Format of an allocated memory block
    +-----------------------------------------------------------------------------------------+
    |                                    64-bit-wide row                                      |
    +-----------------------------------------------------------------------------------------+

    +------------------------------------------------------------+--------+---------+---------+ <- header
    |                                       block_size           | alloc  |prv alloc|         |
    |                                  (3 LSB's implicitly 0)    |  (1)   |  (0/1)  |    0    |
    |                                        (1 row)             | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Payload and Padding                                   |
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +-----------------------------------------------------------------------------------------+

    NOTE: For an allocated block, there is no footer (it is used for payload).
    NOTE: The actual stored header is obfuscated by bitwise XOR'ing with MAGIC.
          The above diagram shows the un-obfuscated contents.
*/

/*
                                     Format of a free memory block
    +------------------------------------------------------------+--------+---------+---------+ <- header
    |                                       block_size           | alloc  |prv alloc|         |
    |                                  (3 LSB's implicitly 0)    |  (0)   |  (0/1)  |    0    |
    |                                        (1 row)             | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                Pointer to next free block                               |
    |                                        (1 row)                                          |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         |
    |                               Pointer to previous free block                            |
    |                                        (1 row)                                          |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         | 
    |                                         Unused                                          | 
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +------------------------------------------------------------+--------+---------+---------+ <- footer
    |                                       block_size           | alloc  |prv alloc|         |
    |                                  (3 LSB's implicitly 0)    |  (0)   |  (0/1)  |    0    |
    |                                        (1 row)             | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+

    NOTE: For a free block, footer contents must always be identical to header contents.
    NOTE: The actual stored footer is obfuscated by bitwise XOR'ing with MAGIC.
          The above diagram shows the un-obfuscated contents.
*/

/* Define WEAK_MAGIC during compilation to use MAGIC of 0x0 for debugging purposes. */
#ifndef WEAK_MAGIC
#define MAGIC (sf_magic())
#else
#define MAGIC ((uint64_t)0x0)
#endif

#define THIS_BLOCK_ALLOCATED  0x4
#define PREV_BLOCK_ALLOCATED  0x2

typedef size_t sf_header;
typedef size_t sf_footer;

/*
 * Structure of a block.
 * The first field of this structure is actually the footer of the *previous* block.
 * This must be taken into account when creating sf_block pointers from memory addresses.
 */
typedef struct sf_block {
    sf_footer prev_footer;  // NOTE: This actually belongs to the *previous* block.
    sf_header header;       // This is where the current block really starts.
    union {
        /* A free block contains links to other blocks in a free list. */
        struct {
            struct sf_block *next;
            struct sf_block *prev;
        } links;
        /* An allocated block contains a payload (aligned), starting here. */
        char payload[0];   // Length varies according to block size.
    } body;
} sf_block;

/*
 * The heap is designed to keep the payload area of each block aligned to a two-row (16-byte)
 * boundary.  The header of a block precedes the payload area, and is only single-row (8-byte)
 * aligned.  The first block of the heap starts as soon as possible after the beginning of the
 * heap, subject to the condition that its payload area is two-row aligned.
 * We are not using any "prologue" or "epilogue" in this assignment.
 */

/*
                                         Format of the heap
    +-----------------------------------------------------------------------------------------+
    |                                    64-bit-wide row                                      |
    +-----------------------------------------------------------------------------------------+

    +-----------------------------------------------------------------------------------------+ <- heap start
    |                                                                                         |    (aligned)
    |                                        Unused                                           |
    |                                        (1 row)                                          |
    +------------------------------------------------------------+--------+---------+---------+ <- header
    |                                       block_size           | alloc  |prv alloc|         |
    |                                  (3 LSB's implicitly 0)    |  (0/1) |  (0/1)  |    0    | first block
    |                                        (1 row)             | 1 bit  |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Payload and Padding                                   |
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +--------------------------------------------+------------------------+---------+---------+
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    |                             Additional allocated and free blocks                        |
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         |
    |                       Unused (will become header when heap grows)                       |
    |                                        (1 row)                                          |
    +-----------------------------------------------------------------------------------------+ <- heap end
                                                                                                   (aligned)
*/

/* sf_errno: will be set on error */
int sf_errno;

/*
 * "Quick lists":  These are used to hold recently freed blocks of small sizes, so that they
 * can be used to satisfy allocations without searching lists or splitting blocks.
 * Blocks on a quick list are marked as allocated, so they are not available for coalescing.
 * The number of blocks in any individual quick list is limited to QUICK_LIST_MAX.
 * If adding a block to a quick list would cause it to exceed QUICK_LIST_MAX, then the
 * list is flushed, returning the existing blocks in the list to the main pool, and then
 * the block being freed is added to the now-empty list, leaving that list containing one block.
 *
 * The quick lists are indexed by (size-32)/16, starting with blocks of the minimum block size 32
 * at index 0, blocks of size 48 at index 1, and so on.  They are maintained as singly linked lists,
 * using a LIFO discipline.
 */

#define NUM_QUICK_LISTS 10  /* Number of quick lists. */
#define QUICK_LIST_MAX   5  /* Maximum number of blocks permitted on a single quick list. */

struct {
    int length;             // Number of blocks currently in the list.
    struct sf_block *first; // Pointer to first block in the list.
} sf_quick_lists[NUM_QUICK_LISTS];

/*
 * Free blocks are maintained in a set of circular, doubly linked lists, segregated by
 * size class.  The first list holds blocks of the minimum size M.  The second list holds
 * blocks of size (M, 2M].  The third list holds blocks whose size is in the interval (2M, 4M].
 * The fourth list holds blocks whose size is in the interval (4M, 8M], and so on.
 * This continues up to the interval (128M, 512M], and then the last list holds all blocks
 * of size greater than 512M.
 *
 * Each of the circular, doubly linked lists has a "dummy" block used as the list header.
 * This dummy block is always linked between the last and the first element of the list.
 * In an empty list, the next and free pointers of the list header point back to itself.
 * In a list with something in it, the next pointer of the header points to the first node
 * in the list and the previous pointer of the header points to the last node in the list.
 * The header itself is never removed from the list and it contains no data (only the link
 * fields are used).  The reason for doing things this way is to avoid edge cases in insertion
 * and deletion of nodes from the list.
 */

#define NUM_FREE_LISTS 10
struct sf_block sf_free_list_heads[NUM_FREE_LISTS];

/*
 * This is your implementation of sf_malloc. It acquires uninitialized memory that
 * is aligned and padded properly for the underlying system.
 *
 * @param size The number of bytes requested to be allocated.
 *
 * @return If size is 0, then NULL is returned without setting sf_errno.
 * If size is nonzero, then if the allocation is successful a pointer to a valid region of
 * memory of the requested size is returned.  If the allocation is not successful, then
 * NULL is returned and sf_errno is set to ENOMEM.
 */
void *sf_malloc(size_t size);

/*
 * Resizes the memory pointed to by ptr to size bytes.
 *
 * @param ptr Address of the memory region to resize.
 * @param size The minimum size to resize the memory to.
 *
 * @return If successful, the pointer to a valid region of memory is
 * returned, else NULL is returned and sf_errno is set appropriately.
 *
 *   If sf_realloc is called with an invalid pointer sf_errno should be set to EINVAL.
 *   If there is no memory available sf_realloc should set sf_errno to ENOMEM.
 *
 * If sf_realloc is called with a valid pointer and a size of 0 it should free
 * the allocated block and return NULL without setting sf_errno.
 */
void *sf_realloc(void *ptr, size_t size);

/*
 * Marks a dynamically allocated region as no longer in use.
 * Adds the newly freed block to the free list.
 *
 * @param ptr Address of memory returned by the function sf_malloc.
 *
 * If ptr is invalid, the function calls abort() to exit the program.
 */
void sf_free(void *ptr);

/* sfutil.c: Helper functions already created for this assignment. */

/*
 * @return The starting address of the heap for your allocator.
 */
void *sf_mem_start();

/*
 * @return The ending address of the heap for your allocator.
 */
void *sf_mem_end();

/*
 * This function increases the size of your heap by adding one page of
 * memory to the end.
 *
 * @return On success, this function returns a pointer to the start of the
 * additional page, which is the same as the value that would have been returned
 * by get_heap_end() before the size increase.  On error, NULL is returned
 * and sf_errno is set to ENOMEM.
 */
void *sf_mem_grow();

/* The size of a page of memory returned by sf_mem_grow(). */
#define PAGE_SZ ((size_t)4096)

/*
 * @return The "magic number" used to obfuscate header and footer contents
 * to make it difficult to free a block without having first succesfully
 * malloc'ed one.
 */
uint64_t sf_magic();

/*
 * Display the contents of the heap in a human-readable form.
 */
void sf_show_block(sf_block *bp);
void sf_show_blocks();
void sf_show_free_list(int index);
void sf_show_free_lists();
void sf_show_quick_list(int index);
void sf_show_quick_lists();
void sf_show_heap();

#endif
