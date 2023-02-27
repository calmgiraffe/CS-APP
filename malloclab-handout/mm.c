#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Hans Park",
    /* First member's email address */
    "hans.park6@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* Rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* Basic constants and macros */
#define ALIGNMENT 8 // single word (4) or double word (8)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) /* Pack a size and allocated bit into a word */
#define GET(p) (*(unsigned int *) (p)) /* Read a word (4 B) at address p */
#define PUT(p, val) (*(unsigned int *) (p) = (val)) /* Write a word (4 B) at address p */
#define GET_SIZE(p) (GET(p) & ~0x7) /* Read the size fields from address p. */
#define GET_ALLOC(p) (GET(p) & 0x1) /* Read the allocated bit from address p. */

/* Given block ptr bp, compute address of its header.
 * Assume bp points to the beginning of the payload. */
#define HDRP(bp) ((char *) (bp) - WSIZE)

/* Given block ptr bp, compute address of its footer.
 * Assume bp points to the beginning of the payload. */
#define FTRP(bp) ((char *) (bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// bp - WISE = curr block's header
// bp - DWISE = prev block's footer
/* Given block ptr bp, compute address of the next block */
#define NEXT_BLKP(bp) ((char *) (bp) + GET_SIZE(((char *) (bp) - WSIZE)))

/* Given block ptr bp, compute address of the previous block */
#define PREV_BLKP(bp) ((char *) (bp) - GET_SIZE(((char *) (bp) - DSIZE))) 

/* Pointer to first byte of heap */
static void* heap_listp;
static void *coalesce(void* bp);
static void* find_fit(size_t asize);
static void* place(void *bp, size_t asize);
static void* extend_heap(size_t words);


/* Initialize the malloc package.
 * Returns 0 if sucessful, -1 if error. 
 */
int mm_init(void)
{   
    heap_listp = mem_sbrk(4*WSIZE);
    if ((long) heap_listp == -1) {
        return -1;
    }
    PUT(heap_listp, 0);                          // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/* Extend the heap by "words" number of words. 
 * Returns a generic pointer to the newly allocated block if successful.
 * Returns NULL on error. 
 */
static void* extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment.
    Expand the heap by 'size' B, setting bp to the first B of the newly allocated heap area */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    bp = mem_sbrk(size);
    if ((long) bp == -1) {
        return NULL;
    }
    /* Initialize free block header/footer and the epilogue header
    put block size + 0 bit at header address, which is 4 bytes before bp
    put block size + 0 bit at footer address, which is 8 bytes before mem_brk
    note: the old epilogue header becomes the new block header
    put new epilogue header 4 bytes before mem_brk */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

/* Given a block pointer bp pointing to a free block, coalesce the prev and next
 * blocks if they are also free. Returns a pointer to start of the coalesced block.
 */
static void* coalesce(void* bp) {

    size_t prevIsAlloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t nextIsAlloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /*
    case 1: prev and next blocks are allocated
    case 2: next block is free
    case 3: prev block is free
    case 4: both next and prev blocks are free
    */
    if (prevIsAlloc && nextIsAlloc) {
        return bp;

    } else if (prevIsAlloc && !nextIsAlloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    } else if (!prevIsAlloc && nextIsAlloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));            // reset curr footer
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // reset prev header
        bp = PREV_BLKP(bp);

    } else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // reset prev header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // reset next footer
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/* Allocate a block whose size is a multiple of the alignment.
 * Returns a pointer to the newly allocated block if successful, NULL on error.
 */
void* mm_malloc(size_t size) {
    size_t allocSize; // adjusted block size
    char *bp;

    /* Adjust block size to include overhead and alignment reqs
    i.e., enforce the minimum block size of 16 blocks and 8 B alignment */
    if (size == 0) {
        return NULL;
    } else if (size < DSIZE) {
        allocSize = 2*DSIZE;
    } else {
        allocSize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }
    /* Search the free list for a fit and places requested allocated block. */
    bp = find_fit(allocSize);
    if (bp != NULL) {
        place(bp, allocSize);
    }
    return bp;
}

/* Searches the free list for a suitable free block. If no fit, gets more memory
 * and places the block. If sucessful, returns a pointer to the beginning of 
 * the block. Returns NULL on error.
 */
// NOTE can probably refactor this into malloc()
static void* find_fit(size_t asize) {
    void* bp = heap_listp;

    // First fit search: iterate through blocks until valid block found
    while (GET_SIZE(HDRP(bp)) > 0) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }    
    // No fit found. Get more memory and place the block. On error, bp = NULL
    size_t extendsize = MAX(asize, CHUNKSIZE);
    bp = extend_heap(extendsize/WSIZE);
    return bp;
}

/* Place the requested allocated block within the block, splits the excess,
 * and sets bp to the address of the newly allocated block. 
 */
static void* place(void* bp, size_t asize) {
    // Get the size of the current block
    size_t csize = GET_SIZE(HDRP(bp));

    // If remainder block size >= 16 
    if ((csize - asize) >= (2*DSIZE)) {
        // Update header and footer of requested block.
        // Note: FTRP depends on size value within header
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // Set the header and footer of next block
        void* bp_next = NEXT_BLKP(bp);
        PUT(HDRP(bp_next), PACK(csize - asize, 0));
        PUT(FTRP(bp_next), PACK(csize - asize, 0));

    } else { 
        // Remainder block is too small. Update current block's header
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    return bp;
}

/* mm_free - free current block, and coalesce prev and next if possible. 
 * Assume bp points to the start of a block.
 */
void mm_free(void* bp) {
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0)); // reset header bit
    PUT(FTRP(bp), PACK(size, 0)); // reset footer bit
    coalesce(bp);
}

/* mm_realloc - Implemented simply in terms of mm_malloc and mm_free.
 * Returns a void pointer to the newly allocated block.
 * If ptr = NULL, equivalent to mm_malloc(size).
 * If size is equal to zero, equivalent to mm_free(ptr).
 * If ptr != NULL, changes the size of the memory block pointed to by ptr
 * to size bytes and returns the address of the new block.
 */
void* mm_realloc(void* ptr, size_t newSize) {
    void* newptr;
    
    if (ptr == NULL) {
        return mm_malloc(newSize);

    } else if (newSize <= 0) {
        mm_free(ptr);
        return NULL;

    } else { // ptr is not null and size != 0
        size_t currSize = GET_SIZE(HDRP(ptr));
        if (newSize == currSize) {
            return ptr;

        } else {
            // Search for a another free block
            // Copy over the data
            // Free the current block
            newptr = mm_malloc(newSize);
            if (newSize < currSize) {
                memcpy(newptr, ptr, newSize);
            } else {
                memcpy(newptr, ptr, currSize);
            }
            mm_free(ptr);
            return newptr;
        }
    }
}