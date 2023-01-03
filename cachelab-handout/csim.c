#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define ADDR_LEN 64
typedef unsigned long long addr_t;

char strMap[4][14] = {"hit", "miss", "miss eviction", ""};

typedef struct cache_line {
    addr_t tag;
    unsigned long timestamp;
    bool isValid;
} cache_line_t;

typedef cache_line_t* cache_set_t;
typedef cache_set_t* cache_t;


/*
In initCache, cache is a stack address. Deference this and malloc memory
so that the 8B chunk in the stack points to the heap.
*/
void initCache(cache_t* cache, int s, int associativity) {

    *cache = (cache_set_t*) malloc(sizeof(cache_set_t) * s);
    for (int i = 0; i < s; i += 1) {

        (*cache)[i] = (cache_line_t*) malloc(sizeof(cache_line_t) * associativity);
        for (int j = 0; j < associativity; j += 1) {
            (*cache)[i][j].tag = 0;
            (*cache)[i][j].isValid = false;
            (*cache)[i][j].timestamp = 0;
        }
    }
}


/*
Results in either hit (pull/push data from data cache),
miss (data not in cache, pull from memory and replace !isValid line / push to
!isValid line), or miss + evict (data not in cache and replace LRU line)
*/
int load(cache_t* cache, addr_t setI, addr_t tag, int associativity, unsigned long timestamp) {
    // Iterate through set, see if line with isValid and matching tag 
    for (int j = 0; j < associativity; j += 1) {
        if ((*cache)[setI][j].isValid && (*cache)[setI][j].tag == tag) {
            (*cache)[setI][j].timestamp = timestamp;
            return 0; // hit
        }
    }
    // Miss: either find a non-valid line, or if all lines valid, the LRU line
    unsigned long lru = 0xFFFFFFFF;
    int lruIndex;
    for (int j = 0; j < associativity; j += 1) {

        // If an instance of !isValid, "load" into that line
        if (!((*cache)[setI][j].isValid)) {
            (*cache)[setI][j].isValid = true;
            (*cache)[setI][j].tag = tag;
            (*cache)[setI][j].timestamp = timestamp;
            return 1; // miss
        }
        // Finding the line with lowest lru in parallel
        if ((*cache)[setI][j].timestamp < lru) {
            lru = (*cache)[setI][j].timestamp;
            lruIndex = j;
        }
    }
    // goes to this line if all isValid
    (*cache)[setI][lruIndex].timestamp = timestamp; // representing an evict
    (*cache)[setI][lruIndex].tag = tag;
    return 2; // miss eviction
}


void freeCache(cache_t* cache, int s) {
    for (int i = 0; i < s; i += 1) {
        free((*cache)[i]);
    }
    free(*cache);
}


void printHelp(char* argv[]) {
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n\n", argv[0]);
    printf("Options:\n");
    printf("-h         Print this help message.\n");
    printf("-v         Optional verbose flag.\n");
    printf("-s <num>   Number of set index bits.\n");
    printf("-E <num>   Number of lines per set. \n");
    printf("-b <num>   Number of block offset bits.\n");
    printf("-t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf("linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
}


int main(int argc, char* argv[]) {
    unsigned long hits = 0;
    unsigned long misses = 0;
    unsigned long evictions = 0;
    
    bool enableVerbose = false;
    int setBits = 0;
    int blockBits = 0;
    int associativity = 0;
    int tagBits;
    char* tracefile;

    // By placing a colon as the first character of the options string,
    // getopt() returns ':' instead of '?' when no argument is given
    int opt;
    while ((opt = getopt(argc, argv, ":hvs:E:b:t:")) != -1) {
        switch(opt) {
            case 'h':
                printHelp(argv);
                break;
            case 'v':
                enableVerbose = true;
                break;
            case 's':
                setBits = atoi(optarg);
                if (setBits > 64 || setBits < 0) {
                    printf("s must be between 0 and 64.\n");
                    return 1;
                }
                break;
            case 'E':
                associativity = atoi(optarg);
                if (associativity < 1)  {
                    printf("E must be at least 1\n");
                    return 1;
                }
                break;
            case 'b':
                blockBits = atoi(optarg);
                if (blockBits > 64 || blockBits < 0)  {
                    printf("b must be between 0 and 64\n");
                    return 1;
                }
                break;
            case 't':
                tracefile = optarg;
                break;
            case ':':
                printf("Option requires an argument -- '%c'\n", optopt);
                return 1;
            case '?':
                if (isprint(optopt)) {
                    printf("Unknown option -- `%c'\n", optopt);
                } else {
                    printf("Unknown option character `\\x%x'\n", optopt);
                }
                return 1;
            default:
                abort();
        }
    }

    // Error checking
    FILE* file = fopen(tracefile, "r");
    if (file == NULL) {
        printf("Invalid or unspecified tracefile.\n");
        return 1;
    }
    // tagBits by definition, cannot be negative
    tagBits = ADDR_LEN - setBits - blockBits;
    if (tagBits < 0) {
        printf("Invalid combination of set and block bits.\n");
        return 1;
    }
    int tagShift = blockBits + setBits;

    // Masks for getting setIndex and tag from full address
    addr_t setMask = ~(0xffffffffffffffff << setBits);
    addr_t tagMask = ~(0xffffffffffffffff << tagBits);
    addr_t numSets = pow(2, setBits);
    cache_t cache;
    initCache(&cache, numSets, associativity);

    // Variables for parsed data
    char cmd;
    addr_t addr;
    int bytes;

    addr_t setIndex, tag;
    
    int result1, result2;
    unsigned long timestamp = 0;
    
    while (fscanf(file, " %c %llx,%d", &cmd, &addr, &bytes) == 3) {

        tag = (addr >> tagShift) & tagMask;
        setIndex = (addr >> blockBits) & setMask;
        if (cmd != 'I') {
            if (cmd == 'L' || cmd == 'S') {
                result1 = load(&cache, setIndex, tag, associativity, timestamp);
                result2 = 3;
            } else { // cmd = M
                result1 = load(&cache, setIndex, tag, associativity, timestamp);
                result2 = load(&cache, setIndex, tag, associativity, timestamp);
            }
            if (result1 == 0) {
                hits += 1;
            } else if (result1 == 1) {
                misses += 1;
            } else if (result1 == 2) {
                misses += 1;
                evictions += 1;
            }
            if (result2 == 0) {
                hits += 1;
            } else if (result2 == 1) {
                misses += 1;
            } else if (result2 == 2) {
                misses += 1;
                evictions += 1;
            }
        } else {
            continue;
        }
        if (enableVerbose) {
            printf("%c %llx,%d %s %s\n", cmd, addr, bytes, strMap[result1], strMap[result2]);
        }
        timestamp += 1;
    }
    printSummary(hits, misses, evictions);
    freeCache(&cache, numSets);
    fclose(file);
    return 0;
}