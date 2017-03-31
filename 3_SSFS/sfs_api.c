#include "sfs_api.h"
#include "disk_emu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 260639512 // student id
#define BLOCK_SIZE 1024
#define NUM_DATA_BLOCKS 1024 // Not including super, FBM, WM
#define NUM_FILES 200 // Number of file i-nodes. Not including super. Set upper bound.
#define NUM_INODES_PER_BLOCK 15
#define NUM_DIRECTORY_ENTRIES_PER_BLOCK 60
#define NUM_DIRECT_POINTERS 14
#define NUM_INDIRECT_POINTERS_PER_BLOCK 256
#define NUM_SHADOWS 4
#define MAX_FILENAME_LENGTH 10
#define DIRECTORY_ENTRY_LENGTH 16

typedef struct _inode_t { // total size of inode = 64 bytes
	int size; // can have negative size (init to -1). Represents size of file, in bytes (can mod for number of blocks...)
	int direct[NUM_DIRECT_POINTERS]; // init to -1
	int indirect; // init to -1. Only used for large files
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
	int read_pointer_offsets[NUM_FILES];
	int write_pointers_offsets[NUM_FILES];

} ofd_table_t;

typedef struct _directory_entry_t { // fits within 16 bytes (actually 14)
	int inode_index; // 4 bytes
	char filename[MAX_FILENAME_LENGTH]; // 10 bytes
} directory_entry_t;

typedef struct _root_directory_t { // A block of directory entries (root directory)
	directory_entry_t directory_entries[NUM_FILES];
} root_directory_t;

// TODO: Put all the inodes in memory in mkssfs (inode_table) -> write to disk

typedef struct _inode_table_t {
    inode_t inodes[NUM_FILES];
} inode_table_t;

typedef struct _indirect_block_t {
    int inode_indices[NUM_INDIRECT_POINTERS_PER_BLOCK];
} indirect_block_t;

super_block_t super; // Defines the file system geometry
block_t fbm; // Unused data blocks (doesn't track super, fbm or wm) (value 1 = data block at that index is unused)
block_t wm; // Writeable data blocks (value 1 = data block at that index is writeable). Not necessary for simple system (without shadowing)
ofd_table_t ofd_table; // Open File Descriptor Table (in-memory cache of RW pointers + inodes).
root_directory_t root_directory; // Cache of all filenames
inode_table_t inode_table;

void write_single_block(int start_address, void *data, int offset) {
	void *buf = calloc(1, BLOCK_SIZE); // Allocate a blank block
	memcpy(buf + offset, data, BLOCK_SIZE - offset);
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
	write_single_block(NUM_DATA_BLOCKS - 2, &fbm, 0);
	write_single_block(NUM_DATA_BLOCKS - 1, &wm, 0);
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
	write_single_block(0, &super, 0);
}

void init_root_directory() {
    for (int i = 0; i < NUM_FILES; i++) {
        root_directory.directory_entries[i].inode_index = -1;
    }
}

void init_ofd() {
    for (int i = 0; i < NUM_FILES; i++) {
        ofd_table.read_pointer_offsets[i] = -1;
        ofd_table.write_pointers_offsets[i] = -1;
    }
}

/*
	Formats the virtual disk and creates the SSFS file system on top of the disk.
	------------------------------------------------------------------------------------------------
	fresh: Flag to signal if the file system should be created from scratch. If false, the file
		   system is opened from the disk.
*/
void mkssfs(int fresh) {
	char *disk_name = "seanstappas";
	if (fresh) {
        init_fresh_disk(disk_name, BLOCK_SIZE, NUM_DATA_BLOCKS + 3); // +3 for super, fbm, wm
        init_fbm_and_wm(); // What to do here if not fresh? Copy super, fbm, wm from disk.
        init_super();
        init_ofd();
        init_root_directory();
    } else {
        init_disk(disk_name, BLOCK_SIZE, NUM_DATA_BLOCKS + 3);
    }
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
        if (root_directory.directory_entries[i].inode_index != -1 && strcmp(root_directory.directory_entries[i].filename, name) == 0)
            return root_directory.directory_entries[i].inode_index;
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
    ofd_table.read_pointer_offsets[fileID] = -1;
    ofd_table.write_pointers_offsets[fileID] = -1;
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
    ofd_table.read_pointer_offsets[fileID] = loc;
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
	ofd_table.write_pointers_offsets[fileID] = loc;
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
	int write_pointer = ofd_table.write_pointers_offsets[fileID];
    if (write_pointer < 0)
        return -1;
    if (write_pointer > length)
        write_pointer = length;
    int num_blocks = length % BLOCK_SIZE;
    if (num_blocks < 0 || num_blocks > NUM_DIRECT_POINTERS) // TODO: Implement indirect pointers
        return -1; // too many blocks
    int offset = write_pointer;
    for (int i = 0; i < num_blocks; i++) {
        int block_index = get_free_block();
        if (block_index == -1) // TODO: What to do when no more space? Overwrite?
            return -1;
        write_single_block(block_index, buf, offset); // TODO: This is wrong... do this correctly.
    }
	return -1;

    /*
     * Implement read first! Need to read first before writing.
     *
     * Check first if at end of file
     *
     * char giant_buf* = calloc(1, num_blocks * 1024);
     * fseek_read(...);
     * ssfs_fread(fileID, giant_buf, length) MAKE SURE TO RESET READ POINTER! (fseek back)
     *
     * memcpy(giant_buf + writeptr, buffer, length);
     *
     * write back to disk (one at a time)
     *
     * Return -1 if cannot write full data...
     */
}

/*
	Read characters from a file on disk to a buffer, starting from the read pointer of the current
	file.
	------------------------------------------------------------------------------------------------
	fileID: The file ID corresponding to the file (from the open file descriptor table).
	buf:	A buffer to store the read bytes in (already allocated).
	length: The number of bytes to be read.

	Returns: The number of bytes read!! Not length (since length may be larger than what is actually left to read).
*/
int ssfs_fread(int fileID, char *buf, int length) {
	int read_pointer = ofd_table.read_pointer_offsets[fileID];
    inode_t inode = inode_table.inodes[fileID];
    int size = inode.size;
    int num_blocks = size / BLOCK_SIZE;

    int bytes_to_read = length;
    if (read_pointer + length > size) {
        bytes_to_read = size - read_pointer;
    }

        // Assuming single block
    //    char *block_temp_buf = read_single_block(inode_table.inodes[fileID].direct[0]);
    //    memcpy(buf, block_temp_buf + read_pointer, length);
    // Error check!

    // Single block:
    // char buffer[1024];
    // readblock(blk)
    // memcpy(buf + readptr, buffer

    // Otherwise:
    // char Buf1[1024]
    // char *Buf2 = calloc(num_blocks * 1024);
    // readblock(blk)
    // memcpy(buf2 + readptr + i * 1024, buf1, length);
    // readptr % 1024 = where you should start
    // readptr + length % 1024 = where to end

    char *buf1;
    char *buf2 = calloc(1, (size_t) (num_blocks * 1024));
    indirect_block_t *indirect = NULL;
    for (int i = 0; i < num_blocks; i++) {
        int block_num;
        if (i < NUM_DIRECT_POINTERS)
            block_num = inode.direct[i];
        else {
            if (indirect == NULL)
                indirect = (indirect_block_t*) read_single_block(inode.indirect);
            block_num = indirect->inode_indices[i - NUM_DIRECT_POINTERS];
        }
        buf1 = read_single_block(block_num); // TODO: make this more efficient (don't need to calloc every time)
        memcpy(buf2 + (i * BLOCK_SIZE), buf1, BLOCK_SIZE);
        free(buf1);
    }

    memcpy(buf, buf2 + read_pointer, bytes_to_read);
    free(buf2);
    if (indirect != NULL)
        free(indirect);


    ofd_table.read_pointer_offsets[fileID] = read_pointer + bytes_to_read;

	return bytes_to_read;
}

/*
	Removes a file from the filesystem. The file is removed from the directory entry, the i-node
	entry is released, and the data blocks used by the file are released.
	------------------------------------------------------------------------------------------------
	file: The file name.

	Returns: 0 on success, -1 on failure.
*/
int ssfs_remove(char *file) {
    // Set all blocks used by file to FREE (in FBM) and remove inode from inode table.
    for (int i = 0; i < NUM_FILES; i++) {
        int inode_index = root_directory.directory_entries[i].inode_index;
        if (inode_index != -1 && strcmp(root_directory.directory_entries[i].filename, file) == 0) {
            root_directory.directory_entries[i].inode_index = -1;
            ofd_table.read_pointer_offsets[inode_index] = -1;
            ofd_table.write_pointers_offsets[inode_index] = -1;
            inode_t inode = inode_table.inodes[inode_index];
            for (int j = 0; j < NUM_DIRECT_POINTERS; j++) {
                int block_number = inode.direct[j];
                if (block_number != -1) {
                    fbm.bytes[block_number] = 1;
                }
            }
            inode_table.inodes[inode_index].size = -1;
            return 0;
        }
    }
    
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
