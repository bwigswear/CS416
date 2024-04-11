/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here

struct superblock *sblock;

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	bitmap_t bitmap = malloc(BLOCK_SIZE);
	bio_read(sblock->i_bitmap_blk, bitmap) ;

	// Step 2: Traverse inode bitmap to find an available slot
	int max_inode = MAX_INUM;
	for(int i = 0; i < max_inode; i++){
		//If inode is available set the bit and return the inode #
		if(get_bitmap(bitmap, i) == 0){

			// Step 3: Update inode bitmap and write to disk 
			set_bitmap(bitmap, i);
			bio_write(sblock->i_bitmap_blk, bitmap);
			return i;
		}
	}
	return -1;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
	// Step 1: Read data block bitmap from disk
	bitmap_t bitmap = malloc(BLOCK_SIZE);
	bio_read(sblock->d_bitmap_blk, bitmap);


	// Step 2: Traverse data block bitmap to find an available slot
	int max_block = MAX_DNUM;
	for(int i = 0; i < max_block; i++){
		//If inode is available set the bit and return the inode #
		if(get_bitmap(bitmap, i) == 0){
			
			
			// Step 3: Update data block bitmap and write to disk 
			set_bitmap(bitmap, i);
			bio_write(sblock->d_bitmap_blk, bitmap);
			return i;
		}
	}
	return -1;
}
/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
	
  	// Step 1: Get the inode's on-disk block number
	int startblock = sblock->i_start_blk;
	int whichblock = startblock + ino / (BLOCK_SIZE / sizeof(inode));

	// Step 2: Get offset of the inode in the inode on-disk block
	struct inode* thisblock = malloc(BLOCK_SIZE);
	bio_read(whichblock, thisblock);
	int offset = ino % (BLOCK_SIZE / sizeof(inode));

	// Step 3: Read the block from disk and then copy into inode structure
	*inode = thisblock[offset];

	free(thisblock);

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	//Assume that if inode is new, get_avail_blkno is already called
	
	// Step 1: Get the block number where this inode resides on disk
	int startblock = sblock->i_start_blk;
	int whichblock = startblock + ino / (BLOCK_SIZE / sizeof(inode));

	// Step 2: Get the offset in the block where this inode resides on disk
	struct inode* thisblock = malloc(BLOCK_SIZE);
	bio_read(whichblock, thisblock);
	int offset = ino % (BLOCK_SIZE / sizeof(inode));
	
	// Step 3: Write inode to disk 
	thisblock[offset] = *inode;
	bio_write(whichblock, thisblock);
	free(thisblock);

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

	// Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode curr_dir_inode;
	readi(ino, &curr_dir_inode);

	// Step 2: Get data block of current directory from inode
	struct dirent* curr_dir = malloc(BLOCK_SIZE);
	int currentlink = 0;
	while(curr_dir_inode.direct_ptr[currentlink] != 0 && currentlink < 16){

		// Step 3: Read directory's data block and check each directory entry.
		bio_read(curr_dir_inode.direct_ptr[currentlink], curr_dir);
		for (int i = 0; i < (BLOCK_SIZE / sizeof(struct dirent)); i++){
			//single directory entry
			struct dirent curr_entry = curr_dir[i];
			if(curr_entry.valid == 0){continue;}
			//If the name matches, then copy directory entry to dirent structure
			if (strcmp(curr_entry.name, fname) == 0) {
				//free(curr_dir);
				*dirent = curr_entry;
				return 0;
			}
		}
		currentlink++;
	}

	
	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {


	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	struct dirent* curr_dir = malloc(BLOCK_SIZE);
	int currentlink = 0;
	while(dir_inode.direct_ptr[currentlink] != 0 && currentlink < 16){

		//Set current directory for reading
		bio_read(dir_inode.direct_ptr[currentlink], curr_dir);
		for (int i = 0; i < (BLOCK_SIZE / sizeof(struct dirent)); i++){
			struct dirent curr_entry = curr_dir[i];

			// Step 2: Check if fname (directory name) is already used in other entries
			if (strcmp(curr_entry.name, fname) == 0) {
				free(curr_dir);
				//file with same name exists -> cannot add
				return -1;
			}

			// Step 3: Add directory entry in dir_inode's data block and write to disk
			// Allocate a new data block for this directory if it does not exist
			if(curr_entry.valid == 0){
				//reached the end of entries, doesn't exist -> add entry here
				// Update directory inode
				curr_entry.ino = f_ino;
				curr_entry.len = name_len;
				curr_entry.valid = 1;
				strcpy(curr_entry.name, fname);
				// Write directory entry
				bio_write(dir_inode.direct_ptr[currentlink], curr_dir);
				
				return 0;
			}
		}
		currentlink++;
		//If next block does not exist, get a new one and create the link
		if(dir_inode.direct_ptr[currentlink] == NULL && currentlink < 16){
			dir_inode.vstat.st_blocks++;
			dir_inode.direct_ptr[currentlink] = get_avail_blkno();
		}
	}

	
	
	return -1;

}

//WE CAN SKIP THIS -----------------
int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way


	//get inode of path root
	struct inode rootinode;
	readi(ino, &rootinode);

	//get directory associated with inode
	struct dirent* root_block = malloc(BLOCK_SIZE);
	bio_read(rootinode.direct_ptr[0], root_block);
	struct dirent curr_dir = root_block[0];
	char *entry = malloc(64);
	entry = strtok(path, "/");

	//iterate through path, checking each directory for the listed file name
	while (entry != NULL) {
		if(dir_find(curr_dir.ino, entry, 0, &curr_dir) != 0){
			free(entry);
			free(root_block);
			return -1;
		}
		entry = strtok(NULL, "/");
    	}
	free(entry);
	free(root_block);
	readi(curr_dir.ino, inode);
	return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	
	sblock = malloc(BLOCK_SIZE);
	sblock->magic_num = MAGIC_NUM;
	sblock->max_inum = MAX_INUM;
	sblock->max_dnum = MAX_DNUM;
	//inode bmap is 2nd block, data bmap is 3rd, inode start block is 4th, data start block is after inodes
	sblock->i_bitmap_blk = 1;
	sblock->d_bitmap_blk = 2;
	sblock->i_start_blk = 3;
	sblock->d_start_blk = sblock->i_start_blk + MAX_INUM * sizeof(struct inode) / BLOCK_SIZE;
	bio_write(0, sblock);

	// initialize inode bitmap
	bitmap_t inode_bitmap = (bitmap_t)calloc(BLOCK_SIZE, 1);

	// initialize data block bitmap
	bitmap_t block_bitmap = (bitmap_t)calloc(BLOCK_SIZE, 1);

	// update bitmap information for root directory
	// write superblock information
	set_bitmap(block_bitmap, 0); //Set first block as used for superblock
	set_bitmap(block_bitmap, 1); //Set second block as used for inode bitmap
	set_bitmap(block_bitmap, 2); //Set third block as used for data block bitmap
	for(int i = 3; i <( 3 + MAX_INUM * sizeof(struct inode) / BLOCK_SIZE); i++){
		set_bitmap(block_bitmap, i); //Set inode blocks as used
	}
	set_bitmap(inode_bitmap, 0);

	
	bio_write(sblock->i_bitmap_blk, inode_bitmap) ;
	bio_write(sblock->d_bitmap_blk, block_bitmap) ;

	// update inode for root directory

	// Initialize root inode
	int root_dblock_start = get_avail_blkno();
	struct inode* inode = malloc(BLOCK_SIZE) ;
	*inode = (struct inode) {
	    .ino = 0,
	    .valid = 1,
		//.size = 0,
		.type = 1,
	    .link = 0,
	    .direct_ptr[0] = root_dblock_start,
	    .vstat = {
	        .st_mode = __S_IFDIR | 0755,
	        .st_nlink = 2,
	        .st_mtime = time(&inode->vstat.st_mtime),
	        .st_blksize = BLOCK_SIZE,
			.st_blocks = 1
	    }
	};
	bio_write(sblock->i_start_blk, inode);
	free(inode);

	// Initialize root directory entry
	struct dirent* root = malloc(BLOCK_SIZE);
	*root = (struct dirent) {
	    .ino = 0,
	    .valid = 1,
	    .name = "."
	};
	*(root + 1) = (struct dirent) {
	    .ino = 0,
	    .valid = 1,
	    .name = ".."
	};
	bio_write(sblock->d_start_blk, root);
	free(root);


	return 0;
	
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

    
	// Step 1a: If disk file is not found, call mkfs
	int result = dev_open(diskfile_path);
	if (result != 0){
		rufs_mkfs();
		return 0;
	}

	// Step 1b: If disk file is found, just initialize in-memory data structures
	// and read superblock from disk
	sblock = malloc(sizeof(struct superblock));
	struct superblock* sblockzz = malloc(sizeof(BLOCK_SIZE));
	bio_read(0, sblockzz);
	sblock = &sblockzz[0];
	free(sblockzz);


	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(sblock);
	// Step 2: Close diskfile
	dev_close();

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(inode));
	if(get_node_by_path(path, 0, inode) < 0){
		free(inode); 
		return 2;
	}



	// Step 2: fill attribute of file into stbuf from inode

		//Project instructions say populate certain parameters, but pretty sure we can copy whole struct
		*stbuf = inode->vstat;
		
		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

		//These were the listed things in the project description so I'm going to leave them here
		//There shouldn't be a conflict with copying the whole vstat as well
		stbuf->st_uid = inode->vstat.st_uid;
		stbuf->st_gid = inode->vstat.st_gid;
		stbuf->st_nlink = inode->vstat.st_nlink;
		stbuf->st_size = inode->vstat.st_size;
		stbuf->st_mtime = inode->vstat.st_mtime;
		stbuf->st_mode = inode->vstat.st_mode;
		
	free(inode);
	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	
	struct inode bruh;
	if(get_node_by_path(path, 0, &bruh) != 0){
		// Step 2: If not find, return -1
		//free(inode);
		return -1;
	}
	//free(inode);

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	

	// Step 1: Call get_node_by_path() to get inode from path
	//struct inode *inode = malloc(sizeof(inode));
	struct inode bruh;
	if(get_node_by_path(path, 0, &bruh) != 0){
		//If not find, return -1
		return -1;
	}
	
	// Step 2: Read directory entries from its data blocks, and copy them to filler

	//Get data block of current directory from inode
	int currentlink = 0;
	struct dirent* curr_dir = malloc(BLOCK_SIZE);
	while(bruh.direct_ptr[currentlink] != 0 && currentlink < 16){

		//Read directory's data block and check each directory entry.
		bio_read(bruh.direct_ptr[currentlink], curr_dir);
		for (int i = 0; i < (BLOCK_SIZE / sizeof(struct dirent)); i++){
			//single directory entry
			struct dirent curr_entry = curr_dir[i];
			//get corresponding inode
			struct inode curr_entry_inode;
			readi(curr_entry.ino, &curr_entry_inode);
			//run filler command parameter with buffer, name of curr item, item's vstat 
			filler(buffer, curr_entry.name, &curr_entry_inode.vstat, 0);

		}
		currentlink++;
	}
	
	return 0;

}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	char *basename = malloc(strlen(path));
	char *dirname = malloc(strlen(path));
	char *last_slash = strchr(path, '/');
	if(last_slash == NULL){
		free(basename);
		free(dirname);
		return -1;
	}else{
		basename = strdup(last_slash + 1);
		size_t len = last_slash - path;
		dirname = strndup(path, len);
	}

	// Step 2: Call get_node_by_path() to get inode of parent directory

	struct inode parent_inode;
	get_node_by_path(basename, 0, &parent_inode);

	// Step 3: Call get_avail_ino() to get an available inode number

	int new_inode_number = get_avail_ino();

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	dir_add(parent_inode, new_inode_number, dirname, 0);

	// Step 5: Update inode for target directory

	struct inode new_inode;
	readi(new_inode_number, &new_inode);
	int first_data_block = get_avail_blkno();

	new_inode = (struct inode) {
	    .ino = new_inode_number,
	    .valid = 1,
		//.size = 0,
		.type = S_IFDIR,
	    .link = 0,
	    .direct_ptr[0] = first_data_block,
	    .vstat = {
	        .st_mode = __S_IFDIR | 0755,
	        .st_nlink = 2,
	        .st_mtime = time(&new_inode.vstat.st_mtime),
	        .st_blksize = BLOCK_SIZE,
			.st_blocks = 1
	    }
	};

	struct dirent* thing = malloc(BLOCK_SIZE);
	*thing = (struct dirent) {
	    .ino = new_inode_number,
	    .valid = 1,
	    .name = "."
	};
	*(thing + 1) = (struct dirent) {
	    .ino = parent_inode.ino,
	    .valid = 1,
	    .name = ".."
	};
	bio_write(new_inode.direct_ptr[0], thing);
	free(thing);

	// Step 6: Call writei() to write inode to disk
	writei(new_inode_number, &new_inode); 
	free(dirname);
	free(basename);
	return 0;
}

//WE CAN SKIP THIS ------------------------------------
static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {


	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	char *basename = malloc(strlen(path));
	char *dirname = malloc(strlen(path));
	char *last_slash = strchr(path, '/');
	if(last_slash == NULL){
		free(basename);
		free(dirname);
		return -1;
	}else{
		basename = strdup(last_slash + 1);
		size_t len = last_slash - path;
		dirname = strndup(path, len);
	}
	// Step 2: Call get_node_by_path() to get inode of parent directory

	struct inode parent_inode;
	get_node_by_path(basename, 0, &parent_inode);

	// Step 3: Call get_avail_ino() to get an available inode number

	int new_inode_number = get_avail_ino();

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	dir_add(parent_inode, new_inode_number, dirname, 0);

	// Step 5: Update inode for target file

	struct inode new_inode;
	readi(new_inode_number, &new_inode);
	int first_data_block = get_avail_blkno();

	new_inode = (struct inode) {
	    .ino = new_inode_number,
	    .valid = 1,
		//.size = 0,
		.type = S_IFREG,
	    .link = 0,
	    .direct_ptr[0] = first_data_block,
	    .vstat = {
	        .st_mode = __S_IFREG | 0755,
	        .st_nlink = 2,
	        .st_mtime = time(&new_inode.vstat.st_mtime),
	        .st_blksize = BLOCK_SIZE,
			.st_blocks = 1
	    }
	};


	// Step 6: Call writei() to write inode to disk
	writei(new_inode_number, &new_inode);
	free(basename);
	free(dirname);
	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode locating_inode;
	if(get_node_by_path(path, 0, &locating_inode) != 0){
		// Step 2: If not find, return -1
		return -1;
	}
	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(inode));
	get_node_by_path(path, 0, inode);
	
	// Step 2: Based on size and offset, read its data blocks from disk
	int bytescopied = 0;
	int copiesneeded = (size - offset) / BLOCK_SIZE + 1;
	int currentblock = offset / BLOCK_SIZE;
	//char* temp = malloc(BLOCK_SIZE); Don't think we need this if we copy directly
	
	for(int i = currentblock; i < currentblock + copiesneeded; i++){
		if(i < 16){
			bio_read(inode->direct_ptr[i], buffer + bytescopied);
			bytescopied+=BLOCK_SIZE;
		}else{
			bio_read(inode->indirect_ptr[i - 16], buffer + bytescopied);
			bytescopied+=(BLOCK_SIZE * (BLOCK_SIZE / sizeof(int)));
		}
	}
	
	// Step 3: copy the correct amount of data from offset to buffer
	
	// Note: this function should return the amount of bytes you copied to buffer
	free(inode);
	return bytescopied;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(inode));
	get_node_by_path(path, 0, inode);
	
	// Step 2: Based on size and offset, read its data blocks from disk
	int bytescopied = 0;
	int copiesneeded = (size - offset) / BLOCK_SIZE + 1;
	int currentblock = offset / BLOCK_SIZE;
	
	// Step 3: Write the correct amount of data from offset to disk
	for(int i = currentblock; i < currentblock + copiesneeded; i++){
		if(i < 16){
			bio_write(inode->direct_ptr[i], buffer + bytescopied);
			bytescopied+=BLOCK_SIZE;
		}else{
			//Each direct pointer is an int, so an indirect_ptr will point to BLOCK_SIZE / sizeof(int) blocks
			bio_write(inode->indirect_ptr[i - 16], buffer + bytescopied);
			bytescopied+=(BLOCK_SIZE * (BLOCK_SIZE / sizeof(int)));
		}
	}
	
	// Step 4: Update the inode info and write it to disk
	inode->size += bytescopied;
	writei(inode->ino, inode);
	
	// Note: this function should return the amount of bytes you write to disk
	free(inode);
	return bytescopied;
}


//WE CAN SKIP THIS ------------------------------------
static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

