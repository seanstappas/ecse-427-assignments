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

const char *SEMAPHORE_MUTEX_PREFIX = "/seanstappas_mutex_";
const char *SEMAPHORE_DB_PREFIX = "/seanstappas_db_";

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

/*
Simple hash function. Taken from:
http://www.cse.yorku.ca/~oz/hash.html
*/
unsigned long hash(unsigned char *str)
{
	unsigned long hash = 5381;
	int c;

	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

/*
Readers and writers problem (Section 2.5.2 of textbook)
*/
//typedef int semaphore; /* use your imagination */
//semaphore mutex = 1; /* controls access to rc */
//semaphore db = 1; /* controls access to the database */
//int rc = 0; /* # of processes reading or wanting to */
//void reader(void)
//{
	//while (TRUE) { /* repeat forever */
	//	down(&mutex); /* get exclusive access to rc */
	//	rc = rc + 1; /* one reader more now */
	//	if (rc == 1) down(&db); /* if this is the first reader ... */
	//	up(&mutex); /* release exclusive access to rc */
	//	read data base(); /* access the data */
	//	down(&mutex); /* get exclusive access to rc */
	//	rc = rc −1; /* one reader few er now */
	//	if (rc == 0) up(&db); /* if this is the last reader ... */
	//	up(&mutex); /* release exclusive access to rc */
	//	use data read(); /* noncr itical region */
	//}
//}
//void writer(void)
//{
	//while (TRUE) { /* repeat forever */
	//	think up data(); /* noncr itical region */
	//	down(&db); /* get exclusive access */
	//	wr ite data base(); /* update the data */
	//	up(&db); /* release exclusive access */
	//}
//}

/*
The kv_store_create() function creates a store if it is not yet created or opens the store if it is
already created. After a successful call to the function, the calling process should have access to the store.
This function could fail, if the system does not enough memory to create another store, or the user does
not have proper permissions. In that case, the function should return -1. A successful creation should
result in a 0 return value. The creation function could set a maximum size for the key-value store, where
the size is measured in terms of the number of key-value pairs in the store.
*/
int kv_store_create(char *name) {
	if (name == NULL)
		return -1;

	int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU);
	if (fd == -1) {
		perror("shm_open failed");
		return -1;
	}

	shared_memory = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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

	// Semaphore creation
	for (int i = 0; i < NUMBER_OF_PODS; i++) {
		last_read_indices[i] = -1;

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
The kv_store_write() function takes a key-value pair and writes them to the store. The key and
value strings can be length limited. For instance, you can limit the key to 32 characters and value to 256
characters. If a longer string is provided as input, you need to truncate the strings to fit into the maximum
placeholders. If the store is already full, the store needs to evict an existing entry to make room for the
new one. In addition to storing the key-value pair in the memory, this function needs to update an index
that is also maintained in the store so that the reads looking for an key-value pair can be completed as fast
as possible.
Returns 0 on success, -1 on failure.
*/
int kv_store_write(char *key, char *value) {
	if (key == NULL || value == NULL)
		return -1;

	if (strlen(key) > MAX_KEY_SIZE) {
		printf("Max key size is %d", MAX_KEY_SIZE);
		return -1;
	}

	if (strlen(value) > MAX_VALUE_SIZE) { // TODO: truncate only the value? or value and key?
		printf("Max value size is %d", MAX_VALUE_SIZE);
		return -1;
	}

	int pod_number = hash(key) % NUMBER_OF_PODS;

	// Entry protocol
	sem_t *db = dbs[pod_number];
	sem_wait(db);

	// Critical section
	int current_pod_size = shared_memory->current_pod_sizes[pod_number];
	if (current_pod_size < POD_SIZE)
		shared_memory->current_pod_sizes[pod_number] = (current_pod_size + 1);
	int key_value_index = shared_memory->last_write_indices[pod_number];
	if (current_pod_size != 0)
		key_value_index = (key_value_index + 1) % POD_SIZE; // wrap-around (overwriting if necessary)
	shared_memory->last_write_indices[pod_number] = key_value_index;
	strcpy(shared_memory->keys[pod_number][key_value_index], key);
	strcpy(shared_memory->values[pod_number][key_value_index], value);
	last_read_indices[pod_number] = -1; // reset read iteration order (needed if read/write is interleaved)

	// Exit protocol
	sem_post(db);

	return 0;
}

/*
The kv_store_read() function takes a key and searches the store for the key-value pair. If found, it
returns a copy of the value. It duplicates the string found in the store and returns a pointer to the string. It
is the responsibility of the calling function to free the memory allocated for the string. If no key-value pair
is found, a NULL value is returned.
*/
char *kv_store_read(char *key) { // TODO: Handle duplicates...
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
	int last_write_index = shared_memory->last_write_indices[pod_number];
	int key_value_index = (last_write_index + 1) % current_pod_size;
	int last_read_index = last_read_indices[pod_number];
	if (last_read_index != -1) {
		char *last_read_key = shared_memory->keys[pod_number][last_read_index];
		if (strcmp(key, last_read_key) == 0)
			key_value_index = (last_read_index + 1) % current_pod_size; // This is needed to cycle through all duplicate keys
		else
			last_read_indices[pod_number] = -1;
	}
	char *store_key;
	for (int i = 0; i < current_pod_size; i++) {
		store_key = shared_memory->keys[pod_number][key_value_index];
		if (strcmp(key, store_key) == 0) {
			char *store_value = shared_memory->values[pod_number][key_value_index];
			last_read_indices[pod_number] = key_value_index;
			return_value = strdup(store_value);
			break;
		}
		key_value_index = (key_value_index + 1) % current_pod_size; // work your way forwards through indices (FIFO)
	}

	// Exit protocol
	sem_wait(mutex);
	rc = shared_memory->number_of_readers[pod_number];
	rc--;
	shared_memory->number_of_readers[pod_number] = rc;
	if (rc == 0)
		sem_post(db);
	sem_post(mutex);

	return return_value;
}

/*
The kv_store_read_all() function takes a key and returns all the values in the store. A NULL is
returned if there is no records for the key.
Returns 0 on success, -1 on failure.
*/
char **kv_store_read_all(char *key) {
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
	if (rc == 1)
		sem_wait(db);
	sem_post(mutex);

	// Critical section
	int current_pod_size = shared_memory->current_pod_sizes[pod_number];
	int number_of_values = 0;
	for (int i = 0; i < current_pod_size; i++) {
		char *current_key = shared_memory->keys[pod_number][i];
		if (strcmp(key, current_key) == 0) {
			number_of_values++;
		}
	}
	char **values = malloc(number_of_values * MAX_VALUE_SIZE);
	int last_write_index = shared_memory->last_write_indices[pod_number];
	int read_index = (last_write_index + 1) % current_pod_size;
	last_read_indices[pod_number] = -1; // Reset read index (to get full FIFO list)
	int j = 0;
	for (int i = 0; i < current_pod_size && j < number_of_values; i++) {
		char *store_key = shared_memory->keys[pod_number][read_index];
		if (strcmp(key, store_key) == 0) {
			char *store_value = shared_memory->values[pod_number][read_index];
			values[j] = strdup(store_value);
			j++;
		}
		read_index = (read_index + 1) % current_pod_size;
	}

	// Exit protocol
	sem_wait(mutex);
	rc = shared_memory->number_of_readers[pod_number];
	rc--;
	shared_memory->number_of_readers[pod_number] = rc;
	if (rc == 0)
		sem_post(db);
	sem_post(mutex);

	return values;
}

/*
Unmaps and unlinks the shared memory. Also deletes all named semaphores.
*/
int kv_delete_db(char *name) {
	if (munmap(shared_memory, sizeof(SharedMemory)) == -1) {
		perror("munmap failed");
		return -1;
	}
	if (shm_unlink(name) == -1) {
		perror("unlink status failed");
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


/*
* Handler for the SIGINT signal (Ctrl-C).
*/

static void handlerSIGINT(int sig) {
	if (sig == SIGINT) {
		kv_delete_db("/seanstappas");
	}
}

void infinite_write() {
	while (1) {
		kv_store_write("key1", "value1");
		printf("Write key1 value1\n");
		sleep(1);
	}
}

void infinite_read() {
	while (1) {
		char *value = kv_store_read("key1");
		printf("Read key1: %s\n", value);
		free(value);
		sleep(1);
	}
}


void test_all() {
	char *value;

	kv_store_write("key1", "value1");
	printf("Write key1 -> value1\n");
	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	kv_store_write("key1", "value2");
	printf("Write key1 -> value2\n");
	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	kv_store_write("key1", "value3");
	printf("Write key1 -> value3\n");
	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	kv_store_write("key1", "value4");
	printf("Write key1 -> value4\n");
	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	value = kv_store_read("key1");
	printf("Read key1: %s\n", value);
	free(value);

	char **all_values = kv_store_read_all("key1");
	for (int i = 0; i < POD_SIZE; i++) {
		if (all_values[i] == NULL)
			break;
		printf("read_all %d: %s\n", i, all_values[i]);
		free(all_values[i]);
	}

	free(all_values);
}

int main(int argc, char **argv) { // TODO: Remove this in final code!
	if (signal(SIGINT, handlerSIGINT) == SIG_ERR) { // Handle Ctrl-C interrupt
		printf("ERROR: Could not bind SIGINT signal handler\n");
		exit(EXIT_FAILURE);
	}
	kv_store_create("/seanstappas");

	//infinite_write();
	//infinite_read();
	test_all();

	kv_delete_db("/seanstappas");
}