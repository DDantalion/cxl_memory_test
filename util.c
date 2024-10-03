#include "util.h"
#include <stdio.h>
#include <numa.h>
#include <numaif.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define PREFETCH_REG_ADDR   0x1A4
int get_node(void *p, uint64_t size)
{
    int* status;
    void** page_arr;
    unsigned long page_size;
    unsigned long page_cnt;
    int ret;
    char* start_addr;

    page_size = (unsigned long)getpagesize();
    page_cnt = (size / page_size);
    status = malloc(page_cnt * sizeof(int));
    page_arr = malloc(page_cnt * sizeof(char*));
    start_addr = (char*)p;

    //fprintf(stdout, "[get_node] buf: %lx, page_size: %ld, page_cnt: %ld\n", (uint64_t)(p), page_size, page_cnt);

    for (unsigned long i = 0; i < page_cnt; i++) {
        page_arr[i] = start_addr;
        if (i < page_cnt) {
            start_addr = &(start_addr[page_size]);
        }
    }


    ret = move_pages(0, page_cnt, page_arr, NULL, status, 0); 
    if (ret != 0) {
        fprintf(stderr, "Problem in %s line %d calling move_pages(), ret = %d\n", __FILE__,__LINE__, ret);
        printf("%s\n", strerror(errno));
    }

    ret = status[0];
    for (uint64_t i = 0; i < page_cnt; i++) {
        if (ret != status[i]) {
            fprintf(stderr, "found page: %lu on node: %d, different from node: %d\n", i, status[i], ret);
            ret = status[i];
            break;
        }
    }

    if (ret == status[0]) {
        //fprintf(stdout, "all pages: %lx, %lx ... are on node: %d\n", (uint64_t)(page_arr[0]), (uint64_t)(page_arr[1]), ret);
    }

    free(page_arr);
    free(status);
    return ret;
}

int init_buf(uint64_t size, int node, char** alloc_ptr) {
    char *ptr;
    int ret;
    unsigned long page_size;
    uint64_t page_cnt;
    uint64_t idx;

    if ((ptr = (char *)numa_alloc_onnode(size, node)) == NULL) {
        fprintf(stderr,"Problem in %s line %d allocating memory\n",__FILE__,__LINE__);
        return -1;
    }
    //printf("[INFO] done alloc. Next, touch all pages\n");
    // alloc is only ready when accessed
    page_size = (unsigned long)getpagesize();
    page_cnt = (size / page_size);
    idx = 0;
    for (uint64_t i = 0; i < page_cnt; i++) {
        ptr[idx] = 0;	
        idx += page_size;
    }
    //printf("[INFO] done touching pages. Next, validate on node X\n");
    ret = get_node(ptr, size);
    if (ret != node) {
        printf("ptr is on node %d, but expect node %d\n", ret, node);
        return -2;
    }
    //printf("ptr is on node %d\n", ret);
    //printf("allocated: %luMB\n", (size >> 20));

    *alloc_ptr = ptr;
    
    return 0;
}

uint64_t read_MSR(int cpu){
    int fd;
    uint64_t data;
    char msr_file_name[64];

    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open(msr_file_name, O_RDONLY);

    if (fd < 0) {
        if (errno == ENXIO) {
            fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
            exit(2);
        } else if (errno == EIO) {
            fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
                    cpu);
            exit(3);
        } else {
            perror("rdmsr: open");
            exit(127);
        }
    }

    if (pread(fd, &data, sizeof data, PREFETCH_REG_ADDR) != sizeof data) {
        if (errno == EIO) {
            fprintf(stderr, "rdmsr: CPU %d cannot read ", cpu);
            exit(4);
        } else {
            perror("rdmsr: pread");
            exit(127);
        }
    }

    close(fd);

    return data;
}

void write_MSR(int cpu, uint64_t val){
    int fd;
    char msr_file_name[64];

    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open(msr_file_name, O_WRONLY);

    if (fd < 0) {
        if (errno == ENXIO) {
            fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
            exit(2);
        } else if (errno == EIO) {
            fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
                    cpu);
            exit(3);
        } else {
            perror("rdmsr: open");
            exit(127);
        }
    }

    if (pwrite(fd, &val, sizeof(val), PREFETCH_REG_ADDR) != sizeof(val)){
        if (errno == EIO) {
            fprintf(stderr,
                    "wrmsr: CPU %d cannot set MSR ", cpu);
            exit(4);
        } else {
            perror("wrmsr: pwrite");
            exit(127);
        }
    }

    close(fd);

    return;
}

void disable_prefetch(int cpu){
    uint64_t val;
    val = read_MSR(cpu);
    write_MSR(cpu, val | 0xF);
    val = read_MSR(cpu);
    //printf(YEL "[INFO]" RESET " CPU %d prefetch disabled. Now at 0x1A4: %lx\n", cpu, val);
}

void enable_prefetch(int cpu){
    uint64_t val;
    val = read_MSR(cpu);
    write_MSR(cpu, val & 0xFFFFFFFFFFFFFFF0);
    //printf(YEL "[INFO]" RESET " CPU %d prefetch enabled.\n", cpu);
}