#ifndef PARSE_LFS_H
#define PARSE_LFS_H
// #define MAX_INODES 131072 // assuming this is large enough, may need to adjust!
#define MAX_INODES 524288
#define NUM_ENTRIES 20
#define MAX_FILENAME_LEN 256 // chose this because its the same as linux uses

// See `man inode` for more on bitmasks.
#include <sys/stat.h> // Provides mode_t, S_ISDIR, etc.
#include <time.h> // Provides time_t
#include "../lib/lib.h" // Provides ls_print_file(...)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// You may need more includes!

// Can add more structs, change, extend, or delete these.
typedef struct{
    uint file_cursor;
    uint size;
    mode_t permissions;
    time_t mtime;

    uint num_direct_blocks;
    uint *data_blocks; // list of datablocks

    // Could have one or more entries pointing to data block 
    // Each entry is:
        // uint direct_block_disk_offset (in bytes)
    // ...

} Inode;

typedef struct{
    uint inode_number;
    uint inode_disk_offset;
} ImapNode; // list of inodes

typedef struct{
    uint num_entries;
    ImapNode *inodes; // list of inodes
    // Could have one or more entries pointing to Inode 
    // Each entry has:
        // uint inumber 
        // uint inode_disk_offset
    // ...

} Imap;

typedef struct{
    uint inumber_start;
    uint inumber_end;
    uint imap_disk_offset;
} CRNode; // list of imaps

typedef struct{
    uint image_offset;
    uint num_entries;
    CRNode *imaps; // list of imaps
    // Could have one or more entries pointing to Imaps
    // Each entry has:
        // uint inumber_start 
        // uint inumber_end
        // uint imap_disk_offset
    // ...
} CR;

typedef struct {
    char name[MAX_FILENAME_LEN];
    uint inumber;
    mode_t permissions;
} DirEntry;

CR *get_checkpoint_region(FILE *fp);
int compare_directory_entries(const void *a, const void *b);
int ls_print_recursive_helper(FILE *fp, Inode *inodes_array[], uint max_num_inodes, uint curr_inumber, uint depth);
int lfs_ls(FILE *fp, Inode* inodes_array[], uint max_inode_num);
int lfs_cat(FILE *fp, Inode* inodes_array[], uint max_inode_num, char *filepath);   
Imap *read_imap(FILE *fp, uint offset);
Inode *read_inode(FILE *fp, uint offset);

#endif
