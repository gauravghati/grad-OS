/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

/**
 * Needs:
 *   fstat()
 *   S_ISREG()
 *   open()
 *   close()
 *   sbrk()
 *   mmap()
 *   munmap()
 *   msync()
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "scm.h"

#define VIRT_ADDR 0x600000000000

#define INT_SIZE 2 * sizeof(int)
#define METADATA_SIZE 2 * sizeof(size_t)

struct scm {
    int fd;
    void *base;
    void *mapped;
    size_t capacity;
    size_t utilized;
};

void scm_close(struct scm *scm) {
    if(scm == NULL){
        return ;
    }

    if (scm->mapped != MAP_FAILED) {
        if (msync(scm->mapped, scm->capacity, MS_SYNC) == -1) {
            perror("Error while msync");
        }
        if (munmap(scm->mapped, scm->capacity) == -1) {
            perror("Error while munmap");
        }
    }

    if (scm->fd != -1) {
        close(scm->fd);
    }

    FREE(scm);
}

struct scm *scm_open(const char *pathname, int truncate) {
    struct scm  *scm = (struct scm*) malloc(sizeof(struct scm));
    struct stat st;
    int *size_info;

    if(scm == NULL) {
        printf("error with scm\n");
        close(scm->fd);
        return NULL;
    }

    scm->fd = open(pathname, O_RDWR);
    if(scm->fd == -1){
        printf("error with fd\n");
        return NULL;
    }

    if(fstat(scm->fd, &st) == -1) {
        printf("error with fstat\n");
        close(scm->fd);
        FREE(scm);
        return NULL;
    }

    if (!S_ISREG(st.st_mode)) {
        TRACE("Error");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    scm->capacity = st.st_size;
    scm->utilized = 0;

    scm->mapped = mmap((void *) VIRT_ADDR, scm->capacity, 
                                PROT_EXEC | PROT_READ | PROT_WRITE , 
                                MAP_FIXED | MAP_SHARED, scm->fd , 0);
    
    if(MAP_FAILED == scm->mapped) {
        printf("error with base\n");
        printf("file size: %d\n", (int)scm->capacity);
        FREE(scm);
        return NULL;
    }

    scm->base = (void *)((char *)scm->mapped + INT_SIZE);
    size_info = (int *)scm->mapped;

    if((1 != size_info[0]) || truncate) {
        size_info[0] = 1;
        size_info[1] = 0;
        scm->utilized = 0;
    }

    scm->utilized = size_info[1];
    close(scm->fd);

    return scm;
}

void *scm_malloc(struct scm *scm, size_t n) {
    size_t *local_meta;
    void *addr;
    int *total_meta;

    if(scm == NULL || scm->base == MAP_FAILED || n==0) {
        return NULL;
    } 

    if(scm->utilized + n + METADATA_SIZE > scm->capacity) {
        printf("scm utili: %d\n", (int)scm->utilized);
        printf("scm cap: %d\n", (int)scm->capacity);
        printf("n: %d\n", (int)n);
        perror("Capacity exceeded\n");
        return NULL;
    }
    
    local_meta = (size_t *) (char *) scm->base + scm->utilized;
    local_meta[0] = 1;
    local_meta[1] = n;

    scm->utilized += n + METADATA_SIZE;

    total_meta = (int *)scm->mapped;
    total_meta[1] = scm->utilized;

    addr = (void *)((char *) local_meta + METADATA_SIZE);
    return addr;
}

char *scm_strdup(struct scm *scm, const char *s) {
    char *dup_str;
    int *total_meta;
    size_t *local_meta = (size_t *)((char *) scm->base + scm->utilized);

    if(s == NULL || scm == NULL || scm->base == MAP_FAILED) {
        return NULL;
    }

    local_meta[0] = 1;
    local_meta[1] = strlen(s) +1 ;
    dup_str = (char *)local_meta + METADATA_SIZE;

    strcpy(dup_str, s);
    scm->utilized += local_meta[1] + METADATA_SIZE;
    total_meta = (int *) scm->mapped;
    total_meta[1] = scm->utilized;

    return dup_str;
}

void scm_free(struct scm *scm, void *ptr) {
    size_t *local_meta;
    void *addr;
    void *base;
    int found = 0;

    addr = base = scm_mbase(scm);

    while (!found && (addr < (void *)((char *)base + scm->utilized))) {
        if (addr == ptr) {
            found = 1;
            break;
        }
        addr = (void *)((char *)addr + ((size_t *)addr - 2)[1] + METADATA_SIZE);
    }

    if(!found) return;
    
    local_meta = (size_t *) ptr - 2;
    local_meta[0] = 0;
}

size_t scm_utilized(const struct scm *scm) {
    if(scm == NULL) {
        return 0;
    }
    return scm->utilized;
}

size_t scm_capacity(const struct scm *scm) {
    if(scm == NULL) {
        return 0;
    }
    return scm->capacity;
}

void *scm_mbase(struct scm *scm) {
    if(scm == NULL) {
        return NULL;
    }
    
    if(0 != scm->utilized)
        return (void *)((size_t *) scm->base + 2);
    
    return scm->base;
}
