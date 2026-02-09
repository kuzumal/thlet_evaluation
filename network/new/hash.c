#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "const.h"
#include "hash.h"

static void free_iter(Entries* entries) {
    if (!entries) return;
    free_iter(entries->next);
    free(entries);
}

void hash_init(Hash* table) {
    for (int i = 0; i < NUM_BUCKETS; i ++) {
        Bucket *bucket = &(table->buckets[i]);
        bucket->entries = NULL;
        bucket->lock = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(bucket->lock, NULL);
    }
}

void hash_free(Hash* table) {
    for (int i = 0; i < NUM_BUCKETS; i ++) {
        Bucket *bucket = &(table->buckets[i]);
        free_iter(bucket->entries);
        bucket->entries = NULL;
        free(bucket->lock);
    }
}

static size_t hash(char* key) {
    size_t res = 0;
    for (int i = 0; key[i] != '\0'; i ++) {
        res += key[i] * i * 10;
    }
    return res % NUM_BUCKETS;
}

void put(char* key, char* value, Hash* table) {
    assert (key && value && table);
    Bucket *bucket = &(table->buckets[hash(key)]);

    pthread_mutex_lock(bucket->lock);

    if (bucket->entries == NULL) {
        Entries *entries = malloc(sizeof(Entries));
        bucket->entries = entries;
        entries->next = NULL;
    }
    strncpy(bucket->entries->key, key, KEY_SIZE);
    strncpy(bucket->entries->value, value, VALUE_SIZE);

    pthread_mutex_unlock(bucket->lock);
}

void get(char* key, char* value, Hash* table) {
    assert (key && table);
    Bucket *bucket = &(table->buckets[hash(key)]);

    pthread_mutex_lock(bucket->lock);

    if (bucket->entries == NULL) {
        pthread_mutex_unlock(bucket->lock);
        return;
    }
    strncpy(value, bucket->entries->value, VALUE_SIZE);

    pthread_mutex_unlock(bucket->lock);
}