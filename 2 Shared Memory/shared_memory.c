#define _XOPEN_SOURCE 700
//#define _BSD_SOURCE

#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define STORE_SIZE 1000 // number of key-value pairs
#define KEY_VALUE_PAIR_SIZE 100
#define NUMBER_OF_PODS 100

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
typedef int semaphore; /* use your imagination */
semaphore mutex = 1; /* controls access to rc */
semaphore db = 1; /* controls access to the database */
int rc = 0; /* # of processes reading or wanting to */
void reader(void)
{
	while (TRUE) { /* repeat forever */
		down(&mutex); /* get exclusive access to rc */
		rc = rc + 1; /* one reader more now */
		if (rc == 1) down(&db); /* if this is the first reader ... */
		up(&mutex); /* release exclusive access to rc */
		read data base(); /* access the data */
		down(&mutex); /* get exclusive access to rc */
		rc = rc −1; /* one reader few er now */
		if (rc == 0) up(&db); /* if this is the last reader ... */
		up(&mutex); /* release exclusive access to rc */
		use data read(); /* noncr itical region */
	}
}
void writer(void)
{
	while (TRUE) { /* repeat forever */
		think up data(); /* noncr itical region */
		down(&db); /* get exclusive access */
		wr ite data base(); /* update the data */
		up(&db); /* release exclusive access */
	}
}

/*
The kv_store_create() function creates a store if it is not yet created or opens the store if it is
already created. After a successful call to the function, the calling process should have access to the store.
This function could fail, if the system does not enough memory to create another store, or the user does
not have proper permissions. In that case, the function should return -1. A successful creation should
result in a 0 return value. The creation function could set a maximum size for the key-value store, where
the size is measured in terms of the number of key-value pairs in the store.
*/
int kv_store_create(char *name) {
	int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU);
	if (fd == -1)
		return -1;

	char *addr = mmap(NULL, STORE_SIZE * KEY_VALUE_PAIR_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		perror("mmap failed");
		return -1;
	}

	int status = ftruncate(fd, STORE_SIZE * KEY_VALUE_PAIR_SIZE); // Needed to set length of new shared memory object (initially 0)
	if (status == -1) {
		perror("ftruncate failed");
		return -1;
	}
	close(fd);

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
*/
int kv_store_write(char *key, char *value) {
	int pod_index = hash(key) % NUMBER_OF_PODS;

}

/*
The kv_store_read() function takes a key and searches the store for the key-value pair. If found, it
returns a copy of the value. It duplicates the string found in the store and returns a pointer to the string. It
is the responsibility of the calling function to free the memory allocated for the string. If no key-value pair
is found, a NULL value is returned.
*/
char *kv_store_read(char *key) {
	int pod_index = hash(key) % NUMBER_OF_PODS;
}

/*
The kv_store_read_all() function takes a key and returns all the values in the store. A NULL is
returned if there is no records for the key.
*/
char **kv_store_read_all(char *key) {

}

/*
The program listing below shows a small example program where the string provided as the first
argument is copied into the shared memory. The shared memory is created under the name “myshared.”
Obviously, you need to use a unique name that is related to your login name so that there are no name
conflicts even if you happen to use a lab machine for running the program. The mmap() is used to map
the shared memory object starting at an address. By specifying a NULL value for the first argument of the
mmap(), we are letting the kernel pick the starting location for the shared memory object in the virtual
address space. The ftruncate() call is used to resize the shared memory object to fit the string (first 
argument). As the last statement we copy the bytes into the shared memory region.
*/
void setup_memory(char *str) {
	int fd = shm_open("/seanstappas", O_CREAT | O_RDWR, S_IRWXU);

	char *addr = mmap(NULL, strlen(str), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ftruncate(fd, strlen(str)); // Needed to set length of new shared memory object (initially 0)
	close(fd);

	memcpy(addr, str, strlen(str));
}


/*
The example below shows a program for reading the contents in the shared memory object and
writing it to the standard output. Obviously, the name of the shared memory object should match the one
in the above program. The fstat() system call allows us to determine the length of the shared memory
object. This length is used in the mmap() so that we can map only that portion into the virtual address
space.
*/
void read_memory() {
	struct stat s;

	int fd = shm_open("/seanstappas", O_RDWR, 0); // mode set to 0 because we don't care (not creating)
	if (fd < 0)
		printf("Error.. opening shm\n");

	if (fstat(fd, &s) == -1)
		printf("Error fstat\n");

	char *addr = mmap(NULL, s.st_size, PROT_READ, MAP_SHARED, fd, 0);

	printf("Memory: %s\n", addr);

	close(fd);
}

int main(int argc, char **argv) {
	setup_memory(argv[1]);
	read_memory();
}