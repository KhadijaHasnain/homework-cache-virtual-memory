#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define ADDRESS_SPACE_SIZE 16777216 // 16MB
#define MAX_CACHE_SIZE 2097152      // 2MB
#define MAX_BLOCK_SIZE 1024

// Structure representing a line in the cache
typedef struct {
    uint32_t tag;
    int valid;
    int lru_counter;
} cache_line_t;

unsigned char main_memory[ADDRESS_SPACE_SIZE]; // Simulated main memory array

// Function declarations
void process_trace_file(const char *filename, cache_line_t *cache, int sets, int ways, int block_size);
int get_set_index(uint32_t address, int sets, int block_size);
uint32_t get_tag(uint32_t address, int sets, int block_size);
void access_cache(uint32_t address, const char *access_type, int access_size, char* value, cache_line_t *cache, int sets, int ways, int block_size);
void update_lru_counters(cache_line_t *cache, int set_index, int ways, int accessed_line);
char* read_memory(uint32_t address, int size);
void write_memory(uint32_t address, char* value, int size);
int find_least_recently_used(cache_line_t *cache, int set_index, int ways);


    // Main function: Entry point of the program. Processes command-line arguments and initiates cache simulation.

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: ./cachesim <trace-file> <cache-size-kB> <associativity> <block-size>\n");
        return 1;
    }

    char *trace_file = argv[1];
    int cache_size = atoi(argv[2]) * 1024;
    int associativity = atoi(argv[3]);
    int block_size = atoi(argv[4]);

    if (cache_size > MAX_CACHE_SIZE || block_size > MAX_BLOCK_SIZE) {
        printf("Invalid cache size or block size\n");
        return 1;
    }

    int sets = cache_size / (associativity * block_size);
    cache_line_t *cache = (cache_line_t *)malloc(sets * associativity * sizeof(cache_line_t));
    if (!cache) {
        printf("Memory allocation failed for cache\n");
        return 1;
    }

    for (int i = 0; i < sets * associativity; i++) {
        cache[i].valid = 0;
        cache[i].tag = 0;
        cache[i].lru_counter = 0;
    }

    memset(main_memory, 0, ADDRESS_SPACE_SIZE);
    process_trace_file(trace_file, cache, sets, associativity, block_size);
    free(cache);
    return 0;
}
    // Processes each line of the given trace file, simulating cache behavior for each memory access.

void process_trace_file(const char *filename, cache_line_t *cache, int sets, int ways, int block_size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open trace file: %s\n", filename);
        return;
    }

    char line[256];
    char access_type[10];
    uint32_t address;
    char value[8]; // Buffer to store value for 'store' operations
    int access_size;

    while (fgets(line, sizeof(line), file)) {
        // Parsing the line depending on whether it's a 'store' or 'load'
        if (strncmp(line, "store", 5) == 0) {
            if (sscanf(line, "%9s 0x%x %d %8s", access_type, &address, &access_size, value) != 4) {
                continue;
            }
        } else if (strncmp(line, "load", 4) == 0) {
            if (sscanf(line, "%9s 0x%x %d", access_type, &address, &access_size) != 3) {
                continue;
            }
            memset(value, 0, sizeof(value)); // Clearing value for 'load'
        } else {
            continue; // Invalid line format
        }

        // Accessing the cache based on the operation type
        access_cache(address, access_type, access_size, value, cache, sets, ways, block_size);
    }

    fclose(file);
}
    // Simulates accessing the cache with the given address and access type (load/store), and updates the cache accordingly.

void access_cache(uint32_t address, const char *access_type, int access_size, char value[8], cache_line_t *cache, int sets, int ways, int block_size) {
    int set_index = get_set_index(address, sets, block_size);
    uint32_t tag = get_tag(address, sets, block_size);
    int hit = 0;

    for (int i = 0; i < ways; i++) {
        int idx = set_index * ways + i;
        if (cache[idx].valid && cache[idx].tag == tag) {
            hit = 1;
            update_lru_counters(cache, set_index, ways, idx);
            break;
        }
    }

    if (strcmp(access_type, "store") == 0) {
        write_memory(address, value, access_size);
        printf("%s 0x%x miss\n", access_type, address);
    } 
    else if (strcmp(access_type, "load") == 0) {
        if (!hit) {
            int lru_index = find_least_recently_used(cache, set_index, ways);
            cache[lru_index].valid = 1;
            cache[lru_index].tag = tag;
            update_lru_counters(cache, set_index, ways, lru_index);
            
            char *read_value = read_memory(address, access_size);
            
            printf("%s 0x%x miss ", access_type, address);
            for (int i = 0; i < access_size; i++) {
                printf("%02x", (unsigned char)read_value[i]);
            }
            printf("\n");
        } else {
            char *read_value = read_memory(address, access_size);
            printf("%s 0x%x hit ", access_type, address);
            for (int i = 0; i < access_size; i++) {
                printf("%02x", (unsigned char)read_value[i]);
            }
            printf("\n");
        }
    }
}
    // Updates the LRU counters for cache lines in a set after an access, to maintain the least recently used information.

void update_lru_counters(cache_line_t *cache, int set_index, int ways, int accessed_line) {
    for (int i = 0; i < ways; i++) {
        int idx = set_index * ways + i;
        if (idx == accessed_line) {
            cache[idx].lru_counter = 0;
        } else {
            cache[idx].lru_counter++;
        }
    }
}
    // Calculates the set index in the cache for a given address.

int get_set_index(uint32_t address, int sets, int block_size) {
    return (address / block_size) & (sets - 1);
}
    // Extracts the tag from a given address.

uint32_t get_tag(uint32_t address, int sets, int block_size) {
    int offset_bits = 0;
    int temp = block_size;
    while (temp >>= 1) ++offset_bits;

    int index_bits = 0;
    temp = sets;
    while (temp >>= 1) ++index_bits;

    return address >> (offset_bits + index_bits);
}
    // Reads a value from the main memory at the specified address and size, considering the block boundaries.

char* read_memory(uint32_t address, int size) {
    static char value[8]; // Static buffer to hold the read value

    if (address + size > ADDRESS_SPACE_SIZE) {
        printf("Memory read out of bounds\n");
        return NULL;
    }

    for (int i = 0; i < size; i++) {
        value[i] = main_memory[address + i];
    }

    return value;
}
    // Writes a value to the main memory at the specified address and size.

void write_memory(uint32_t address, char* value, int size) {
    if (address + size > ADDRESS_SPACE_SIZE) {
        printf("Memory write out of bounds\n");
        return;
    }

    for (int i = 0; i < size; i++) {
        main_memory[address + i] = value[i];
    }
}
    // Finds the least recently used cache line in a set for replacement purposes.

int find_least_recently_used(cache_line_t *cache, int set_index, int ways) {
    int lru_index = set_index * ways;
    int max_lru_counter = cache[lru_index].lru_counter;

    for (int i = 1; i < ways; i++) {
        int idx = set_index * ways + i;
        if (cache[idx].lru_counter > max_lru_counter) {
            lru_index = idx;
            max_lru_counter = cache[idx].lru_counter;
        }
    }

    return lru_index;
}


