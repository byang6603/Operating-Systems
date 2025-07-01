#include "parse_lfs.h"
#include <string.h> // Include for strcmp


CR* get_checkpoint_region(FILE *fp){
    CR *cr = malloc(sizeof(CR));
    if(cr == NULL){
        printf("Error, cr initilization failed\n");
        return NULL;
    }

    if (fread(&cr->image_offset, sizeof(uint), 1, fp) != 1) {
        printf("Error, failed to read offset\n");
        free(cr);
        return NULL;
    };
    if (fread(&cr->num_entries, sizeof(uint), 1, fp) != 1) {
        printf("Error, failed to read num_entries\n");
        free(cr);
        return NULL;
    };

    cr->imaps = malloc(cr->num_entries * sizeof(*cr->imaps));// initialize array of imap pointers
    for(uint i = 0; i < cr->num_entries; i++){
        if (fread(&cr->imaps[i].inumber_start, sizeof(uint), 1, fp) != 1) {
            printf("Error, failed to read imaps[%i] inumber_start\n", i);
            free(cr->imaps);
            free(cr);
            return NULL;
        };
        if (fread(&cr->imaps[i].inumber_end, sizeof(uint), 1, fp) != 1) {
            printf("Error, failed to read imaps[%i] inumber_end\n", i);
            free(cr->imaps);
            free(cr);
            return NULL;
        };
        if (fread(&cr->imaps[i].imap_disk_offset, sizeof(uint), 1, fp) != 1) {
            printf("Error, failed to read imaps[%i] imap_disk_offset\n", i);
            free(cr->imaps);
            free(cr);
            return NULL;
        };
    }

    return cr;
}

Imap *read_imap(FILE* fp, uint offset){
    Imap *imap = malloc(sizeof(Imap));
    if (imap == NULL) {
        perror("Error, failed to alloc memory for Imap struct");
        return NULL;
    }
    imap->inodes = NULL;

    if(fseek(fp, offset, SEEK_SET) != 0){
        printf("Error seeking to read imap at offset %d", offset);
        free(imap);
        return NULL;
    }

    if(fread(&imap->num_entries, sizeof(uint), 1, fp) != 1){
        printf("Error reading num_entries for imap at offset %u", offset);
        free(imap);
        return NULL;
    }

    imap->inodes = malloc(imap->num_entries * sizeof(ImapNode));
    if(!imap->inodes){
        printf("Error allocating memory for inodes in imap at offset %d", offset);
        free(imap);
        return NULL;
    }

    size_t read_items = fread(imap->inodes, sizeof(ImapNode), imap->num_entries, fp);
    if (read_items != imap->num_entries) {
        fprintf(stderr, "Error reading imap entries: expected %u, got %zu\n", imap->num_entries, read_items);
        free(imap->inodes);
        free(imap);
        return NULL;
    }

    // no need for this since the read above does the job properly
    // for(int i = 0; i < imap->num_entries; i++){
    //     if(fread(&imap->inodes[i].inode_number, sizeof(uint), 1, fp) != 1){
    //         printf("Error retrieving inode numbers");
    //         free(imap->inodes);
    //         free(imap);
    //         return NULL;
    //     }

    //     if(fread(&imap->inodes[i].inode_disk_offest, sizeof(uint), 1,fp) != 1){
    //         printf("Error reading inode disk offset");
    //         free(imap->inodes);
    //         free(imap);
    //         return NULL;
    //     }
    // }

    return imap;
}

Inode *read_inode(FILE *fp, uint offset){
    Inode *inode = malloc(sizeof(Inode));
    if (inode == NULL) {
        fprintf(stderr, "Error, failed to alloc memory for Inode\n");
        return NULL;
    }
    inode->data_blocks = NULL;

    if(fseek(fp, offset, SEEK_SET) != 0){
        fprintf(stderr, "Error in inode when seeking to offset %d\n", offset);
        free(inode);
        return NULL;
    }

    if(fread(&inode->file_cursor, sizeof(uint), 1, fp) != 1){
        fprintf(stderr, "Error reading inode file cursor at offset %u\n", offset);
        free(inode);
        return NULL;
    }

    if(fread(&inode->size, sizeof(uint), 1, fp) != 1){
        fprintf(stderr, "Error reading inode size at offset %u\n", offset);
        free(inode);
        return NULL;
    }

    if(fread(&inode->permissions, sizeof(mode_t), 1, fp )!= 1){
        fprintf(stderr, "Error reading inode permissions at offset %u\n", offset);
        free(inode);
        return NULL;
    }

    if(fread(&inode->mtime, sizeof(time_t), 1, fp) != 1){
        printf("Error reading inode time at offset %u\n", offset);
        free(inode);
        return NULL;
    }

    if(fread(&inode->num_direct_blocks, sizeof(uint), 1, fp) != 1){
        printf("Error reading inode num direct blocks at offset %u\n", offset);
        free(inode);
        return NULL;
    }

    // in case there are 0 blocks
    if (inode->num_direct_blocks == 0) {
        inode->data_blocks = NULL;
        return inode;
    }

    inode->data_blocks = malloc(inode->num_direct_blocks * sizeof(uint));
    if(inode->data_blocks == NULL){
        fprintf(stderr, "Error allocating space for inode datablocks");
        free(inode);
        return NULL;
    }
    size_t read_items = fread(inode->data_blocks, sizeof(uint), inode->num_direct_blocks, fp);
    if (read_items != inode->num_direct_blocks) {
        fprintf(stderr, "Error reading Inode data blocks: expected %u, got %zu\n", inode->num_direct_blocks, read_items);
        free(inode->data_blocks);
        free(inode);
        return NULL;
    }
    
    // for(int i = 0; i < inode->num_direct_blocks; i++){
    //     if(fread(&inode->data_blocks[i], sizeof(uint), 1, fp) != 1){
    //         printf("Error reading inode datablocks");
    //         free(inode->data_blocks);
    //         inode->num_direct_blocks = 0;
    //         return inode;
    //     }
    // }

    return inode;

}
char* get_next_segment(char** path){
    if(*path == NULL){
        return NULL;
    }

    if(**path == '\0'){
        return NULL;
    }

    while(**path == '/'){
        (*path)++;
    }

    if(**path == '\0'){
        return NULL;
    }

    char* start = *path;

    while(**path != '/' && **path != '\0'){
        (*path)++;
    }

    if(**path == '/'){
        **path = '\0';
        (*path)++;

        char* segment = strdup(start);
        *((*path) - 1) = '/';

        return segment;
    }else{
        return strdup(start);
    }
}

// Helper function for parsing paths
char* normalize_path(const char* path) {
    if(path == NULL) {
        return NULL;
    }
    
    char* result = strdup(path);
    if(result == NULL) {
        return NULL;
    }
    
    char* in = result;
    char* out = result;
    char* last_slash = NULL;
    
    if(*in == '/') {
        *out++ = *in++;
        last_slash = out - 1;
    }
    
    while(*in) {
        if(*in == '.' && (in[1] == '/' || in[1] == '\0')) {
            if(in[1] == '/'){
                in += 2;
            }else{
                in += 1;
            }
            continue;
        }
        
        if(*in == '.' && in[1] == '.' && (in[2] == '/' || in[2] == '\0')) {
            if(last_slash != NULL) {
                out = last_slash;
                
                if(out > result) {
                    char* prev = out - 1;
                    while(prev >= result && *prev != '/') {
                        prev--;
                    }
                    if(prev >= result && *prev == '/') {
                        last_slash = prev;
                        out = prev + 1;
                    } else {
                        last_slash = result;
                        out = result + 1;
                    }
                }
            }
            
            if(in[2] == '/'){
                in += 3;
            }else{
                in += 2;
            }
            continue;
        }
        
        *out++ = *in++;
        
        if(*(out-1) == '/') {
            last_slash = out - 1;
            
            while(*in == '/') {
                in++;
            }
        }
    }
    
    if(out > result + 1 && *(out-1) == '/') {
        out--;
    }
    
    *out = '\0';
    
    return result;
}

uint path_to_inode(FILE *fp, Inode* inodes_array[], uint max_inode_num, const char* lfs_filepath) {
    if(lfs_filepath == NULL) {
        return 0;
    }
    
    if(inodes_array == NULL) {
        return 0;
    }
    
    char* normalized_path = normalize_path(lfs_filepath);
    if(normalized_path == NULL) {
        return 0;
    }
    
    uint current_inode_num = 0;
    
    if(normalized_path[0] == '\0' || (normalized_path[0] == '/' && normalized_path[1] == '\0')) {
        free(normalized_path);
        return current_inode_num;
    }
    
    char* path_ptr = normalized_path;
    if(*path_ptr == '/') {
        path_ptr++;// skip first / if it exists
    }
    
    char* segment;
    char* saveptr = NULL;
    segment = strtok_r(path_ptr, "/", &saveptr);
    
    while(segment != NULL) {
        if(current_inode_num >= max_inode_num) {
            free(normalized_path);
            return 0;
        }

        if(inodes_array[current_inode_num] == NULL){
            free(normalized_path);
            return 0;
        }
        
        if(!S_ISDIR(inodes_array[current_inode_num]->permissions)) {
            free(normalized_path);
            return 0;
        }
        
        Inode *dir_inode = inodes_array[current_inode_num];
        uint next_inode_num = 0;
        int found = 0;
        
        for(uint i = 0; i < dir_inode->num_direct_blocks && !found; i++) {
            uint block_offset = dir_inode->data_blocks[i];
            
            if(fseek(fp, block_offset, SEEK_SET) != 0) {
                continue;
            }
            
            char block_buffer[DATA_BLOCK_SIZE];
            size_t bytes_read = fread(block_buffer, 1, DATA_BLOCK_SIZE, fp);
            
            if (bytes_read <= 0) {
                continue;
            }
            
            char *curr_pos = block_buffer;
            char *end_pos = block_buffer + bytes_read;
            
            while (curr_pos < end_pos && !found) {
                char *comma_pos = memchr(curr_pos, ',', end_pos - curr_pos);// find comma separated file name and inode numbers
                if (comma_pos == NULL) {
                    break;
                }
                
                int filename_len = comma_pos - curr_pos;
                if (filename_len <= 0 || filename_len >= 256) {
                    curr_pos = comma_pos + 1;
                    continue;
                }
                
                char filename_buffer[256];
                memcpy(filename_buffer, curr_pos, filename_len);
                filename_buffer[filename_len] = '\0';
                
                curr_pos = comma_pos + 1;
                
                if (curr_pos + sizeof(uint) > end_pos) {
                    break;
                }
                
                uint entry_inode_num;
                memcpy(&entry_inode_num, curr_pos, sizeof(uint));
                
                if (strcmp(segment, filename_buffer) == 0) {
                    next_inode_num = entry_inode_num;
                    found = 1;
                    break;
                }
                
                curr_pos += sizeof(uint);
            }
        }
        
        if (!found) {
            free(normalized_path);
            return 0;
        }
        
        current_inode_num = next_inode_num;
        
        segment = strtok_r(NULL, "/", &saveptr);
    }
    
    free(normalized_path);
    return current_inode_num;
}


int compare_directory_entries(const void *a, const void *b) {
    DirEntry *entry_A = (DirEntry *)a;
    DirEntry *entry_B = (DirEntry *)b;

    if (S_ISDIR(entry_A->permissions) && !S_ISDIR(entry_B->permissions)) {
        return 1; 
    } else if (!S_ISDIR(entry_A->permissions) && S_ISDIR(entry_B->permissions)) {
        return -1; 
    } else {
        return strcmp(entry_A->name, entry_B->name); // sort alphabetically like requested
    }
}



// helper function to recursively print the directory since there is depth
// best way to do it imo
int ls_print_recursive_helper(FILE *fp, Inode *inodes_array[], uint max_num_inodes, uint curr_inumber, uint depth) {

    // validation checks
    if (curr_inumber >= max_num_inodes || inodes_array[curr_inumber] == NULL) {
        return 1; 
    }  

    Inode *directory_inode = inodes_array[curr_inumber];

    if (!S_ISDIR(directory_inode->permissions)) {
        return 1;
    }

    // read in directory data
    if (directory_inode->num_direct_blocks == 0 || directory_inode->size == 0) {
        return 0; // empty directory so nothing to print or recurse
    }

    // allocing buffer to hold dir content
    char *directory_buffer = malloc(directory_inode->size);
    if (!directory_buffer) {
        return 1;
    }

    // reading bytes
    uint read_bytes = 0;
    for (uint i = 0; i < directory_inode->num_direct_blocks; ++i) {
        uint block_offset = directory_inode->data_blocks[i];
        uint curr_read_bytes = DATA_BLOCK_SIZE; // from lib

        // calc how many bytes are left according to inode size
        uint bytes_remaining = directory_inode->size - read_bytes;
        if (bytes_remaining == 0) {
            break; // already read everything
        }

        if (curr_read_bytes > bytes_remaining) {
            curr_read_bytes = bytes_remaining;
        }

        if (fseek(fp, block_offset, SEEK_SET) != 0) {
            free(directory_buffer);
            return 1;
        }

        size_t read_items = fread(directory_buffer + read_bytes, 1, curr_read_bytes, fp);
        if (read_items != curr_read_bytes) {
            free(directory_buffer);
            return 1;
        }
        read_bytes += read_items;
    }

    // parse directory entries
    // using dynamically alloc'd array since we don't know max entries allowed
    DirEntry *entries = malloc(NUM_ENTRIES * sizeof(DirEntry));
    if (!entries) {
        free(directory_buffer);
        return 1;
    }
    uint entry_count = 0;
    uint entry_capacity = NUM_ENTRIES;

    char *curr_pos = directory_buffer;
    char *end_pos = directory_buffer + directory_inode->size;

    // commas are the seperator for file name!
    while (curr_pos < end_pos) {
        char *comma_pos = memchr(curr_pos, ',', end_pos - curr_pos);
        if (comma_pos == NULL) {
            break;
        }

        int length_of_name = comma_pos - curr_pos;
        if (length_of_name >= MAX_FILENAME_LEN) {
            length_of_name = MAX_FILENAME_LEN - 1;
        }

        // check to resize the array if required
        if (entry_count >= entry_capacity) {
            entry_capacity = entry_capacity * 2;
            DirEntry *new_entries = realloc(entries, entry_capacity * sizeof(DirEntry));
            if (!new_entries) {
                free(entries);
                free(directory_buffer);
                return 1;
            }
            entries = new_entries;
        }

        // need to copy name and null terminator
        strncpy(entries[entry_count].name, curr_pos, length_of_name);
        entries[entry_count].name[length_of_name] = '\0';

        // move onwards
        curr_pos = comma_pos + 1;

        if ((curr_pos + sizeof(uint)) > end_pos) {
            break;
        }
        // safer way to read innumber
        uint temp_inumber;
        memcpy(&temp_inumber, curr_pos, sizeof(uint));
        entries[entry_count].inumber = temp_inumber;
        // entries[entry_count].inumber = *(uint*)curr_pos; // read the inumber

        // get permissions so that it can be sorted according to the README
        uint entry_inumber = entries[entry_count].inumber;
        if (entry_inumber < max_num_inodes && inodes_array[entry_inumber] != NULL) {
            entries[entry_count].permissions = inodes_array[entry_inumber]->permissions;
        } else {
            entries[entry_count].permissions = 0; 
        }

        entry_count++;
        curr_pos += sizeof(uint); // move past the inumber
    }
    // parsing should now be complete
    free(directory_buffer);

    // sort entries alphabetically like stated in the README
    qsort(entries, entry_count, sizeof(DirEntry), compare_directory_entries);

    // now that its sorted, start printing the files using helper from lib.c
    for (uint i = 0; i < entry_count; ++i) {
        uint entry_inumber = entries[i].inumber;

        if (entry_inumber >= max_num_inodes || inodes_array[entry_inumber] == NULL) {
            continue;
        }
        Inode *entry_inode = inodes_array[entry_inumber];

        ls_print_file(entries[i].name, entry_inode->size, entry_inode->permissions, entry_inode->mtime, depth);

        // check if directory, then recurse immediately if it is
        if (S_ISDIR(entry_inode->permissions)) {
            int result = ls_print_recursive_helper(fp, inodes_array, max_num_inodes, entry_inumber, depth + 1);
            if (result == 1) {
                fprintf(stderr, "uh oh,  not returning right, ls_print_recursive_helper, line 350\n");
            }
        }
    }

    free(entries);

    return 0;
}

int lfs_ls(FILE *fp, Inode* inodes_array[], uint max_inode_num) {
    uint root_inumber = 0;

    if (root_inumber >= max_inode_num) {
        fprintf(stderr, "Error in ls, root inode not loaded or invalid\n");
        return 1;
    }

    if(inodes_array[root_inumber] == NULL){
        fprintf(stderr, "Error in ls, root inode not loaded or invalid\n");
        return 1;
    }

    // Inode *root_inode = inodes_array[root_inumber];

    printf("/\n"); // root directory printed first

    return ls_print_recursive_helper(fp, inodes_array, max_inode_num, root_inumber, 1);
}

int lfs_cat(FILE *fp, Inode* inodes_array[], uint max_inode_num, char *filepath){
    uint file_inode_num = path_to_inode(fp, inodes_array, max_inode_num, filepath);
    if(file_inode_num == 0){ //path to node fails and returned default value
        fprintf(stderr, "parse_lfs error: Failed to find file.\n");
        return 1;
    }

    if(file_inode_num >= max_inode_num){
        fprintf(stderr, "parse_lfs error: Failed to find file.\n");
        return 1;
    }

    if(inodes_array[file_inode_num] == NULL){
        fprintf(stderr, "parse_lfs error: Failed to find file.\n");
        return 1;
    }

    Inode *target_inode = inodes_array[file_inode_num];
    if(target_inode == NULL){
        fprintf(stderr, "Error: target inode is null");
        return 1;
    }

    if(S_ISDIR(target_inode->permissions)){
        fprintf(stderr, "File permissions incorrect\n");
        return 1;
    }

    const int BLOCK_SIZE = 4096;
    char buffer[BLOCK_SIZE];
    uint bytes_read = 0;
    uint remaining_bytes = target_inode->size;
    uint block_idx = 0;

    while(remaining_bytes > 0 && block_idx < target_inode->num_direct_blocks){// read until nothing remaining
        if (fseek(fp, target_inode->data_blocks[block_idx], SEEK_SET) != 0) {
            fprintf(stderr, "Failed to seek to the target inode data blocks\n");
            return 1;
        }
        uint bytes_to_read;
        if(remaining_bytes < BLOCK_SIZE){
            bytes_to_read = remaining_bytes;
        }else{
            bytes_to_read = BLOCK_SIZE;
        }

        size_t bytes_read_in = fread(buffer, 1, bytes_to_read, fp);

        if(bytes_read_in > 0){
            fwrite(buffer, 1, bytes_read_in, stdout);
            bytes_read += bytes_read_in;
            remaining_bytes -= bytes_read_in;
        }else{
            fprintf(stderr, "Failed to read blocks.\n");
        }
        block_idx++;
    }
    return 0;
}

int main(int argc, char ** argv)
{
    int result = 0;
    FILE *fp = NULL;
    CR *cr = NULL;   
    Imap **imaps = NULL; 
    uint num_imaps = 0; 
    Inode* inodes_array[MAX_INODES]; 
    // init inodes_array elements to NULL
    for(int i = 0; i < MAX_INODES; ++i) {
        inodes_array[i] = NULL;
    }


    if(argc < 3){
        fprintf(stderr, "Usage: %s <ls|cat> <image_file> [filepath_for_cat]\n", argv[0]);
        result = 1;
        goto cleanup;
    }

    char *operation = argv[1];
    // validate operation and argument count
    if (strcmp("ls", operation) == 0 && argc != 3) {
        fprintf(stderr, "Usage: %s ls <image_file>\n", argv[0]);
        result = 1;
        goto cleanup;
    } else if (strcmp("cat", operation) == 0 && argc != 4) {
        fprintf(stderr, "Usage: %s cat <filepath_in_image> <image_file>\n", argv[0]);
        result = 1;
        goto cleanup;
    } else if (strcmp("ls", operation) != 0 && strcmp("cat", operation) != 0) {
        fprintf(stderr, "Error: Unknown operation '%s'. Use 'ls' or 'cat' pls.\n", operation);
        result = 1;
        goto cleanup;
    }


    char *image_file = argv[2]; // always the second argument
    char *lfs_filepath_for_cat = NULL; // only used for cat

    if(strcmp("cat", operation) == 0){
        image_file = argv[3];
        lfs_filepath_for_cat = argv[2];
    }

    fp = fopen(image_file, "rb");
    if(!fp){
        fprintf(stderr, "parse_lfs error: Failed to find file.\n");
        result = 1;
        goto cleanup;
    }

    cr = get_checkpoint_region(fp);
    if (cr == NULL) {
        fprintf(stderr, "Error, checkpoint region returned null\n");
        result = 1;
        goto cleanup;
    }

    num_imaps = cr->num_entries;
    imaps = malloc(num_imaps * sizeof(Imap*));
    if (imaps == NULL) {
        fprintf(stderr, "Failed to allocate memory for imaps array\n");
        result = 1;
        goto cleanup;
    }
    // init allocated pointers to NULL
    for (uint i = 0; i < num_imaps; ++i) {
        imaps[i] = NULL;
    }

    uint max_num_inodes = 0; // keepin track of max number of inodes

    // load imaps
    for (uint i = 0; i < num_imaps; i++) {
        imaps[i] = read_imap(fp, cr->imaps[i].imap_disk_offset);
        if (imaps[i] == NULL) {
            fprintf(stderr, "Error, failed to load imap for the range %u - %u\n", cr->imaps[i].inumber_start, cr->imaps[i].inumber_end);
            result = 1;
            goto cleanup;
        }
        // update max_num_inodes
        if (cr->imaps[i].inumber_end >= max_num_inodes) {
            max_num_inodes = cr->imaps[i].inumber_end + 1; 
            if (max_num_inodes > MAX_INODES) {
                fprintf(stderr, "Error: Required inode number %u exceeds MAX_INODES limit (%d).\n", cr->imaps[i].inumber_end, MAX_INODES);
                result = 1;
                goto cleanup;
            }
        }
    }

    // load inodes from loaded imaps
    for (uint i = 0; i < num_imaps; i++) {
        Imap* curr_imap = imaps[i];
        for (uint j = 0; j < curr_imap->num_entries; j++) {
            uint curr_inumber = curr_imap->inodes[j].inode_number;
            uint curr_offset = curr_imap->inodes[j].inode_disk_offset;

            // check bounds before accessing inodes_array
            if (curr_inumber < MAX_INODES) {
                 // check to prevent redundant loading
                 if (inodes_array[curr_inumber] == NULL) {
                    inodes_array[curr_inumber] = read_inode(fp, curr_offset);
                    if (inodes_array[curr_inumber] == NULL) {
                        fprintf(stderr, "Error, failed to load inode %u\n", curr_inumber);
                        result = 1;
                        goto cleanup;
                    }
                 }
            } else {
                fprintf(stderr, "Error, inode number %u exceeds MAX_INODES limit (%d)\n", curr_inumber, MAX_INODES);
                result = 1;
                goto cleanup;
            }
        }
    } 

    if(strcmp("ls", operation) == 0){
        if(lfs_ls(fp, inodes_array, max_num_inodes) != 0){
            result = 1;
        }
    }else if(strcmp("cat", operation) == 0){
        if(lfs_cat(fp, inodes_array, max_num_inodes, lfs_filepath_for_cat) != 0){
            result = 1;
        }
    }

cleanup:
    // clean up inodes_array and the inode structs
    for (uint i = 0; i < MAX_INODES; ++i) {
        if (inodes_array[i]) {
            if (inodes_array[i]->data_blocks) {
                free(inodes_array[i]->data_blocks);
            }
            free(inodes_array[i]);
        }
    }

    // clean up imaps array and the imap structs
    if (imaps) {
        for (uint i = 0; i < num_imaps; ++i) {
            if (imaps[i]) { // check if this imap pointer is valid
                if (imaps[i]->inodes) {
                    free(imaps[i]->inodes); 
                }
                free(imaps[i]);
            }
        }
        free(imaps);
    }

    // clean up checkpoint region
    if (cr) {
        if (cr->imaps) {
            free(cr->imaps);
        }
        free(cr);
    }

    if (fp) {
        fclose(fp);
    }
    return result;
}
