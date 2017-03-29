#include "sfs_api.h"
#include "disk_emu.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAGIC 260639512 // student id
#define BLOCK_SIZE 1024
#define NUM_DATA_BLOCKS 1024 // Not including super, FBM, WM
#define NUM_FILES 200 // Number of file i-nodes. Not including super. Set upper bound.
#define NUM_INODES_PER_BLOCK 15
#define NUM_DIRECTORY_ENTRIES_PER_BLOCK 60
#define NUM_DIRECT_POINTERS 14
#define NUM_SHADOWS 4
#define MAX_FILENAME_LENGTH 10
#define DIRECTORY_ENTRY_LENGTH 16

typedef struct _inode_t { // total size of inode = 64 bytes
	int32_t size; // can have negative size (init to -1). Represents size of file, in bytes
	int32_t direct[NUM_DIRECT_POINTERS]; // init to -1
	int32_t indirect; // init to -1. Only used for large files
} inode_t;

typedef struct _inode_block_t { // A block of inodes
	inode_t inodes[NUM_INODES_PER_BLOCK];
} inode_block_t;

typedef struct _superblock_t {
	uint32_t magic; // 4 bytes long
	uint32_t block_size;
	uint32_t num_blocks; // Q: 1024 + 3 or 1024 (data blocks)?
	uint32_t num_inodes;
	inode_t root;
	inode_t shadow[NUM_SHADOWS];
	int32_t last_shadow; // index of last shadow node? init to -1
} super_block_t;

typedef struct _block_t {
	unsigned char bytes[BLOCK_SIZE]; // use char since 1 char = 1 byte
} block_t;

typedef struct _ofd_table_t { // Q: inode index here, or actually copy the inode?
	int read_pointers[NUM_FILES];
	int write_pointers[NUM_FILES];

} ofd_table_t;

typedef struct _directory_entry_t { // fits within 16 bytes (actually 14)
	uint32_t inode_index; // 4 bytes
	char filename[MAX_FILENAME_LENGTH]; // 10 bytes
} directory_entry_t;

typedef struct _directory_block_t { // A block of directory entries (root directory)
	directory_entry_t directory_entries[NUM_DIRECTORY_ENTRIES_PER_BLOCK];
} directory_block_t;

super_block_t super; // Defines the file system geometry
block_t fbm; // Unused data blocks (doesn't track super, fbm or wm) (value 1 = data block at that index is unused)
block_t wm; // Writeable data blocks (value 1 = data block at that index is writeable). Not necessary for simple system (without shadowing)
ofd_table_t ofd_table; // Open File Descriptor Table (in-memory cache of RW pointers + inodes).
directory_entry_t directory_cache[NUM_FILES]; // Cache of all filenames

void write_single_block(int start_address, void *data) {
	void *buf = calloc(1, BLOCK_SIZE); // Allocate a blank block
	memcpy(buf, data, BLOCK_SIZE);
	write_blocks(start_address, 1, buf);
	free(buf);
}

void* read_single_block(int start_address) {
    void *buf = calloc(1, BLOCK_SIZE); // Allocate a blank block
    read_blocks(start_address, 1, buf);
    return buf;
}

int get_free_block() {
	for (int i = 0; i < BLOCK_SIZE; i++) {
		if (fbm.bytes[i] != 0) {  // Only need to use the LSB here
			fbm.bytes[i] = 0;
			return i;
		}
	}
	return -1;
}

void init_fbm_and_wm() {
	for (int i = 0; i < BLOCK_SIZE; i++) {
		fbm.bytes[i] = 1; // Only need to use the LSB here, instead of 0xFF
		wm.bytes[i] = 1;
	}
	write_single_block(NUM_DATA_BLOCKS - 2, &fbm);
	write_single_block(NUM_DATA_BLOCKS - 1, &wm);
}

void init_super() { // populate root j-node
	super.magic = MAGIC;
	super.block_size = BLOCK_SIZE;
	super.num_blocks = NUM_DATA_BLOCKS + 3;
	super.num_inodes = 14;
	super.last_shadow = -1;
	inode_t root;
	root.size = -1; // Q: Is the size of root = sum of all bytes or the number of i-nodes? (Probably sum of all bytes)
	root.indirect = -1;
	for (int i = 0; i < NUM_DIRECT_POINTERS; i++)
	{
		root.direct[i] = get_free_block(); // find free blocks for every direct pointer
	}
	super.root = root;
	write_single_block(0, &super);
}

void init_directory_cache() {

}

void init_ofd() {

}

/*
	Formats the virtual disk and creates the SSFS file system on top of the disk.
	------------------------------------------------------------------------------------------------
	fresh: Flag to signal if the file system should be created from scratch. If false, the file
		   system is opened from the disk.
*/
void mkssfs(int fresh) {
	char *disk_name = "seanstappas";
	if (fresh)
		init_fresh_disk(disk_name, BLOCK_SIZE, NUM_DATA_BLOCKS + 3); // +3 for super, fbm, wm
	else
		init_disk(disk_name, BLOCK_SIZE, NUM_DATA_BLOCKS + 3);
	init_fbm_and_wm(); // What to do here if not fresh? Copy super, fbm, wm from disk.
	init_super();
    init_ofd();
    init_directory_cache();
	// init file containing all i-nodes
	// setup root directory
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

    for (int i = 0; i < NUM_FILES; i++) {
        if (strcmp(directory_cache[i].filename, name) == 0)
            return directory_cache[i].inode_index;
    }

	return -1;
}

/*
	Closes the given file. The corresponding entry is removed from the open file descriptor table.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).

	Returns: 0 on success, -1 on failure.
*/
int ssfs_fclose(int fileID) {
	return -1;
}        

/*
	Move the read pointer to the given location in the file.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).
	loc:	The location to move the read pointer to.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_frseek(int fileID, int loc) { // TODO: Check if loc is valid?
    if (fileID < 0 || fileID >= NUM_FILES)
		return -1;
    ofd_table.read_pointers[fileID] = loc;
	return 0;
}

/*
	Move the write pointer to the given location in the file.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).
	loc:	The location to move the write pointer to.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_fwseek(int fileID, int loc) {
	if (fileID < 0 || fileID >= NUM_FILES)
		return -1;
	ofd_table.write_pointers[fileID] = loc;
	return 0;
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
	int write_pointer = ofd_table.write_pointers[fileID];
	return -1;
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
	int read_pointer = ofd_table.read_pointers[fileID];
	return -1;
}

/*
	Removes a file from the filesystem. The file is removed from the directory entry, the i-node
	entry is released, and the data blocks used by the file are released.
	------------------------------------------------------------------------------------------------
	file: The file name.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_remove(char *file) {
	return -1;
}         

/*
	Create a shadow of the file system. The newly added blocks become read-only.
	------------------------------------------------------------------------------------------------
	Returns: The index of the shadow root that holds the previous commit.
*/
int ssfs_commit() {
	return -1;
}                   

/*
	Restore the file system to a previous shadow (state).
	------------------------------------------------------------------------------------------------
	cnum: The commit number to restore to.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_restore(int cnum) {
	return -1;
}
