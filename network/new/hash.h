#ifndef __HASH_H__
#define __HASH_H__

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "const.h"

#define NUM_BUCKETS 100

typedef struct {
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
    void *next;
} Entries;

typedef struct {
    Entries *entries;
    pthread_mutex_t *lock;
} Bucket;

typedef struct {
    Bucket buckets[NUM_BUCKETS];
} Hash;

void hash_init(Hash* table);
void hash_free(Hash* table);
void put(char* key, char* value, Hash* table);
void get(char* key, char* value, Hash* table);

#endif