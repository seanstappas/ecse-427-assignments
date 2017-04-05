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
#define NUM_ROOT_DIRECTORY_BLOCKS 4

#define SUPER_INDEX 0
#define FBM_INDEX 1022
#define WM_INDEX 1023

#define DEBUG 0

/**
 * Simple version of the UNIX inodes, with only single indirect.
 */
typedef struct _inode_t { // total size of inode = 64 bytes
    int size; // can have negative size (init to -1). Represents size of file, in bytes (can mod for number of blocks...)
    int direct[NUM_DIRECT_POINTERS]; // init to -1
    int indirect; // init to -1. Only used for large files
} inode_t;

/**
 * Super block, holding useful metadata about the file system and the system geometry.
 */
typedef struct _superblock_t {
    int magic;
    int block_size;
    int num_blocks;
    int num_inodes;
    inode_t root;
    inode_t shadow[NUM_SHADOWS];
    int32_t last_shadow;
} super_block_t;

/**
 * Block type. From the program's point of view, the disk emulator is just an array of blocks.
 */
typedef struct _block_t {
    unsigned char bytes[BLOCK_SIZE]; // use char since 1 char = 1 byte
} block_t;

/**
 * Open File Descriptor (OFD) table. Contains the read and write pointers for each file. Each file is identified by its
 * index, which is the same for the OFD table, root directory, and inode table. This table is only stored in memory, and
 * not on disk.
 */
typedef struct _ofd_table_t {
    int read_pointers[NUM_FILES];
    int write_pointers[NUM_FILES];
} ofd_table_t;

/**
 * Single directory entry, which is simply the file name.
 */
typedef struct _directory_entry_t {
    char filename[MAX_FILENAME_LENGTH];
} directory_entry_t;

/**
 * Root directory which contains all the directory entries for each file. Each file is identified by its index, which
 * is the same for the OFD table, root directory, and inode table. This root directory resides on the disk, but will
 * also be cached in memory.
 */
typedef struct _root_directory_t { // A block of directory entries (root directory)
    directory_entry_t directory_entries[NUM_FILES];
} root_directory_t;

/**
 * Structure containing all the inodes. This will be present on the disk, but will also be cached in memory.
 */
typedef struct _inode_table_t {
    inode_t inodes[NUM_FILES];
} inode_table_t;

/**
 * A block holding only pointers to other data blocks. This is used for large files, where the direct pointers alone of
 * the inode are not enough to store the entire file. Each file can have a maximum of one indirect block.
 */
typedef struct _indirect_block_t {
    int inode_indices[NUM_INDIRECT_POINTERS_PER_BLOCK];
} indirect_block_t;

/**
 * In-memory caches of all the important structures (super block, fbm block, wm block, OFD table, root directory, and
 * inode table).
 */
super_block_t super; // Cache of the super block
block_t fbm; // Cache of the FBM block, which keeps track of unused data blocks
block_t wm; // Cache of the WM block, which keeps track of writeable data blocks
ofd_table_t ofd_table; // Open File Descriptor table (cache of read and write pointers for each file)
root_directory_t root_directory; // Cache of all file names
inode_table_t inode_table; // Cache of all inodes

/**
 * Writes a single block to the disk emulator.
 *
 * @param start_address  the address to start writing data to (in number of blocks)
 * @param data           the data to write
 */
void write_single_block(int start_address, void *data) {
    void *buf = calloc(1, BLOCK_SIZE); // Allocate a blank block
    memcpy(buf, data, BLOCK_SIZE);
    write_blocks(start_address, 1, buf);
    free(buf);
}

/**
 * Reads a single data block from the disk emulator and places the data in a calloc'd buffer. It is up to the user to
 * free the buffer.
 *
 * @param   start_address the address to start reading from (in number of blocks)
 * @return  a pointer to the buffer with read data
 */
void *read_single_block(int start_address) {
    void *buf = calloc(1, BLOCK_SIZE); // Allocate a blank block
    read_blocks(start_address, 1, buf);
    return buf;
}

/**
 * Saves the FBM to the disk emulator.
 */
void save_fbm() {
    write_single_block(NUM_DATA_BLOCKS - 2, &fbm);
}

/**
 * Saves the WM to the disk emulator.
 */
void save_wm() {
    write_single_block(NUM_DATA_BLOCKS - 1, &wm);
}

/**
 * Saves the root directory the disk emulator.
 */
void save_root_directory() {
    write_blocks(15, 4, &root_directory);
}

/**
 * Saves the root directory to the emulator.
 */
void save_inode_table() {
    void *buf = calloc(1, BLOCK_SIZE * NUM_DIRECT_POINTERS);
    memcpy(buf, &inode_table, BLOCK_SIZE * NUM_DIRECT_POINTERS);
    write_blocks(1, NUM_DIRECT_POINTERS, buf);
    free(buf);
}

/**
 * Finds a free data block in the disk emulator.
 *
 * @return  the address on the free block (in number of blocks)
 */
int get_free_block() {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (fbm.bytes[i] != 0) {  // Only need to use the LSB here
            fbm.bytes[i] = 0;
            save_fbm();
            return i;
        }
    }
    return -1;
}

/**
 * Initializes the inode table and saves it to the disk emulator.
 */
void init_inode_table() {
    for (int i = 0; i < NUM_FILES; i++) {
        inode_table.inodes[i].size = -1;
        inode_table.inodes[i].indirect = -1;
        for (int j = 0; j < NUM_DIRECT_POINTERS; j++) {
            inode_table.inodes[i].direct[j] = -1;
        }
    }
    save_inode_table();
}

/**
 * Initializes the FBM and WM, and saves them to the disk emulator.
 */
void init_fbm_and_wm() {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        fbm.bytes[i] = 1;
        wm.bytes[i] = 1;
    }
    fbm.bytes[BLOCK_SIZE - 1] = 0;
    fbm.bytes[BLOCK_SIZE - 2] = 0;
    wm.bytes[BLOCK_SIZE - 1] = 0; // TODO: Change this for shadowing...
    wm.bytes[BLOCK_SIZE - 2] = 0;
    save_fbm();
    save_wm();
}

/**
 * Initializes the super block, and saves it to the disk emulator.
 */
void init_super() { // populate root j-node // TODO: update super for shadowing section...
    super.magic = MAGIC;
    super.block_size = BLOCK_SIZE;
    super.num_blocks = NUM_DATA_BLOCKS + 3;
    super.num_inodes = 14;
    super.last_shadow = -1;
    inode_t root;
    root.size = -1; // Q: Is the size of root = sum of all bytes or the number of i-nodes? (Probably sum of all bytes)
    root.indirect = -1;
    for (int i = 0; i < NUM_DIRECT_POINTERS; i++) {
        root.direct[i] = get_free_block(); // find free blocks for every direct pointer (should be 1...14)
    }
    super.root = root;
    write_single_block(SUPER_INDEX, &super);
}

/**
 * Initializes the root directory and saves it to the disk emulator.
 */
void init_root_directory() {
    for (int i = 0; i < NUM_FILES; i++) {
        root_directory.directory_entries[i].filename[0] = '\0';
    }
    for (int j = 0; j < NUM_ROOT_DIRECTORY_BLOCKS; j++) {
        get_free_block(); // should be (15...18)
    }
    save_root_directory();
}

/**
 * Initializes the open file descriptor table.
 */
void init_ofd() {
    for (int i = 0; i < NUM_FILES; i++) {
        ofd_table.read_pointers[i] = -1;
        ofd_table.write_pointers[i] = -1;
    }
}

/**
 * Formats the virtual disk and creates the SSFS file system on top of the disk.
 *
 * @param fresh  a flag to signal if the file system should be created from scratch. If false, the file
 *               system is opened from the disk.
 */
void mkssfs(int fresh) {
    char *disk_name = "seanstappas";
    if (fresh) { // Create new copy
        init_fresh_disk(disk_name, BLOCK_SIZE, NUM_DATA_BLOCKS + 3); // +3 for super, fbm, wm
        init_fbm_and_wm();
        init_super();
        init_root_directory();
        init_inode_table();
    } else { // Access old copy
        init_disk(disk_name, BLOCK_SIZE, NUM_DATA_BLOCKS + 3);
        read_blocks(SUPER_INDEX, 1, &super);
        read_blocks(1, NUM_DIRECT_POINTERS, &inode_table);
        read_blocks(15, 4, &root_directory);
        read_blocks(FBM_INDEX, 1, &fbm);
        read_blocks(WM_INDEX, 1, &wm);
    }
    init_ofd();
}

/**
 * Opens the given file. If the file does not exist, a new file with size 0 is created. If it
 * exists, read pointer is at the beginning of the file, and write pointer at the end (append
 * mode).
 *
 * @param name  the name of the file to be opened
 * @return      an integer corresponding to the index of the entry for the opened file in the file
 *              descriptor table, or -1 on failure
 */
int ssfs_fopen(char *name) {
    if (strlen(name) < 1 || strlen(name) > MAX_FILENAME_LENGTH - 1)
        return -1; // Error: invalid name

    for (int i = 0; i < NUM_FILES; i++) {
        if (strcmp(root_directory.directory_entries[i].filename, name) == 0) { // Root directory match found
            int size = inode_table.inodes[i].size;
            ofd_table.write_pointers[i] = size; // Update read & write pointers
            ofd_table.read_pointers[i] = 0;
            return i; // Success: returns index of existing file
        }
    }

    // File doesn't exist
    for (int j = 0; j < NUM_FILES; j++) {
        if (root_directory.directory_entries[j].filename[0] == '\0') { // Free slot in the root directory
            inode_table.inodes[j].size = 0;
            save_inode_table();
            ofd_table.write_pointers[j] = 0;
            ofd_table.read_pointers[j] = 0;
            strncpy(root_directory.directory_entries[j].filename, name,
                    MAX_FILENAME_LENGTH); // Place the name in the root directory
            save_root_directory();
            return j; // Success: returns index of new file
        }
    }

    return -1; // Error: no space for new file
}

/**
 * Closes the given file. The corresponding entry is removed from the open file descriptor table.
 *
 * @param fileID  the file ID corresponding to the file (from the open file descriptor table)
 * @return        0 on success, -1 on failure
 */
int ssfs_fclose(int fileID) {
    if (fileID < 0 || fileID >= NUM_FILES || ofd_table.read_pointers[fileID] < 0 ||
        ofd_table.write_pointers[fileID] < 0)
        return -1; // Error: invalid fileID

    ofd_table.read_pointers[fileID] = -1; // Reset read & write pointers
    ofd_table.write_pointers[fileID] = -1;

    return 0; // Success
}

/**
 * Moves the read pointer to the given location in the file.
 *
 * @param fileID  the file ID corresponding to the file (from the open file descriptor table)
 * @param loc     the location to move the read pointer to
 * @return        0 on success, -1 on failure
 */
int ssfs_frseek(int fileID, int loc) {
    if (fileID < 0 || fileID >= NUM_FILES || ofd_table.read_pointers[fileID] < 0 ||
        ofd_table.write_pointers[fileID] < 0 || loc < 0 || loc > inode_table.inodes[fileID].size) // Error checking
        return -1; // Error: invalid fileID or loc

    ofd_table.read_pointers[fileID] = loc; // Update read pointer

    return 0; // Success
}

/**
 * Moves the write pointer to the given location in the file.
 *
 * @param fileID  the file ID corresponding to the file (from the open file descriptor table)
 * @param loc     the location to move the write pointer to
 * @return        0 on success, -1 on failure
 */
int ssfs_fwseek(int fileID, int loc) {
    if (fileID < 0 || fileID >= NUM_FILES || ofd_table.read_pointers[fileID] < 0 ||
        ofd_table.write_pointers[fileID] < 0 || loc < 0 || loc > inode_table.inodes[fileID].size)
        return -1; // Error: invalid fileID or loc

    ofd_table.write_pointers[fileID] = loc; // Update write pointer

    return 0; // Success
}

/**
 * Writes characters into a file on the disk, starting from the write pointer of the file. It is up to the user to
 * properly initialize and provide the data buffer.
 *
 * @param fileID  the file ID corresponding to the file (from the open file descriptor table)
 * @param buf     the characters to be written into the file
 * @param length  the number of bytes to be written
 * @return        the number of bytes written, or -1 on failure (if reached maximum capacity)
 */
int ssfs_fwrite(int fileID, char *buf, int length) {
    if (fileID < 0 || fileID >= NUM_FILES || ofd_table.read_pointers[fileID] < 0 ||
        ofd_table.write_pointers[fileID] < 0 || length < 0)
        return -1; // Error: invalid fileID or length

    int write_pointer = ofd_table.write_pointers[fileID];
    inode_t inode = inode_table.inodes[fileID]; // Find inode associated with current file
    int size = inode.size;
    int new_size = write_pointer + length; // New size of file after write is complete
    if (new_size < size) {
        new_size = size;
    }
    int num_blocks = new_size / BLOCK_SIZE; // Number of blocks the file will take up
    if (new_size % BLOCK_SIZE != 0)
        num_blocks++;

    char *buf1 = calloc(1, (size_t) (num_blocks * BLOCK_SIZE)); // Buffer to hold the entire read file
    int old_read_pointer = ofd_table.read_pointers[fileID];
    ssfs_frseek(fileID, 0);
    ssfs_fread(fileID, buf1, size); // TODO: Make this more efficient (only read what is necessary)
    ssfs_frseek(fileID, old_read_pointer); // Reset read pointer

    memcpy(buf1 + write_pointer, buf, length);

    indirect_block_t *indirect = NULL; // Cached indirect block
    for (int i = 0; i < num_blocks; i++) {
        int block_num;
        if (i < NUM_DIRECT_POINTERS) { // Direct blocks
            block_num = inode.direct[i];
            if (block_num < 0) { // Uninitialized direct block
                block_num = get_free_block();
                inode_table.inodes[fileID].direct[i] = block_num;
            }
        } else { // Indirect blocks
            if (inode.indirect < 0) { // Uninitialized single indirect block
                indirect_block_t indirect_block;
                int indirect_block_index = get_free_block();
                inode.indirect = indirect_block_index;
                inode_table.inodes[fileID].indirect = indirect_block_index;
                block_num = get_free_block();
                for (int j = 0; j < NUM_INDIRECT_POINTERS_PER_BLOCK; j++) {
                    indirect_block.inode_indices[j] = -1;
                }
                indirect_block.inode_indices[0] = block_num;
                write_single_block(indirect_block_index, &indirect_block); // Save single indirect block to emulator
            } else { // Single indirect block is already initialized
                if (indirect == NULL)
                    indirect = (indirect_block_t *) read_single_block(inode.indirect); // Cache single indirect block
                int indirect_index = i - NUM_DIRECT_POINTERS;
                if (indirect_index >= NUM_INDIRECT_POINTERS_PER_BLOCK) {
                    free(buf1);
                    if (indirect != NULL)
                        free(indirect);
                    return -1; // Error: reached maximum number of indirect blocks
                }
                block_num = indirect->inode_indices[indirect_index];
                if (block_num < 0) { // Uninitialized indirect block
                    block_num = get_free_block();
                    indirect->inode_indices[indirect_index] = block_num;
                    write_single_block(inode.indirect, indirect);
                }
            }
        }
        if (block_num < 0 || block_num >= NUM_DATA_BLOCKS) {
            free(buf1);
            if (indirect != NULL)
                free(indirect);
            return -1; // Error: no free block (reached maximum capacity)
        }
        write_blocks(block_num, 1, buf1 + (i * BLOCK_SIZE));
    }
    free(buf1);
    if (indirect != NULL)
        free(indirect);

    inode_table.inodes[fileID].size = new_size; // Update the inode's size
    ofd_table.write_pointers[fileID] = new_size; // Move the write pointer to the end of the file
    save_inode_table();

    return length; // Success: returns the number of bytes written
}

/**
 * Read characters from a file on disk to a buffer, starting from the read pointer of the current file.
 *
 * @param fileID  the file ID corresponding to the file (from the open file descriptor table)
 * @param buf     a buffer to store the read bytes in (already allocated)
 * @param length  the number of bytes to be read
 * @return        the number of bytes read
 */
int ssfs_fread(int fileID, char *buf, int length) {
    if (fileID < 0 || fileID >= NUM_FILES || ofd_table.read_pointers[fileID] < 0 ||
        ofd_table.write_pointers[fileID] < 0 || length < 0)
        return -1; // Error: invalid fileID or length

    int read_pointer = ofd_table.read_pointers[fileID];
    inode_t inode = inode_table.inodes[fileID]; // Find inode associated with current file
    int size = inode.size;
    if (size == 0)
        return 0; // Success: no bytes to read

    int num_blocks = size / BLOCK_SIZE; // Number of blocks needed to hold file
    if (size % BLOCK_SIZE != 0)
        num_blocks++;

    int bytes_to_read = length; // Number of bytes to read
    if (read_pointer + length > size) {
        bytes_to_read = size - read_pointer;
        if (bytes_to_read <= 0)
            return 0; // Success: no bytes to read
    }
    char *buf1 = calloc(1, BLOCK_SIZE); // Buffer to hold each block read
    char *buf2 = calloc(1, (size_t) (num_blocks * BLOCK_SIZE)); // Buffer to hold the entire file
    indirect_block_t *indirect = NULL;
    for (int i = 0; i < num_blocks; i++) {
        int block_num;
        if (i < NUM_DIRECT_POINTERS) // Direct blocks
            block_num = inode.direct[i];
        else { // Indirect blocks
            if (indirect == NULL)
                indirect = (indirect_block_t *) read_single_block(inode.indirect); // Cache single indirect block
            int indirect_index = i - NUM_DIRECT_POINTERS;
            if (indirect_index >= NUM_INDIRECT_POINTERS_PER_BLOCK) {
                free(buf1);
                free(buf2);
                if (indirect != NULL)
                    free(indirect);
                return -1; // Error: reached maximum size of single indirect block
            }
            block_num = indirect->inode_indices[indirect_index];
        }
        if (block_num < 0) {
            free(buf1);
            free(buf2);
            if (indirect != NULL)
                free(indirect);
            return -1; // Error: invalid block number (tried to read from uninitialized block)
        }
        memset(buf1, 0, BLOCK_SIZE); // Clear first buffer
        read_blocks(block_num, 1, buf1); // Place data block in first buffer
        memcpy(buf2 + (i * BLOCK_SIZE), buf1, BLOCK_SIZE); // Place data block in big buffer
    }

    free(buf1);
    memcpy(buf, buf2 + read_pointer, bytes_to_read); // Copy needed data into final buffer (starting from read pointer)

    free(buf2);
    if (indirect != NULL)
        free(indirect);

    ofd_table.read_pointers[fileID] = read_pointer + bytes_to_read; // Move read pointer up

    return bytes_to_read; // Success: returns the number of bytes read
}

/**
 * Removes a file from the filesystem. The file is removed from the directory entry, the i-node
 * entry is released, and the data blocks used by the file are released. Set all blocks used by file to free in the FBM
 * and remove the associated inode from the inode table.
 *
 * @param file  the file name
 * @return      0 on success, -1 on failure
 */
int ssfs_remove(char *file) {
    for (int i = 0; i < NUM_FILES; i++) {
        if (strcmp(root_directory.directory_entries[i].filename, file) == 0) { // Found file in root directory
            root_directory.directory_entries[i].filename[0] = '\0'; // Clear filename
            save_root_directory();
            ofd_table.read_pointers[i] = -1; // Clear read & write pointers
            ofd_table.write_pointers[i] = -1;
            inode_t inode = inode_table.inodes[i];
            for (int j = 0; j < NUM_DIRECT_POINTERS; j++) {
                int block_number = inode.direct[j];
                if (block_number != -1) {
                    fbm.bytes[block_number] = 1; // Free the direct blocks
                }
                inode_table.inodes[i].direct[j] = -1;
            }
            inode_table.inodes[i].size = -1;
            if (inode.indirect != -1) {
                indirect_block_t *indirect = (indirect_block_t *) read_single_block(inode.indirect);
                for (int j = 0; j < NUM_INDIRECT_POINTERS_PER_BLOCK; j++) {
                    int block_num = indirect->inode_indices[j];
                    if (block_num != -1) {
                        fbm.bytes[block_num] = 1; // Free the indirect blocks
                    }
                }
                free(indirect);
                inode_table.inodes[i].indirect = -1;
            }
            save_fbm();
            save_inode_table();
            return 0; // Success: file removed
        }
    }

    return -1; // Error: file not found in root directory (invalid file name)
}

/**
 * Creates a shadow of the file system. The newly added blocks become read-only.
 *
 * @return  the index of the shadow root that holds the previous commit.
 */
int ssfs_commit() {
    return -1;
}

/**
 * Restores the file system to a previous shadow (state).
 *
 * @param cnum  The commit number to restore to.
 * @return      0 on success, -1 on failure
 */
int ssfs_restore(int cnum) {
    return -1;
}
