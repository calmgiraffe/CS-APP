#include <unistd.h>

/* mem_init - initialize the memory system model */
void mem_init(void);

/* mem_deinit - free the storage used by the memory system model */            
void mem_deinit(void);

/* mem_sbrk - simple model of the sbrk function. Expands the heap by incr bytes,
 * where incr is a positive non-zero int. 
 *
 * Returns a generic pointer to the first byte of the newly allocated heap area.
 * If error, returns (void *) - 1 */
void *mem_sbrk(int incr);

/* mem_reset_brk - reset the simulated brk pointer to make an empty heap */
void mem_reset_brk(void);

/* mem_heap_lo - return address of the first heap byte */
void *mem_heap_lo(void);

/* mem_heap_hi - return address of last heap byte */
void *mem_heap_hi(void);

/* mem_heapsize() - returns the heap size in bytes */
size_t mem_heapsize(void);

/* mem_pagesize() - returns the page size of the system */
size_t mem_pagesize(void);