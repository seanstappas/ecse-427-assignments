#include "sfs_api.h"
#include "disk_emu.h"
#include <stdint.h>

#define MAGIC 260639512 // student id
#define BLOCK_SIZE 1024
#define NUM_DATA_BLOCKS 1024 // Not including super, FBM, WM
#define NUM_FILES 200
#define NUM_DIRECT_POINTERS 14
#define NUM_SHADOWS 4
#define FILENAME_LENGTH 10
#define DIRECTORY_ENTRY_LENGTH 16

typedef struct _inode_t { // total size of inode = 64 bytes
	int32_t size; // can have negative size (init to -1). Represents size of file, in bytes
	int32_t direct[NUM_DIRECT_POINTERS]; // init to -1
	int32_t indirect; // init to -1
} inode_t;

typedef struct _superblock_t {
	uint32_t magic; // 4 bytes long
	uint32_t block_size;
	uint32_t num_blocks; // Q: 1024 + 3 or 1024 (data blocks)?
	inode_t root;
	inode_t shadow[NUM_SHADOWS];
	int32_t last_shadow; // index of last shadow node? init to -1
} super_block_t;

typedef struct _block_t {
	unsigned char bytes[BLOCK_SIZE]; // use char since 1 char = 1 byte
} block_t;

typedef struct _cache_t {
	int read_pointers[NUM_DATA_BLOCKS];
	int write_pointers[NUM_DATA_BLOCKS];
	inode_t inodes[NUM_DATA_BLOCKS];

} cache_t;

typedef struct _directory_entry_t {
	char filename[FILENAME_LENGTH];
	int inode_index;
} directory_entry_t;

super_block_t *super; // Defines the file system geometry
block_t *fbm; // Unused data blocks (doesn't track super, fbm or wm) (value 1 = data block at that index is unused)
block_t *wm; // Writeable data blocks (value 1 = data block at that index is writeable)

cache_t *inode_cache; // in-memory cache of RW pointers + inodes

/*
	Formats the virtual disk and creates the SSFS file system on top of the disk.
	------------------------------------------------------------------------------------------------
	fresh: Flag to signal if the file system should be created from scratch. If false, the file
		   system is opened from the disk.
*/
void mkssfs(int fresh) {
	init_disk(fresh, "seanstappas");
	init_fbm_and_wm(); // What to do here if not fresh?
	init_super();
	// init file containing all i-nodes
	// setup root directory
}

void init_disk(int fresh, char *disk_name) {
	if (fresh)
		init_fresh_disk(disk_name, BLOCK_SIZE, NUM_DATA_BLOCKS + 3); // +3 for super, fbm, wm
	else
		init_disk(disk_name, BLOCK_SIZE, NUM_DATA_BLOCKS + 3);
}

void init_fbm_and_wm() {
	for (int i = 0; i < BLOCK_SIZE; ++i)
	{
		fbm->bytes[i] = 1; // Only need to use the LSB here, instead of 0xFF
		wm->bytes[i] = 1;
	}

	void *fbm_buf = calloc(1, BLOCK_SIZE);
	memcpy(fbm_buf, fbm, BLOCK_SIZE);
	write_blocks((NUM_DATA_BLOCKS - 2) * BLOCK_SIZE, 1, fbm_buf); // do calloc + memcpy here probably

	void *wm_buf = calloc(1, BLOCK_SIZE);
	memcpy(wm_buf, wm, BLOCK_SIZE);
	write_blocks((NUM_DATA_BLOCKS - 1) * BLOCK_SIZE, 1, wm);
}

void init_super() { // populate root j-node
	super->magic = MAGIC;
	super->block_size = BLOCK_SIZE;
	super->num_blocks = NUM_DATA_BLOCKS + 3;
	super->last_shadow = -1;
	inode_t *root;
	root->size = -1; // Q: Is the size of root = sum of all bytes or the number of i-nodes?
	root->indirect = -1;
	for (int i = 0; i < NUM_DIRECT_POINTERS; i++)
	{
		root->direct[i] = get_free_block(); // find free blocks for every direct pointer
	}
	super->root = root;

	void *super_buf = calloc(1, BLOCK_SIZE);
	memcpy(super_buf, root, BLOCK_SIZE);
	write_blocks(0, 1, super_buf);
}

int get_free_block() {
	for (int i = 0; i < BLOCK_SIZE; i++)
	{
		if (fbm->bytes[i] != 0) {  // Only need to use the LSB here
			fbm->bytes[i] = 0;
			return i;
		}
	}
	return -1;
}

/*
	Open the given file. If the file does not exist, a new file with size 0 is created. If it
	exists, read pointer is at the beginning of the file, and write pointer at the end (append
	mode).
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
