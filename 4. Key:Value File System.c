/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * logfs.c
 */

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "device.h"
#include "logfs.h"

/**
 * Needs:
 *   pthread_create()
 *   pthread_join()
 *   pthread_mutex_init()
 *   pthread_mutex_destroy()
 *   pthread_mutex_lock()
 *   pthread_mutex_unlock()
 *   pthread_cond_init()
 *   pthread_cond_destroy()
 *   pthread_cond_wait()
 *   pthread_cond_signal()
 *
 */

#define WCACHE_BLOCKS 32
#define RCACHE_BLOCKS 10
#define BLOCK_SIZE 4096

struct logfs
{
    struct device *device;
    uint64_t block_size;
    uint64_t file_filled;
    uint64_t write_buffer_size;
};

static struct
{
    char data[BLOCK_SIZE * WCACHE_BLOCKS];
    int head;
    int tail;
    pthread_t worker;
    pthread_mutex_t lock;
    pthread_cond_t space_avail, item_avail, save_disk;
    int done;
} buffer;

int buffer_used(struct logfs* logfs) {
    int tail = buffer.tail;
    int head = buffer.head;
    if (head == -1)
        return 0;
    int val = (tail >= head) ? (tail - head) : (((int) logfs->block_size - head) + tail);
    return val + 1;
}

int buffer_full(struct logfs* logfs) {
    return (buffer.tail + 1) % (int) logfs->block_size == buffer.head;
}

int buffer_empty() {
    return buffer.head == -1;
}

void logfs_close(struct logfs *logfs){
    buffer.done = 1;
    pthread_join(buffer.worker, NULL);

    pthread_mutex_destroy(&buffer.lock);
    pthread_cond_destroy(&buffer.item_avail);
    pthread_cond_destroy(&buffer.space_avail);
    device_close(logfs->device);
}

void *consumer(void *args) {
    struct logfs *logfs = (struct logfs *)args;
    while (!buffer.done) {
        pthread_mutex_lock(&buffer.lock);
        while (buffer_empty() || (int) logfs->block_size >= buffer_used(logfs)) {
            if (buffer.done)
                break;
            pthread_cond_wait(&buffer.item_avail, &buffer.lock);
        }

        if (buffer.done) {
            pthread_mutex_unlock(&buffer.lock);
            break;
        }

        char *item = (char *)malloc(sizeof(logfs->block_size));

        if(item == NULL) {
            printf("error with malloc item in consumer");
            EXIT(0);
        }

        for (int i = 0; i < (int) logfs->block_size; i++) {
            item[i] = buffer.data[(buffer.head + i) % logfs->write_buffer_size];
        }

        if (-1 == device_write(logfs->device, item, logfs->file_filled, (int) logfs->block_size)){
            TRACE("ERROR DEVICE WRITE");
            EXIT(0);
        }
        logfs->file_filled += (int) logfs->block_size;

        if (buffer.head == buffer.tail)
            buffer.head = buffer.tail = -1;
        else
            buffer.head = (buffer.head + (int) logfs->block_size) % logfs->write_buffer_size;

        if(buffer.tail == buffer.head || (buffer.head == 0 && buffer.tail == (int) logfs->write_buffer_size)){
            pthread_cond_signal(&buffer.save_disk);
        }

        pthread_cond_signal(&buffer.space_avail);
        pthread_mutex_unlock(&buffer.lock);
    }
    return 0;
}

struct logfs *logfs_open(const char *pathname){
    struct logfs *logfs = (struct logfs *)malloc(sizeof(struct logfs));
    if(logfs == NULL) {
        printf("error with malloc logfs\n");
        EXIT(0);
    }

    logfs->device = device_open(pathname);
    logfs->file_filled = 0;
    logfs->block_size = device_block(logfs->device);
    logfs->write_buffer_size = logfs->block_size * WCACHE_BLOCKS;

    buffer.done = 0;
    buffer.head = buffer.tail = -1;

    pthread_mutex_init(&buffer.lock, NULL);
    pthread_cond_init(&buffer.space_avail, NULL);
    pthread_cond_init(&buffer.item_avail, NULL);
    pthread_create(&buffer.worker, NULL, &consumer, (void *)logfs);
    return logfs;
}

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len) {
    char *temp = (char *)malloc(sizeof(10 * logfs->block_size));

    if(temp == NULL) {
        printf("error with malloc tempory buffer\n");
        EXIT(0);
    }
    uint64_t off_ = off - off % logfs->block_size;

    if(off > logfs->file_filled + buffer_used(logfs)) {
        TRACE("READ ERROR");
        FREE(temp);
        EXIT(0);
        return -1;
    } else if (off > logfs->file_filled){
        off_ = off % (int) logfs->block_size;
        memcpy(buf, buffer.data + buffer.head + off_, len);
    } else {
        if (-1 == device_read(logfs->device, temp, off_, RCACHE_BLOCKS * (int) logfs->block_size)){
            TRACE("READ ERROR");
            FREE(temp);
            EXIT(0);
            return -1;
        }
        memcpy(buf, (char *)temp + off % logfs->write_buffer_size, len);
    }
    FREE(temp);
    return 0;
}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len){
    char *item = (char *)buf;
    pthread_mutex_lock(&buffer.lock);
    while (buffer_full(logfs)) {
        pthread_cond_wait(&buffer.space_avail, &buffer.lock);
    }

    if (buffer_empty())
        buffer.head = 0;
    
    for (int i = 0; i < (int) len; i++) {
        buffer.tail = (buffer.tail + 1) % logfs->write_buffer_size;
        buffer.data[buffer.tail] = item[i];
    }
    pthread_cond_signal(&buffer.item_avail);
    pthread_mutex_unlock(&buffer.lock);
    return 0;
}
