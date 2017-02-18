#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int kv_store_create(char *name) {

}

int kv_store_write(char *key, char *value) {

}

char *kv_store_read(char *key) {

}

char **kv_store_read_all(char *key) {

}

/*
The program listing below shows a small example program where the string provided as the first
argument is copied into the shared memory. The shared memory is created under the name “myshared.”
Obviously, you need to use a unique name that is related to your login name so that there are no name
conflicts even if you happen to use a lab machine for running the program. The mmap() is used to map
the shared memory object starting at an address. By specifying a NULL value for the first argument of the
mmap(), we are letting the kernel pick the starting location for the shared memory object in the virtual
address space. The example below shows a program for reading the contents in the shared memory object and
writing it to the standard output. Obviously, the name of the shared memory object should match the one
in the above program. The fstat() system call allows us to determine the length of the shared memory
object. This length is used in the mmap() so that we can map only that portion into the virtual address
space.
*/
void setup(char *str) {
	int fd = shm_open("myshared", O_CREAT | ORDWR, S_IRWXU);

	char *addr = mmap(NULL, strlen(str), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ftruncate(fd, strlen(str));
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
void read() {
	int fd = shm_open("myshared", O_RDWR, 0);
	if (fd < 0)
		printf("Error.. opening shm\n");

	if (fstat(fd, &s) == -1)
		printf("Error fstat\n");

	char *addr = mmap(NULL, s.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
}

int main(int argc, char **argv) {
	setup(argv[1]);
	read();
}