/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
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

/* Pack a size and allocaed bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read a word (4 B) at address p */
#define GET(p) (*(unsigned int *) (p))

/* Write a word (4 B) at address p */
#define PUT(p, val) (*(unsigned int *) (p) = (val))

/* Read the size fields from address p. Assume p is a header/footer. */
#define GET_SIZE(p) (GET(p) & ~0x7) // ~0x7 = 111...11000

/* Read the allocated bit from address p. Assume p is a header/footer. */
#define GET_ALLOC(p) (GET(p) & 0x1) //  0x1 = 000...00001

/* Given block ptr bp, compute address of its header.
 * Assume bp points to the beginning of the paylod. */
#define HDRP(bp) ((char *)(bp) - WSIZE)

/* Given block ptr bp, compute address of its footer.
 * Assume bp points to the beginning of the paylod. */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// bp - WISE = curr block's header
// bp - DWISE = prev block's footer
/* Given block ptr bp, compute address of the next block */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))

/* Given block ptr bp, compute address of the previous block */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

/* Pointer to first byte of heap */
static void* heap_listp;

static void *coalesce(void* bp);
static void* find_fit(size_t asize);
static void* place(void *bp, size_t asize);
static void* extend_heap(size_t words);


/* mm_init - initialize the malloc package.
 * Returns 0 if sucessful, -1 if error. */
int mm_init(void)
{   
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1) {
        return -1;
    }
    PUT(heap_listp, 0); // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2*WSIZE);

    /* Entend the empty heap with a free block of CHUNKSIZE bytes */
    // replace epilogue block with free block header
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/* Extend the heap by "words" number of words. 
 * Returns NULL on error */
static void* extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    // If words is odd, increase by 1 to make it even. Then, multiply by 4.
    // Expand the heap by 'size' number of bytes
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }
    /* Initialize free block header/footer and the epilogue header */
    // put block size + 0 bit at header address, which is 4 bytes before bp
    // put block size + 0 bit at footer address, which is 8 bytes before mem_brk
    // note: the old epilogue header becomes the new block header
    // put new epilogue header 4 bytes before mem_brk 
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* Given a block pointer bp pointing to a free block, 
 * coalesce the prev and next blocks if they are also free */
static void *coalesce(void* bp) {
    // Get the alloc bit of prev block's footer
    // Get the alloc bit of next block's header
    // Get the size of the current block
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { // case 1: prev and next are allocated
        return bp;

    } else if (prev_alloc && !next_alloc) { // case 2: next is free
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    } else if (!prev_alloc && next_alloc) { // case 3: prev is free
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0)); // reset curr footer
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // reset prev header
        bp = PREV_BLKP(bp);

    } else { // case 4: both next and prev are free
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // reset prev header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // reset next footer
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/* mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment. */
void *mm_malloc(size_t size) {
    size_t asize;       // adjusted block size
    size_t extendsize;  // amount to entend heap if no fit
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }
    /* Adjust block size to include overhead and alignment reqs */
    // i.e., enforce the minimum block size of 16 blocks
    if (size < DSIZE) {
        asize = 2*DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }
    /* Search the free list for a fit */
    // Iteration through heap to find a free block that fits asize
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/* Searches the free list for a sutiable free block. If there is a fit,
 * returns a pointer to the beginning of the block. If no fit, returns NULL. */
static void* find_fit(size_t asize) {
    /* First fit search */
    void* bp = heap_listp;

    while (GET_SIZE(HDRP(bp)) > 0) {
        // if not allocated and block size is sufficient, return bp
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    /*
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    */
    return NULL; // return NULL on no fit
}

/* Place the requested allocated block within the block, splits the excess,
 * and sets bp to the address of the newly allocated block. */
static void* place(void *bp, size_t asize) {
    // Get the size of the current block
    size_t csize = GET_SIZE(HDRP(bp));

    // If remainder block size > 16 
    if ((csize - asize) >= (2*DSIZE)) {
        // Set header and footer of current block
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // Set the header and footer of next block
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

    } else { 
        // Remainder block is too small. Update current block's header
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* mm_free - free current block, and coalesce prev and next if possible. 
 * Assume bp points to the start of a block. */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0)); // reset header bit
    PUT(FTRP(bp), PACK(size, 0)); // reset footer bit
    coalesce(bp);
}

/* mm_realloc - Implemented simply in terms of mm_malloc and mm_free */
void *mm_realloc(void *ptr, size_t size) {
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}