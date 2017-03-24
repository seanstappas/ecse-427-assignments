#include "disk_emu.h"

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 20
#define NUM_FILES 200
#define NUM_DIRECT_POINTERS 14
#define NUM_SHADOWS 4

typedef struct _inode_t {
	int size;
	int direct[NUM_DIRECT_POINTERS];
	int indirect;
} inode_t;

typedef struct _superblock_t {
	unsigned char magic[4];
	int bsize;
	// ...
	inode_t root;
	inode_t shadow[NUM_SHADOWS];
	int last_shadow;
} super_block_t;

typedef struct _block_t {
	unsigned char bytes[BLOCK_SIZE];
} block_t;

typedef struct _cache_t
{
	int read_pointers[NUM_BLOCKS];
	int write_pointers[NUM_BLOCKS];
	inode_t inodes[NUM_BLOCKS];

} cache_t;

block_t super, fbm, wm;
cache_t inode_cache;

/*
	Formats the virtual disk and creates the SSFS file system on top of the disk.
	------------------------------------------------------------------------------------------------
	fresh: Flag to signal if the file system should be created from scratch. If false, the file
		   system is opened from the disk.
*/
void mkssfs(int fresh) {
	char *filename = "seanstappas";
	init_fresh_disk(filename, BLOCK_SIZE, NUM_BLOCKS);
	init_fbm_and_wm();
	init_super();
}

void init_fbm_and_wm() {
	for (int i = 0; i < BLOCK_SIZE; ++i)
	{
		fbm->bytes[i] = 0xFF;
		wm->bytes[i] = 0xFF;
	}
	write_blocks((NUM_BLOCKS - 2) * BLOCK_SIZE, 1, fbm); // do calloc + memcpy here probably
	write_blocks((NUM_BLOCKS - 1) * BLOCK_SIZE, 1, wm);
}

void init_super() { // populate root j-node
	for (int i = 0; i < NUM_DIRECT_POINTERS; i++)
	{
		int index = get_free_block();
		super->root->size = -1;
	}
	write_blocks(0, 1, super);
}

int get_free_block() {
	for (int i = 0; i < BLOCK_SIZE; i++)
	{
		if (fbm->bytes[i] != 0) {
			fbm->bytes[i] = 0;
			return i;
		}
	}
	return -1;
}

/*
	Open the given file. If the file does not exist, a new file with size 0 is created.
	------------------------------------------------------------------------------------------------
	name: Name of the file to be opened

	Returns: An integer corresponding to the index of the entry for the opened file in the file
			 descriptor table.
*/
int ssfs_fopen(char *name) {
	// Access root directory
	// Find inode
	// Copy inode to OFD table
	// Return read/write pointer
}

/*
	Closes the given file. The corresponding entry is removed from the open file descriptor table.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).

	Returns: 0 on success, -1 on failure.
*/
int ssfs_fclose(int fileID) {
	
}        

/*
	Move the read pointer to the given location in the file.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).
	loc:	The location to move the read pointer to.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_frseek(int fileID, int loc) {
	
}

/*
	Move the write pointer to the given location in the file.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).
	loc:	The location to move the write pointer to.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_fwseek(int fileID, int loc) {
	
}

/*
	Write characters into a file on the disk, starting from the write pointer of the current file.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).
	buf:	The characters to be written into the file.
	length: The number of bytes to be written.

	Returns: The number of bytes written.
*/
int ssfs_fwrite(int fileID, char *buf, int length) {

}

/*
	Read characters from a file on disk to a buffer, starting from the read pointer of the current
	file.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).
	buf:	The characters to be read from the file.
	length: The number of bytes to be read.

	Returns: The number of bytes read.
*/
int ssfs_fread(int fileID, char *buf, int length) {

}

/*
	Removes a file from the filesystem. The file is removed from the directory entry, the i-node
	entry is released, and the data blocks used by the file are released.
	------------------------------------------------------------------------------------------------
	file: The file name.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_remove(char *file) {
	
}         

/*
	Create a shadow of the file system. The newly added blocks become read-only.
	------------------------------------------------------------------------------------------------
	Returns: The index of the shadow root that holds the previous commit.
*/
int ssfs_commit() {
	
}                   

/*
	Restore the file system to a previous shadow (state).
	------------------------------------------------------------------------------------------------
	cnum: The commit number to restore to.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_restore(int cnum) {
	
}          
