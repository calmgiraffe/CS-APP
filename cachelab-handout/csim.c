#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

typedef unsigned long long addr_t;

typedef struct cache_line {
    addr_t tag;
    unsigned long timestamp;
    bool isValid;
} cache_line_t;

// Define a new type cache_set_t that is a pointer to cache_line_t
// Define a new type cache_t that is a pointer to cache_set_t
typedef cache_line_t* cache_set_t;
typedef cache_set_t* cache_t;


/*
Initialize cache.
For cache, which is a pointer to an array of pointers, allocate memory
equal to the size of the pointer * s. For cache[i], which is an array of
cache_line_t, allocate memory equal to sizeof(cache_line_t) * E.
Finally set all struct instance variables to 0.
*/
void initCache(cache_t* cache, int s, int E) {

    *cache = (cache_set_t*) malloc(sizeof(cache_set_t) * s);
    for (int i = 0; i < s; i += 1) {

        (*cache)[i] = (cache_line_t*) malloc(sizeof(cache_line_t) * E);
        for (int j = 0; j < E; j += 1) {
            (*cache)[i][j].tag = 0;
            (*cache)[i][j].isValid = false;
            (*cache)[i][j].timestamp = 0;
        }
    }
}


/*
Function representing pulling data from the cache.
Results in either hit (data in cache), miss (data not in cache),
or miss + evict (data not in cache and replace LRU line)
*/
char* load(cache_t c, addr_t setI, addr_t tag, int E, unsigned long timestamp) {
    // Loading results in either hit, miss, or miss + evict

    // Iterate through the particular set, see if there is a line with isValid
    // and matching tag. If yes, hit
    for (int i = 0; i < E; i += 1) {
        bool allValid = allValid && c[setI][E].isValid;
        if (c[setI][E].isValid && c[setI][E].tag == tag) {
            c[setI][E].timestamp = timestamp;
            return "hit";
        }
    }
    // Miss: either find a non-valid line, or if all lines valid, the LRU line
    unsigned long lru = 0xFFFFFFFF;
    int lruIndex;
    for (int i = 0; i < E; i += 1) {
        if (!(c[setI][E].isValid)) {
            c[setI][E].isValid = true; // representing a load into cache
            return "miss";
        }
        if (c[setI][E].timestamp < lru) {
            lru = c[setI][E].timestamp;
            lruIndex = i;
        }
    }
    c[setI][lruIndex].timestamp = timestamp; // representing an evict
    c[setI][lruIndex].tag = tag;
    return "miss eviction";
}


/*
Function representing pushing data to the cache.
Results in either hit (data in cache), miss (data not in cache),
or miss + evict (data not in cache and replace LRU line)
*/
char* store(cache_t c, addr_t setI, addr_t tag, int E, unsigned long timestamp) {
    // Loading results in either hit, miss, or miss + evict

    // Iterate through the particular set, see if there is a line with isValid
    // and matching tag. If yes, hit
    for (int i = 0; i < E; i += 1) {
        bool allValid = allValid && c[setI][E].isValid;

        if (c[setI][E].isValid && c[setI][E].tag == tag) {
            c[setI][E].timestamp = timestamp;
            return "hit";
        }
    }
    // Miss: either find a non-valid line, or if all lines valid, the LRU line
    unsigned long lru = 0xFFFFFFFF;
    int lruIndex;
    for (int i = 0; i < E; i += 1) {
        if (!(c[setI][E].isValid)) {
            c[setI][E].isValid = true; // representing a load into cache
            return "miss";
        }
        if (c[setI][E].timestamp < lru) {
            lru = c[setI][E].timestamp;
            lruIndex = i;
        }
    }
    c[setI][lruIndex].timestamp = timestamp; // representing an evict
    c[setI][lruIndex].tag = tag;
    return "miss eviction";
}


void freeCache(cache_t cache, int s, int E) {
    cache = (cache_set_t*) malloc(sizeof(cache_set_t) * s);
    for (int i = 0; i < s; i += 1) {
        for (int j = 0; j < E; j += 1) {
            free(&cache[s][E]);
        }
    }
}


/*
Print help message to terminal.
*/
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
    bool enableVerbose = false;
    int E = 0; // associativity
    int setBits = 0;
    int blockBits = 0;
    int tagBits;
    char* tracefile;
    cache_t cache;

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
                E = atoi(optarg);
                if (E < 1)  {
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

    // Checking for valid inputs
    // tagBits by definition, cannot be negative
    // Thus, setBits + blockBits cannot be greater than 64
    tagBits = 64 - setBits - blockBits;
    if (tagBits < 0) {
        printf("Invalid combination of set and block bits.\n");
        return 1;
    }
    FILE* file = fopen(tracefile, "r");
    if (file == NULL) {
        printf("Invalid or unspecified tracefile.\n");
        return 1;
    }

    // Masks for getting setIndex and tag from full address
    
    addr_t setMask = ~(0xffffffffffffffff << setBits);
    addr_t tagMask = ~(0xffffffffffffffff << tagBits);
    addr_t numSets = pow(2, setBits);
    initCache(&cache, numSets, E);

    char cmd;
    addr_t addr;
    int bytes;
    
    addr_t setIndex;
    addr_t tag;
    int tagShift = blockBits + setBits;
    unsigned long timestamp = 0;
    char result[20];
    while (fscanf(file, " %c %llx,%d", &cmd, &addr, &bytes) == 3) {

        if (cmd == 'L') {
            setIndex = (addr >> blockBits) & setMask;
            tag = (addr >> tagShift) & tagMask;
            strcat(result, load(cache, setIndex, tag, E, timestamp));

        } else if (cmd == 'M') {
            setIndex = (addr >> blockBits) & setMask;
            tag = (addr >> tagShift) & tagMask;
            strcat(result, load(cache, setIndex, tag, E, timestamp));
            strcat(result, " ");
            strcat(result, store(cache, setIndex, tag, E, timestamp));

        } else if (cmd == 'S') {
            setIndex = (addr >> blockBits) & setMask;
            tag = (addr >> tagShift) & tagMask;
            strcat(result, load(cache, setIndex, tag, E, timestamp));

        } else {
            continue;
        }
        // Print the statement here
        if (enableVerbose) {
            printf("%c %llx,%d %s\n", cmd, addr, bytes, result);
        }
        result[0] = '\0';
        timestamp += 1;
    }
    
    freeCache(cache, numSets, E);
    fclose(file);
    return 0;
}