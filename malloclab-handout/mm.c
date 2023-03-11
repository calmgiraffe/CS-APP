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

/* Macro interface */
// Return a word (4 B) at address p.
#define GET(p) (*(unsigned int *) (p))
// Return the maximum of x and y.
#define MAX(x, y) ((x) > (y) ? (x) : (y))
// Pack the given size and alloc bit, then place at addr p as a 4B unsigned int.
#define PUT(p, size, alloc) (*(unsigned int *) (p) = ((size) | (alloc)))
// Given a block pointer bp, set the value of the 8 B area that points to the prev free block
#define SET_PREV_PTR(bp, ptr) (*(void**) (bp) = (void*) (ptr))
// Given a block pointer bp, set the value of the 8 B area that points to the next free block
#define SET_NEXT_PTR(bp, ptr) (*(void**) ((char *) bp + sizeof(void*)) = (void*) (ptr))
// Return the header address of the block, pointed to by bp.
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
#define checkheap(lineno) printf("%s: ", __func__); (mm_check(lineno))
// #define checkheap(lineno)

static void* root; // pointer to beginning of linked list of free blocks
static void* coalesce(void* bp);
static void* place(void *bp, size_t asize);
static void* extend_heap(size_t words);
void mm_check(int lineno);
inline static void insert_block(void* bp);
inline static void remove_block(void* bp);

/* Initialize the malloc package. Places prologue and eplilogue headers, then
 * extends the heap by CHUNKSIZE. Points heap_listp to the empty payload of the
 * prologue, i.e., the prologue footer.
 * 
 * Returns 0 if sucessful, -1 if error. 
 */
int mm_init(void) { // [x] verified correctness. Separate calls for each trace
    checkheap(__LINE__);
    char* heap_listp = mem_sbrk(4*WSIZE);
    if ((long) heap_listp == -1) {
        return -1;
    }
    PUT(heap_listp, 0, 0);               // Alignment padding
    PUT(heap_listp + 1*WSIZE, DSIZE, 1); // Prologue header
    PUT(heap_listp + 2*WSIZE, DSIZE, 1); // Prologue footer
    PUT(heap_listp + 3*WSIZE, 0, 1);     // Epilogue header

    /* Extend the heap by CHUNKSIZE, get pointer to first byte of new block.
    Within the new block, place header, footer, and the epilogue. */
    char* bp = mem_sbrk(CHUNKSIZE);
    if ((long) bp == -1) {
        return -1;
    }
    PUT(HDRP(bp), CHUNKSIZE, 0); // old epilogue becomes the new block header
    PUT(FTRP(bp), CHUNKSIZE, 0);
    PUT(HDRP(NEXT_BLKP(bp)), 0, 1); // place new epilogue

    /* Set initial empty link */
    root = bp;
    SET_PREV_PTR(root, NULL);
    SET_NEXT_PTR(root, NULL);

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
    Then get a pointer to the first B of the new heap area. */
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    char* bp = mem_sbrk(size);
    if ((long) bp == -1) {
        return NULL;
    }
    /* Within the new block, place header, footer, and the epilogue header. */
    PUT(HDRP(bp), size, 0); // old epilogue block becomes the new block header
    PUT(FTRP(bp), size, 0);
    PUT(HDRP(NEXT_BLKP(bp)), 0, 1); // placing new epilogue block
    return coalesce(bp);
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

        // coalesce blocks; bp stays the same because only next is free
        size += GET_SIZE(NEXT_BLKP(bp));
        PUT(HDRP(bp), size, 0);
        PUT(FTRP(bp), size, 0);
        insert_block(bp);

    } else if (prevIsFree && !nextIsFree) {
        // Splice out predecessor block
        void* bp_prev = PREV_BLKP(bp);
        remove_block(bp_prev);

        // Coalesce blocks
        size += GET_SIZE(PREV_BLKP(bp));
        PUT(FTRP(bp), size, 0);            // reset curr footer
        PUT(HDRP(PREV_BLKP(bp)), size, 0); // reset prev header
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
        PUT(HDRP(PREV_BLKP(bp)), size, 0); // reset prev header
        PUT(FTRP(NEXT_BLKP(bp)), size, 0); // reset next footer
        bp = PREV_BLKP(bp);
        insert_block(bp);
    }
    return bp;
}

// Given a block pointer bp, remove this block from the linked list
inline static void remove_block(void* bp) {
    void* bp_prev = *(void**) bp;
    void* bp_next = *(void**) ((char*) bp + sizeof(void*));

    if (bp_prev == NULL && bp_next == NULL) {
        // only possible if root points to single link, where prev and next are null
        root = NULL;

    } else if (bp_prev == NULL) { // at beginning of list
        root = bp_next;
        SET_PREV_PTR(bp_next, NULL);

    } else if (bp_next == NULL) { // at end of list
        SET_NEXT_PTR(bp_prev, NULL);
    
    } else {
        SET_NEXT_PTR(bp_prev, bp_next);
        SET_PREV_PTR(bp_next, bp_prev);
    }
    SET_NEXT_PTR(bp, NULL);
    SET_PREV_PTR(bp, NULL);
    checkheap(__LINE__);
}

// Insert freed block at the front of the list sequence
inline static void insert_block(void* bp) {
    SET_PREV_PTR(bp, NULL); // bp.prev = null
    SET_NEXT_PTR(bp, root); // bp.next = head
    SET_PREV_PTR(root, bp); // head.prev = bp
    root = bp;              // root = bp
    checkheap(__LINE__);
}

/* Allocate a block whose size is a multiple of the alignment.
 *
 * If successful, returns a pointer to the newly allocated block.
 * If error, such as no more heap memory to extend, returns NULL.
 */
void* mm_malloc(size_t payloadSize) {
    checkheap(__LINE__);
    size_t adjustedSize; // adjusted block size

    /* Adjust block size to include overhead and alignment reqs. 
    A next & prev pointer, a header, and a footer take up 24 B-- thus, this is
    the minimum block size. The payload area will be 16 B.

    If <= 16 B is requested, data will fit in the minimum block size of 24. 
    Else, round up to the nearest multiple of 8 and add 8.
    For example, 17 becomes 24, then 32. */
    if (payloadSize == 0) {
        return NULL;
    } else if (payloadSize <= 2*DSIZE) {
        adjustedSize = 3*DSIZE;
    } else {
        adjustedSize = DSIZE * ((payloadSize + DSIZE + (DSIZE-1)) / DSIZE);
    }
    /* Traverse the linked list(s) until a fit is found */
    void* bp = root;
    while (bp != NULL) {
        if (adjustedSize <= GET_SIZE(bp)) {
            break;
        }
        bp = *(void**) ((char*) bp + sizeof(void*));
    }
    if (bp == NULL) {  
        // No fit found. Get more memory and place the block
        // On extend_heap error, bp = NULL
        size_t extendSize = MAX(adjustedSize, CHUNKSIZE);
        bp = extend_heap(extendSize/WSIZE);
        if (bp == NULL) return NULL;
    }
    place(bp, adjustedSize);
    return bp;
}

/* Place the requested allocated block within the block, splits the excess,
 * and sets bp to the address of the newly allocated block.
 *
 * Returns the same passed-in bp pointer.
 */
static void* place(void* bp, size_t asize) {
    checkheap(__LINE__);

    // Get the size of the current block
    size_t csize = GET_SIZE(bp);
    printf("requested: %d; block size: %d\n", asize, csize);

    // FIXME: if root points to block being placed, there will be an error
    // If remainder block size >= 24, split it and append it to list
    if ((csize - asize) >= (3*DSIZE)) {
        // Update header and footer of requested block.
        // Note: FTRP depends on size value within header
        PUT(HDRP(bp), asize, 1);
        PUT(FTRP(bp), asize, 1);

        // Set the header and footer of next block
        void* bp_next = NEXT_BLKP(bp);
        PUT(HDRP(bp_next), csize - asize, 0);
        PUT(FTRP(bp_next), csize - asize, 0);
        insert_block(bp_next);

    } else { // Remainder block is too small. Just update the alloc bit
        PUT(HDRP(bp), csize, 1);
        PUT(FTRP(bp), csize, 1);
        remove_block(bp);
    }
    return bp;
}

/* mm_free - free current block, and coalesce prev and next if possible. 
 * Assume bp points to the start of a block.
 */
void mm_free(void* bp) {
    checkheap(__LINE__);
    size_t size = GET_SIZE(bp);
    PUT(HDRP(bp), size, 0); // reset header bit
    PUT(FTRP(bp), size, 0); // reset footer bit
    coalesce(bp);
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
    void* newptr;
    
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

/* Checks the heap for correctness. Call this function using checkheap(__LINE__) */
void mm_check(int lineno) {
    // are there any contiguous free blocks that somehow escaped coalescing?
    // is every free block actually in the free list?
    printf("called from %d. ", lineno);
    printf("root = %p\n", root);

    // do headers and footers match?
    // is payload aligned?
    // First fit search: traverse linked lists until valid free block found
    void* bp = root;
    while (bp != NULL) {
        if (GET_ALLOC(bp)) {
            printf("ERROR: Not all blocks in linked list are free\n");
            exit(-1);
        }
        bp = *(void**) ((char*) bp + sizeof(void*));
    }
}