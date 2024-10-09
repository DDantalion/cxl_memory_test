#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdbool.h>

#define DEBUG 1
#define debug_print(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
            __LINE__, __func__, __VA_ARGS__); } while (0)


/* text color */
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

typedef struct test_cfg {
    // overall
    uint64_t total_buf_size;
    int buf_a_numa_node;
    char* buf_a;
    int core_a;
    char* start_addr_a;

} test_cfg_t;

int get_node(void *p, uint64_t size);

int init_buf(uint64_t size, int node, char** alloc_ptr);

uint64_t read_MSR(int cpu);

void write_MSR(int cpu, uint64_t val);

void disable_prefetch(int cpu);

void enable_prefetch(int cpu);

#endif // UTIL_H