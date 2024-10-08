/*
  These cache utils have been stitched together over time, including modifications.
  We try to attribute them to original sources where we can.
*/

#include <stdlib.h>
#include <stdio.h>
#include <numa.h>
#include <numaif.h>
#include <stdint.h>
#include "util.h"

////////////////////////////////////////////////////////////////////////////////
static double v4time[25];
inline
void
// Attribution: https://github.com/IAIK/flush_flush/blob/master/sc/cacheutils.h
clflush(void *p)
{
	__asm__ volatile ("clflush 0(%0)" : : "c" (p) : "rax");
}

inline
void 
// Attribution: https://github.com/jovanbulck/sgx-tutorial-space18/blob/master/common/cacheutils.h
clflush_f(void *p)
{
  asm volatile (
    "mfence\n"
    "clflush 0(%0)\n"
    "mfence\n"
    :
    : "D" (p)
    : "rax");
}

////////////////////////////////////////////////////////////////////////////////

inline
uint64_t
// https://github.com/cgvwzq/evsets/blob/master/micro.h
rdtsc()
{
	unsigned a, d;
	__asm__ volatile ("cpuid\n"
	"rdtsc\n"
	"mov %%edx, %0\n"
	"mov %%eax, %1\n"
	: "=r" (a), "=r" (d)
	:: "%rax", "%rbx", "%rcx", "%rdx");
	return ((uint64_t)a << 32) | d;
}

// Attribution: https://cs.adelaide.edu.au/~yval/Mastik/
uint64_t rdtscp64() {
  uint32_t low, high;
  asm volatile ("rdtscp": "=a" (low), "=d" (high) :: "ecx");
  return (((uint64_t)high) << 32) | low;
}

////////////////////////////////////////////////////////////////////////////////

uint32_t memaccesstime(char *v, uint64_t size) {
  uint32_t rv;
  asm volatile(
                "xor %%r10, %%r10 \n"
                "mfence\n"
               "lfence\n"
               "rdtscp\n"
               "mov %%eax, %%esi\n"
            "LOOP_access: \n"
              "lfence\n"
              "mov (%1), %%eax\n"
              "mfence\n"
              "lfence\n"
              "clflush 0(%1) \n" 
              "add $0x40, %1\n"
              "inc %%r10 \n"
              "cmp %2, %%r10 \n"
            "jl LOOP_access \n"
               "rdtscp\n"
               "sub %%esi, %%eax\n"
               : "=&a"(rv)
               : "r"(v), "r" (size)
               : "ecx", "edx", "esi","%r10");
               return rv;
}

uint32_t memaccesstime_s(char *v, uint64_t size) {
  uint32_t rv;
  asm volatile(
                "xor %%r10, %%r10 \n"
                "mfence\n"
               "sfence\n"
               "rdtscp\n"
               "mov %%eax, %%esi\n"
            "LOOP_access_s: \n"
            "sfence\n"
              "movl $10, (%1)\n"
              "mfence\n"
               "sfence\n"
              "clflush 0(%1) \n" 
              "add $0x40, %1\n"
              "inc %%r10 \n"
              "cmp %2, %%r10 \n"
            "jl LOOP_access_s \n"
               "rdtscp\n"
               "sub %%esi, %%eax\n"
               : "=&a"(rv)
               : "r"(v), "r" (size)
               : "ecx", "edx", "esi","%r10");
               return rv;
}





inline
void
// Attribution: https://github.com/IAIK/flush_flush/blob/master/sc/cacheutils.h
maccess(void* p)
{
	__asm__ volatile ("movq (%0), %%rax\n" : : "c" (p) : "rax");
}

inline 
void 
mwrite(void *v)
{
  asm volatile (
    "mfence\n\t"
    "lfence\n\t"
    "movl $10, (%0)\n\t"
    "mfence\n\t"
    : 
    : "D" (v)
    : );
}

inline 
int 
// Attribution: https://cs.adelaide.edu.au/~yval/Mastik/
mread(void *v) 
{
  int rv = 0;
  asm volatile("mov (%1), %0": "+r" (rv): "r" (v):);
  return rv;
}

inline
int 
// Attribution: https://cs.adelaide.edu.au/~yval/Mastik/
time_mread(void *adrs) 
{
  volatile unsigned long time;

  asm volatile (
    // "lfence\n"
    "mfence\n"
    "rdtscp\n"
    "lfence\n"
    "mov %%eax, %%esi\n"
    "mov (%1), %%eax\n"
    "rdtscp\n"
    "sub %%esi, %%eax\n"
    : "=&a" (time)          // output
    : "r" (adrs)            // input
    : "ecx", "edx", "esi"); // clobber registers

  return (int) time;
}

inline
int 
// Attribution: https://cs.adelaide.edu.au/~yval/Mastik/ (modified)
time_mread_nofence(void *adrs) 
{
  volatile unsigned long time;

  asm volatile (
    "rdtscp\n"
    "movl %%eax, %%esi\n"
    "movl (%1), %%eax\n"
    "rdtscp\n"
    "sub %%esi, %%eax\n"
    : "=&a" (time)          // output
    : "r" (adrs)            // input
    : "ecx", "edx", "esi"); // clobber registers

  return (int) time;
}

int m_test(long int buf_size,long int count, int m){
uint32_t t_time;
double avg_time = 0;
    int ret;
    uint64_t iter_count;
    iter_count= (uint64_t)count;
    test_cfg_t* cfg;
    count= count * 64;
    //printf("memory size: %ld Bytes, ",count);
    cfg = malloc(sizeof(test_cfg_t));
    cfg->buf_a_numa_node = 0;
    cfg->total_buf_size = buf_size; // in bytes
    cfg->core_a=0;
    ret = init_buf(cfg->total_buf_size, cfg->buf_a_numa_node, &(cfg->buf_a));
    if (ret < 0) {
        if (ret == -1) {
            printf("BAD init_buf buf_a, fail to alloc\n");
            goto out;
        } else { // already alloc, needs to free
            printf("BAD init_buf buf_a, alloc strange\n");
            goto out1;
        }
    }
    cfg->start_addr_a = &(cfg->buf_a[0]);
    disable_prefetch(cfg->core_a);
    t_time = memaccesstime(cfg->start_addr_a, iter_count);
    avg_time += (((double) t_time));
    v4time[m]+=(avg_time/50);
    enable_prefetch(cfg->core_a);
    goto out1;

out1:
    numa_free(cfg->buf_a, cfg->total_buf_size);
    return 0;
out:
    free(cfg);
    return 0;	
}

int main(){
FILE *file;
file = fopen("output.txt", "a");
for (int x=0; x<50; x++){
int m = 0;
for(long int j=1; j<30000000; j=(j*2)){
m_test(4000000000, j, m);
m=m+1;
}
}
for (int i = 0; i < 25; i++) {
  printf("v4time[%d] = %.2f\n", i, v4time[i]);  // Print to stdout
  fprintf(file, "v4time[%d] = %.2f\n", i, v4time[i]);  // Print to file
}
fclose(file);
return 0;
}