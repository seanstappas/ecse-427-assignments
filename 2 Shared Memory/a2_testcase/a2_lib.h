#ifndef __A2_SEANSTAPPAS__
#define __A2_SEANSTAPPAS__

#define _XOPEN_SOURCE 700

#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <signal.h>

#define MAX_KEY_SIZE 32
#define MAX_VALUE_SIZE 256
#define POD_SIZE 256
#define NUMBER_OF_PODS 256 // k pods. could be a multiple of 16

typedef struct {
	char keys[NUMBER_OF_PODS][POD_SIZE][MAX_KEY_SIZE];
	char values[NUMBER_OF_PODS][POD_SIZE][MAX_VALUE_SIZE];
	int last_write_indices[NUMBER_OF_PODS]; // keeps track of the last written kv pair
	int current_pod_sizes[NUMBER_OF_PODS]; // current size of every pod (better read performance with this)
	int number_of_readers[NUMBER_OF_PODS];
} SharedMemory;

SharedMemory *shared_memory;
int last_read_indices[NUMBER_OF_PODS]; // keeps track of the last read index.
sem_t *mutexes[NUMBER_OF_PODS];
sem_t *dbs[NUMBER_OF_PODS]; // multiple writers can write to different pods (as long as each writer is in different pod)!!
char *db_name;

unsigned long hash(char *str);
int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);
int kv_delete_db();

#endif
