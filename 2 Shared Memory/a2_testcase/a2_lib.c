#include "a2_lib.h"

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

#define MAX_KEY_SIZE 32    // max size of key (excluding null termination)
#define MAX_VALUE_SIZE 256 // max size of value (excluding null termination)
#define POD_CAPACITY 256   // max number of elements in pod
#define NUMBER_OF_PODS 256 // total number of pods

typedef struct {
	char keys[NUMBER_OF_PODS][POD_CAPACITY][MAX_KEY_SIZE + 1]; // keys in the store, indexed by pod number and index within pod
	char values[NUMBER_OF_PODS][POD_CAPACITY][MAX_VALUE_SIZE + 1]; // values in the store, indexed by pod number and index within pod
	int last_read_indices[NUMBER_OF_PODS][POD_CAPACITY]; // last read index for each key (in every pod)
	int last_write_indices[NUMBER_OF_PODS]; // keeps track of the last written kv pair
	int current_pod_sizes[NUMBER_OF_PODS]; // current size of every pod (better read performance with this)
	int number_of_readers[NUMBER_OF_PODS]; // needed to implement Readers & Writers algorithm within each pod
} SharedMemory;

SharedMemory *shared_memory;
sem_t *mutexes[NUMBER_OF_PODS];
sem_t *dbs[NUMBER_OF_PODS];
char *db_name;

const char *SEMAPHORE_MUTEX_PREFIX = "/seanstappas_mutex_";
const char *SEMAPHORE_DB_PREFIX = "/seanstappas_db_";

/*
	Simple hash function. Taken from:
	http://www.cse.yorku.ca/~oz/hash.html
*/
unsigned long hash(char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

/*
	Creates a shared memory store if not yet created or opens if already created.

	name: The name of the shared memory.

	Returns: 0 on success, -1 on failure.
*/
int kv_store_create(char *name) {
	if (name == NULL)
		return -1;

	db_name = name;

	int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU);
	if (fd == -1) {
		perror("shm_open failed");
		return -1;
	}

	shared_memory = (SharedMemory*) mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shared_memory == MAP_FAILED) {
		perror("mmap failed");
		return -1;
	}

	int status = ftruncate(fd, sizeof(SharedMemory));
	if (status == -1) {
		perror("ftruncate failed");
		return -1;
	}
	close(fd);

	for (int i = 0; i < NUMBER_OF_PODS; i++) {
		for (int j = 0; j < POD_CAPACITY; j++) {
			shared_memory->last_read_indices[i][j] = -1;	
		}
		char mutex_semaphore_name[22];
		sprintf(mutex_semaphore_name, "%s%d", SEMAPHORE_MUTEX_PREFIX, i);
		mutexes[i] = sem_open(mutex_semaphore_name, O_CREAT, S_IRWXU, 1);
		if (mutexes[i] == SEM_FAILED) {
			perror("sem_open mutex failed");
			return -1;
		}

		char db_semaphore_name[22];
		sprintf(db_semaphore_name, "%s%d", SEMAPHORE_DB_PREFIX, i);
		dbs[i] = sem_open(db_semaphore_name, O_CREAT, S_IRWXU, 1);
		if (dbs[i] == SEM_FAILED) {
			perror("sem_open db failed");
			return -1;
		}
	}

	return 0;
}

/*
	Writes key/value pair to the shared memory store.

	key: The key to be written.
	value: The value to be written.

	Returns: 0 on success, -1 on failure.
*/
int kv_store_write(char *key, char *value) {
	if (key == NULL || value == NULL)
		return -1;

	int pod_number = hash(key) % NUMBER_OF_PODS;

	// Entry protocol
	sem_t *db = dbs[pod_number];
	sem_wait(db);

	// Critical section
	int current_pod_size = shared_memory->current_pod_sizes[pod_number];
	if (current_pod_size < POD_CAPACITY)
		shared_memory->current_pod_sizes[pod_number] = (current_pod_size + 1);
	int key_value_index = shared_memory->last_write_indices[pod_number];
	if (current_pod_size != 0)
		key_value_index = (key_value_index + 1) % POD_CAPACITY; // wrap-around (overwriting if necessary)
	shared_memory->last_write_indices[pod_number] = key_value_index;
	strncpy(shared_memory->keys[pod_number][key_value_index], key, MAX_KEY_SIZE);
	strncpy(shared_memory->values[pod_number][key_value_index], value, MAX_VALUE_SIZE);

	for (int i = 0; i < current_pod_size; i++) {
		int last_read_index = shared_memory->last_read_indices[pod_number][i];
		if (last_read_index == key_value_index) { // Remove references to current index
			 shared_memory->last_read_indices[pod_number][i] = -1;
		}
	}

	// Exit protocol
	sem_post(db);

	return 0;
}

/*
	Read by key from the shared memory store.

	key: The key to be read.

	Returns: The value associated with the given key on success, NULL on failure.
*/
char *kv_store_read(char *key) {
	if (key == NULL)
		return NULL;

	int pod_number = hash(key) % NUMBER_OF_PODS;

	// Entry protocol
	sem_t *mutex = mutexes[pod_number];
	sem_wait(mutex);
	int rc = shared_memory->number_of_readers[pod_number];
	rc++;
	shared_memory->number_of_readers[pod_number] = rc;
	sem_t *db = dbs[pod_number];
	if (rc == 1) {
		sem_wait(db);
	}
	sem_post(mutex);

	// Critical section
	char *return_value = NULL;
	int current_pod_size = shared_memory->current_pod_sizes[pod_number];
	if (current_pod_size > 0) {
		int last_write_index = shared_memory->last_write_indices[pod_number];
		int key_value_index = (last_write_index + 1) % current_pod_size;

		int last_read_index = -1;
		char *store_key;
		for (int i = 0; i < current_pod_size; i++) {
			store_key = shared_memory->keys[pod_number][i];
			if (strcmp(key, store_key) == 0) {
				last_read_index = shared_memory->last_read_indices[pod_number][i];
				if (last_read_index != -1)
					break;
			}
		}

		if (last_read_index != -1) {
			key_value_index = (last_read_index + 1) % current_pod_size; // This is needed to cycle through all duplicate keys
		}
		int searching = 1; // Searching for first occurence of key
		int first_index = -1;
		for (int i = 0; i < current_pod_size; i++) {
			store_key = shared_memory->keys[pod_number][key_value_index];
			if (strcmp(key, store_key) == 0) {
				if (searching) {
					first_index = key_value_index;
					char *store_value = shared_memory->values[pod_number][key_value_index];
					return_value = malloc(sizeof(char) * MAX_VALUE_SIZE + 1);
					strncpy(return_value, store_value, MAX_VALUE_SIZE);
					searching = 0;
				}
				shared_memory->last_read_indices[pod_number][key_value_index] = first_index;
			}
			key_value_index = (key_value_index + 1) % current_pod_size; // work your way forwards through indices (FIFO)
		}
	}

	// Exit protocol
	sem_wait(mutex);
	rc = shared_memory->number_of_readers[pod_number];
	rc--;
	shared_memory->number_of_readers[pod_number] = rc;
	if (rc == 0) {
		sem_post(db);
	}
	sem_post(mutex);

	return return_value;
}

/*
	Read all values associated with given key in the shared memory store.

	key: The key to be read.

	Returns: All values associated with key on success, NULL on failure.
*/
char **kv_store_read_all(char *key) {
	if (key == NULL || strlen(key) > MAX_KEY_SIZE)
		return NULL;

	int pod_number = hash(key) % NUMBER_OF_PODS;

	// Entry protocol
	sem_t *mutex = mutexes[pod_number];
	sem_wait(mutex);
	int rc = shared_memory->number_of_readers[pod_number];
	rc++;
	shared_memory->number_of_readers[pod_number] = rc;
	sem_t *db = dbs[pod_number];
	if (rc == 1) {
		sem_wait(db);
	}
	sem_post(mutex);

	// Critical section
	char **values = NULL;
	int current_pod_size = shared_memory->current_pod_sizes[pod_number];
	if (current_pod_size > 0) {
		int number_of_values = 1;
		for (int i = 0; i < current_pod_size; i++) {
			char *current_key = shared_memory->keys[pod_number][i];
			if (strcmp(key, current_key) == 0) {
				number_of_values++;
			}
		}
		if (number_of_values > 0) {
			values = malloc(number_of_values * (MAX_VALUE_SIZE + 1)); // + 1 for null termination
			int last_write_index = shared_memory->last_write_indices[pod_number];
			int read_index = (last_write_index + 1) % current_pod_size;
			int j = 0;
			for (int i = 0; i < current_pod_size && j < number_of_values; i++) {
				char *store_key = shared_memory->keys[pod_number][read_index];
				if (strcmp(key, store_key) == 0) {
					char *store_value = shared_memory->values[pod_number][read_index];
					values[j] = malloc(sizeof(char) * MAX_VALUE_SIZE + 1);
					strncpy(values[j], store_value, MAX_VALUE_SIZE);
					j++;
				}
				read_index = (read_index + 1) % current_pod_size;
			}
			values[number_of_values - 1] = NULL;
		}
	}

	// Exit protocol
	sem_wait(mutex);
	rc = shared_memory->number_of_readers[pod_number];
	rc--;
	shared_memory->number_of_readers[pod_number] = rc;
	if (rc == 0) {
		sem_post(db);
	}
	sem_post(mutex);

	return values;
}

/*
	Unmaps and unlinks the shared memory. Also deletes all named semaphores.

	Returns: 0 on success, -1 on failure.
*/
int kv_delete_db() {
	if (munmap(shared_memory, sizeof(SharedMemory)) == -1) {
		perror("munmap failed");
		return -1;
	}
	if (shm_unlink(db_name) == -1) {
		perror("shm_unlink failed");
		return -1;
	}
	for (int i = 0; i < NUMBER_OF_PODS; i++) {
		if (sem_close(mutexes[i]) == -1) {
			perror("sem_close mutex failed");
			return -1;
		}
		if (sem_close(dbs[i]) == -1) {
			perror("sem_close db failed");
			return -1;
		}
		char mutex_semaphore_name[22];
		sprintf(mutex_semaphore_name, "%s%d", SEMAPHORE_MUTEX_PREFIX, i);
		if (sem_unlink(mutex_semaphore_name) == -1) {
			perror("sem_unlink mutex failed");
			return -1;
		}
		char db_semaphore_name[22];
		sprintf(db_semaphore_name, "%s%d", SEMAPHORE_DB_PREFIX, i);
		if (sem_unlink(db_semaphore_name) == -1) {
			perror("sem_unlink db failed");
			return -1;
		}
	}
	return 0;
}