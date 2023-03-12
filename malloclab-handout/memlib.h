#include <unistd.h>

/* mem_init - initialize the memory system model */
void mem_init(void);

/* mem_deinit - free the storage used by the memory system model */            
void mem_deinit(void);

/* mem_sbrk - simple model of the sbrk function. Increments mem_brk pointer by 
 * incr, where incr is a positive non-zero int. This has the effect of
 * expanding the heap by incr bytes.
 *
 * Returns a generic pointer to the first byte of the newly allocated heap area,
 * i.e., the old pointer to mem_brk. If error, returns (void *) -1 */
void *mem_sbrk(int incr);

/* mem_reset_brk - reset the simulated brk pointer to make an empty heap.
 * Pointer to last byte of heap = pointer to first byte of heap  */
void mem_reset_brk(void);

/* mem_heap_lo - return address of the first heap byte */
void *mem_heap_lo(void);

/* mem_heap_hi - return address of last heap byte */
void *mem_heap_hi(void);

/* mem_heapsize() - returns the heap size in bytes */
size_t mem_heapsize(void);

/* mem_pagesize() - returns the page size of the system */
size_t mem_pagesize(void);