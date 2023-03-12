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
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) // NOTE: can remove this

/* Basic constants and macros */
#define ALIGNMENT 8 // single word (4) or double word (8)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // = 8 on 64 b computer // NOTE: can remove this
#define WSIZE 4
#define DSIZE 8
#define MIN_BLOCK_SIZE 24
#define CHUNKSIZE (1<<12)

/** Macro interface */
// Given a block pointer bp, get the value of its 'prev' pointer.
#define PREV(bp) (*(void**) bp)
// Given a block pointer bp, get the value of its 'next' pointer.
#define NEXT(bp) (*(void**) ((char*) bp + sizeof(void*)))
// Given a block pointer bp, set the value its 'prev' pointer.
#define SET_PREV(bp, ptr) (*(void**) (bp) = (void*) (ptr))
// Given a block pointer bp, set the value of its 'next' pointer.
#define SET_NEXT(bp, ptr) (*(void**) ((char *) bp + sizeof(void*)) = (void*) (ptr))
// Return a word (4 B) at address p.
#define GET(p) (*(unsigned int *) (p))
// Pack the given size and alloc bit, then place at addr p as a 4B unsigned int.
#define PUT(p, size, alloc) (*(unsigned int *) (p) = ((size) | (alloc)))
// Return the header address of the block, pointed to by bp. // TODO: can maybe change this so there isn't a dependancy with HDRP
#define HDRP(bp) ((char *) (bp) - WSIZE)
// Return the footer address of the block bp, pointed to by bp.
#define FTRP(bp) ((char *) (bp) + GET_SIZE(bp) - DSIZE)
// Get the size of the block bp, pointed to by bp.
#define GET_SIZE(bp) (GET(HDRP(bp)) & ~0x7)
// Get the alloc bit of the block bp, pointed to by bp.
#define GET_ALLOC(bp) (GET(HDRP(bp)) & 0x1)
// Get address of the next adjacent block, pointed to by bp.
#define NEXT_BLKP(bp) ((char *) (bp) + GET_SIZE(bp)) 
// Get address of the previous adjacent block, pointed to by bp.
#define PREV_BLKP(bp) ((char *) (bp) - (GET((char *) (bp) - DSIZE) & ~0x7))

/* Heapchecker - comment/uncomment to disable and enable */
// #define checkheap(lineno) printf("%s: ", __func__); (mm_check(lineno))
#define checkheap(lineno)

static void* sentinel; // pointer to beginning of linked list of free blocks
static void* coalesce(void* bp);
static void place(void *bp, size_t allocSize);
static void* extend_heap(size_t words);
void mm_check(int lineno);
inline static void insert_block(void* bp);
inline static void remove_block(void* bp);

/* Initialize the malloc package. Places prologue header & footer and sentinel 
 * node, then extends the heap by CHUNKSIZE and places epilogue. This creates an
 * initial empty list of size 4096 B.
 * 
 * Returns 0 if sucessful, -1 if error. 
 */
int mm_init(void) {
    char* heap_ptr = mem_sbrk(10*WSIZE);
    if ((long) heap_ptr == -1) {
        return -1;
    }
    /* Initializes linked list sentinel node */
    PUT(heap_ptr, 0, 0);                    // Alignment padding
    sentinel = heap_ptr + 2*WSIZE;
    PUT(HDRP(sentinel), MIN_BLOCK_SIZE, 1); // @ heap_ptr + 1*WSIZE
    PUT(FTRP(sentinel), MIN_BLOCK_SIZE, 1); // @ heap_ptr + 6*WSIZE
    SET_PREV(sentinel, sentinel);
    SET_NEXT(sentinel, sentinel);

    PUT(heap_ptr + 7*WSIZE, DSIZE, 1); // Prologue header
    PUT(heap_ptr + 8*WSIZE, DSIZE, 1); // Prologue footer

    /* Creates an empty link of size 4096 B,
    then inserts this into the linked list */
    char* bp = mem_sbrk(CHUNKSIZE);
    if ((long) bp == -1) {
        return -1;
    }
    PUT(HDRP(bp), CHUNKSIZE, 0);    // @ heap_ptr + 9*WSIZE
    PUT(FTRP(bp), CHUNKSIZE, 0);
    PUT(HDRP(NEXT_BLKP(bp)), 0, 1); // place epilogue
    insert_block(bp);   

    checkheap(__LINE__);
    return 0;
}

/* Extend the heap by "words" number of words. Newly allocated heap memory is
 * effectively appended to the end of the current heap. Epilogue block is placed
 * at end of newly allocated memory.
 *
 * Returns a generic pointer to the newly allocated block if successful.
 * Returns NULL on error. 
 */
static void* extend_heap(size_t words) {
    /* Extend by an even num of words (8 B) to maintain double word alignment.
    Then get a pointer to the first byte of the new heap area. */
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    char* bp = mem_sbrk(size);
    if ((long) bp == -1) {
        return NULL;
    }
    /* Place header, footer, and epilogue of the new block */
    PUT(HDRP(bp), size, 0); // old epilogue becomes the new block header
    PUT(FTRP(bp), size, 0);
    PUT(HDRP(NEXT_BLKP(bp)), 0, 1); // placing new epilogue block

    return coalesce(bp); // if prev adjacent block is free, will coalesce
}

/* Given a block pointer bp to a free block, coalesce current block with the
 * previous and next blocks if possible. 
 * 
 * If bp is free, returns a pointer to the start of the newly coalesced block.
 * If bp is not free, behavior is undefined.
 */
static void* coalesce(void* bp) {
    size_t prevIsFree = !GET_ALLOC(PREV_BLKP(bp));
    size_t nextIsFree = !GET_ALLOC(NEXT_BLKP(bp));
    size_t size = GET_SIZE(bp);
    /*
    case 1: prev and next blocks are allocated
    case 2: next block is free
    case 3: prev block is free
    case 4: both next and prev blocks are free
    */
    if (!prevIsFree && !nextIsFree) {
        insert_block(bp);

    } else if (!prevIsFree && nextIsFree) {
        // Splice out successor block
        void* bp_next = NEXT_BLKP(bp);
        remove_block(bp_next);

        // Coalesce & insert to linked list
        // bp stays the same because only next is free
        size += GET_SIZE(NEXT_BLKP(bp));
        PUT(HDRP(bp), size, 0);
        PUT(FTRP(bp), size, 0);
        insert_block(bp);

    } else if (prevIsFree && !nextIsFree) {
        // Splice out predecessor block
        void* bp_prev = PREV_BLKP(bp);
        remove_block(bp_prev);

        // Coalesce & insert to linked list
        size += GET_SIZE(PREV_BLKP(bp));
        PUT(FTRP(bp), size, 0);      // reset curr footer
        PUT(HDRP(bp_prev), size, 0); // reset prev header
        bp = PREV_BLKP(bp);
        insert_block(bp);

    } else {
        // Splice out successor block
        void* bp_next = NEXT_BLKP(bp);
        remove_block(bp_next);

        // Splice out predecessor block
        void* bp_prev = PREV_BLKP(bp);
        remove_block(bp_prev);
        
        // coalesce both memory blocks
        size += GET_SIZE(PREV_BLKP(bp)) + GET_SIZE(NEXT_BLKP(bp));
        PUT(HDRP(bp_prev), size, 0); // reset prev header
        PUT(FTRP(bp_next), size, 0); // reset next footer
        bp = PREV_BLKP(bp);
        insert_block(bp);
    }
    return bp;
}

/* Given a block pointer bp, remove this block from the linked list */
inline static void remove_block(void* bp) {
    void* bp_prev = PREV(bp);
    void* bp_next = NEXT(bp);
    SET_NEXT(bp_prev, bp_next);
    SET_PREV(bp_next, bp_prev);
    checkheap(__LINE__);
}

/* Append block at the front of the linked list (in front of sentinel) */
inline static void insert_block(void* bp) {
    SET_PREV(bp, sentinel);         // bp.prev = sentinel
    SET_NEXT(bp, NEXT(sentinel));   // bp.next = sentinel.next
    void* tmp = NEXT(sentinel);     // tmp = sentinel.next
    SET_PREV(tmp, bp);              // tmp.prev = bp
    SET_NEXT(sentinel, bp);         // sentinel.next = bp
    checkheap(__LINE__);
}

/* Allocate a block whose size is a multiple of the alignment.
 *
 * If successful, returns a pointer to the newly allocated block.
 * If error, such as no more heap memory to extend, returns NULL.
 * if payloadSize is negative, behavior is undefined.
 */
void* mm_malloc(size_t payloadSize) {
    size_t adjustedSize;

    /* Adjust block size to include overhead and alignment reqs. 
    The next & prev pointer and a header & footer take up 24 B-- thus, this is
    the minimum block size. The payload area will be 24 - 4 - 4 = 16 B.

    If <= 16 B is requested, data will fit in the minimum block size of 24. 
    Else, round up to the nearest multiple of 8 and add 8 for the header & footer.
    For example, 17 becomes 24, then 32. */
    if (payloadSize == 0) {
        return NULL;
    } else if (payloadSize <= 2*DSIZE) {
        adjustedSize = MIN_BLOCK_SIZE;
    } else {
        adjustedSize = DSIZE * ((payloadSize + DSIZE + (DSIZE-1)) / DSIZE);
    }
    /* Traverse the linked list until a fit is found. Start at sentinel.next.
    If sentinel.next = sentinel, loop immediately terminates. If sentinel.next
    != sentinel, but no fit found, bp eventually equals sentinel. */
    void* bp = NEXT(sentinel);
    while (bp != sentinel) {
        if (adjustedSize <= GET_SIZE(bp)) {
            place(bp, adjustedSize);
            checkheap(__LINE__);
            return bp;
        }
        bp = NEXT(bp);
    }
    /* No fit found. Get more memory and place the block.
    On extend_heap error, bp = NULL */
    size_t extendSize = (adjustedSize > CHUNKSIZE) ? adjustedSize : CHUNKSIZE;
    bp = extend_heap(extendSize / WSIZE);
    if (bp == NULL) {
        return NULL;
    }
    place(bp, adjustedSize);
    checkheap(__LINE__);
    return bp;
}

/* Place the requested allocated block within the block, splits the excess,
 * and sets bp to the address of the newly allocated block.
 *
 * Returns the same passed-in bp pointer.
 */
static void place(void* bp, size_t allocSize) {
    // Get the size of the current block
    size_t currSize = GET_SIZE(bp);

    // If remainder block size >= 24, split it and append it to list
    size_t remainder = currSize - allocSize;
    if (remainder >= MIN_BLOCK_SIZE) {
        // remove current block from linked list
        remove_block(bp);

        // Update header and footer of requested block.
        // Note: FTRP depends on size value within header
        PUT(HDRP(bp), allocSize, 1);
        PUT(FTRP(bp), allocSize, 1);

        // Set header and footer of next block, then insert into linked list
        void* bp_next = NEXT_BLKP(bp);
        PUT(HDRP(bp_next), remainder, 0);
        PUT(FTRP(bp_next), remainder, 0);
        insert_block(bp_next);

    } else { // Remainder block too small. Remainder block becomes fragmentation
        PUT(HDRP(bp), currSize, 1);
        PUT(FTRP(bp), currSize, 1);
        remove_block(bp);
    }
}

/* mm_free - free current block, and coalesce prev and next if possible. 
 * Assume bp points to the start of a block.
 */
void mm_free(void* bp) {
    size_t size = GET_SIZE(bp);
    PUT(HDRP(bp), size, 0); // reset header bit
    PUT(FTRP(bp), size, 0); // reset footer bit
    coalesce(bp);
    checkheap(__LINE__);
}

/* mm_realloc - realloc payload data that ptr points to to a new payload of newSize.
 * If ptr = NULL, equivalent to mm_malloc(size).
 * If size is equal to zero, equivalent to mm_free(ptr).
 * If ptr != NULL, changes the size of the payload block pointed to by ptr to
 * newSize bytes and returns the address of the new block.
 *
 * If successful, returns a pointer to the newly allocated block.
 * If error, returns NULL.
 */
void* mm_realloc(void* ptr, size_t newSize) {
    checkheap(__LINE__);
    void* new_ptr;
    
    if (ptr == NULL) {
        return mm_malloc(newSize);

    } else if (newSize <= 0) {
        mm_free(ptr);
        return NULL;

    } else { // ptr is not null and size != 0
        size_t currSize = GET_SIZE(ptr);
        if (newSize == currSize) {
            return ptr;
        } else {
            // Search for new free block, copy the payload, free old block
            new_ptr = mm_malloc(newSize);
            if (newSize < currSize) {
                memcpy(new_ptr, ptr, newSize);
            } else {
                memcpy(new_ptr, ptr, currSize);
            }
            mm_free(ptr);
            return new_ptr;
        }
    }
}

/* Checks the heap for correctness. Call this function using checkheap(__LINE__) */
void mm_check(int lineno) {
    printf("called from %d. ", lineno);

    // do headers and footers match?
    // Is payload aligned?
    // are there any contiguous free blocks that somehow escaped coalescing?
    // Is every free block actually in the free list?
    void* bp = NEXT(sentinel);
    while (bp != sentinel) {
        if (GET_ALLOC(bp)) {
            printf("ERROR: Not all blocks in linked list are free\n");
            exit(-1);
        }
        bp = NEXT(bp);
    }
}