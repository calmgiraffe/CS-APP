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
// Given a block pointer bp, sets the pointer of its prev pointer
#define SET_PREV_PTR(bp, ptr) (*(void**) (bp) = (void*) (ptr))
// Given a block pointer bp, sets the pointer of its next pointer
#define SET_NEXT_PTR(bp, ptr) (*(void**) ((char *) bp + 8) = (void*) (ptr))
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
#define checkheap(lineno) (mm_checkheap(lineno))
// #define checkheap(lineno)

static void* heap_listp; // pointer to first byte of heap
static void* root; // pointer to beginning of linked list of free blocks
static void* coalesce(void* bp);
static void* find_fit(size_t asize);
static void* place(void *bp, size_t asize);
static void* extend_heap(size_t words);

/* Initialize the malloc package. Places prologue and eplilogue headers, then
 * extends the heap by CHUNKSIZE. Points heap_listp to the empty payload of the
 * prologue, i.e., the prologue footer.
 * 
 * Returns 0 if sucessful, -1 if error. 
 */
int mm_init(void)
{   
    heap_listp = mem_sbrk(4*WSIZE);
    if ((long) heap_listp == -1) {
        return -1;
    }
    PUT(heap_listp, 0, 0);               // Alignment padding
    PUT(heap_listp + 1*WSIZE, DSIZE, 1); // Prologue header
    PUT(heap_listp + 2*WSIZE, DSIZE, 1); // Prologue footer
    PUT(heap_listp + 3*WSIZE, 0, 1);     // Epilogue header
    heap_listp += 2*WSIZE;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    // Set root to point to the newly created single linked list
    // [x] created the initial element of the linked list
    root = heap_listp + 2*WSIZE;
    SET_PREV_PTR(root, NULL);
    SET_NEXT_PTR(root, NULL);
    return 0;
}

/* Extend the heap by "words" number of words. Newly allocated heap memory is
 * effectively appended to the end of the current heap.
 *
 * Returns a generic pointer to the newly allocated block if successful.
 * Returns NULL on error. 
 */
static void* extend_heap(size_t words) {
    char* bp;
    size_t size;

    /* Extend by an even number of words to maintain alignment. */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    bp = mem_sbrk(size);
    if ((long) bp == -1) {
        return NULL;
    }
    /* Within the new free block, place header, footer, and the epilogue header. */
    PUT(HDRP(bp), size, 0); // old epilogue block becomes the new block header
    PUT(FTRP(bp), size, 0);
    PUT(HDRP(NEXT_BLKP(bp)), 0, 1); // placing new epilogue block

    bp = coalesce(bp);
    return bp;
}

/* Given a block pointer bp to a free block, coalesce curr block with the prev
 * and next blocks if possible. Returns a pointer to the start of the newly
 * coalesced block. If bp is not free, behavior is undefined.
 */
static void* coalesce(void* bp) {
    // TODO: edit to match for linked list
    size_t prevIsAlloc = GET_ALLOC(PREV_BLKP(bp));
    size_t nextIsAlloc = GET_ALLOC(NEXT_BLKP(bp));
    size_t size = GET_SIZE(bp);
    /*
    case 1: prev and next blocks are allocated
    case 2: next block is free
    case 3: prev block is free
    case 4: both next and prev blocks are free
    */
    if (prevIsAlloc && nextIsAlloc) {
        // [x] Insert freed block at the root of the list
        /*
        SET_PREV_PTR(bp, NULL); 
        SET_NEXT_PTR(bp, root); // bp.next = head
        SET_PREV_PTR(root, bp); // head.prev = bp
        root = bp; // root = bp
        */
        return bp;

    } else if (prevIsAlloc && !nextIsAlloc) {
        // [x] Splice out successor block, coalesce both memory blocks and insert
        // the new block at the end of the list
        size += GET_SIZE(NEXT_BLKP(bp));
        PUT(HDRP(bp), size, 0);
        PUT(FTRP(bp), size, 0);

        // void* bpNext = NEXT_BLKP(bp);

    } else if (!prevIsAlloc && nextIsAlloc) {
        // [x] Splice out predecessor block, coalesce both memory blocks and insert
        // the new block at the end of the list
        size += GET_SIZE(PREV_BLKP(bp));
        PUT(FTRP(bp), size, 0);            // reset curr footer
        PUT(HDRP(PREV_BLKP(bp)), size, 0); // reset prev header
        bp = PREV_BLKP(bp);

    } else {
        // [x] Splice out predecessor & sucessor block, coalesce both memory blocks and insert
        // the new block at the end of the list
        size += GET_SIZE(PREV_BLKP(bp)) + GET_SIZE(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), size, 0); // reset prev header
        PUT(FTRP(NEXT_BLKP(bp)), size, 0); // reset next footer
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/* Allocate a block whose size is a multiple of the alignment.
 * Returns a pointer to the newly allocated block if successful, NULL on error.
 */
void* mm_malloc(size_t size) {
    size_t allocSize; // adjusted block size
    char* bp;

    /* Adjust block size to include overhead and alignment reqs. 
    A next & prev pointer, a header, and a footer take up 24 B-- thus, this is
    the minimum block size. The payload area will be 16 B.

    If <= 16 B is requested, data will fit in the minimum block size of 24. 
    Else, round up to the nearest multiple of 8.
    */
    if (size == 0) {
        return NULL;
    } else if (size <= 2*DSIZE) {
        allocSize = 3*DSIZE;
    } else {
        allocSize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);
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
    void* bp = root;

    /*
    // First fit search: traverse linked lists until valid block found
    while (bp != NULL) {
        if (!GET_ALLOC(bp) && (asize <= GET_SIZE(bp))) {
            return bp;
        }
        bp = *((void**) (char*) bp + 8); // NOTE: this is probably correct for explicit free lists
    }
    */
    while (GET_SIZE(bp) > 0) {
        if (!GET_ALLOC(bp) && (asize <= GET_SIZE(bp))) {
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
    size_t csize = GET_SIZE(bp);

    // If remainder block size >= 24
    if ((csize - asize) >= (3*DSIZE)) {
        // Update header and footer of requested block.
        // Note: FTRP depends on size value within header
        PUT(HDRP(bp), asize, 1);
        PUT(FTRP(bp), asize, 1);
        // Set the header and footer of next block
        void* bp_next = NEXT_BLKP(bp);
        PUT(HDRP(bp_next), csize - asize, 0);
        PUT(FTRP(bp_next), csize - asize, 0);

    } else { 
        // Remainder block is too small. Update current block's header
        PUT(HDRP(bp), csize, 1);
        PUT(FTRP(bp), csize, 1);
    }
    return bp;
}

/* mm_free - free current block, and coalesce prev and next if possible. 
 * Assume bp points to the start of a block.
 */
void mm_free(void* bp) {
    size_t size = GET_SIZE(bp);

    PUT(HDRP(bp), size, 0); // reset header bit
    PUT(FTRP(bp), size, 0); // reset footer bit
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

/* Checks the heap for correctness. Call this function using mm_check(__LINE__) */
void mm_check(int lineno) {
    printf("checkheap called from %d\n", lineno);
    // check for invariants
    // is every block in the free list marked as free?
    // are there any contiguous free blocks that somehow escaped coalescing?
    // is every free block actually in the free list?
}