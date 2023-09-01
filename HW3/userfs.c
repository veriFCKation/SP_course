#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/** Last block in the list.  */
	struct block *last_block;
	
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;
	
	
	size_t max_file_size;
	size_t file_size;
	/* PUT HERE OTHER MEMBERS */
	
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;
	enum open_flags flag;
	struct block* fd_ptr;
	/* PUT HERE OTHER MEMBERS */
	
	struct filedesc *prev;
	struct filedesc *next;
	int desc_num;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

struct filedesc * get_file_desc(int fd){ //returns pointer to filedesc
	if (file_descriptors == NULL) {printf("\nbad *\n"); return NULL;}
	
	struct filedesc *start = *file_descriptors;
	
	while (start != NULL){
		if (start->desc_num == fd) {return start; }
		start = start->next;
	}
	return NULL;
}

int del_file_desc(int fd){ //returns success status
	struct filedesc *fd_ptr = get_file_desc(fd);
	if (fd_ptr == NULL) {return -1;}
	
	fd_ptr->file = NULL;
	return 0;
}

int add_file_desc(){ //returns number of free slot in the list
	if (file_descriptors == NULL) {
		struct filedesc *new_desc = (struct filedesc *)malloc(sizeof(struct filedesc));
		new_desc->desc_num = 0;
		file_descriptors = &new_desc;
		file_descriptor_capacity++;
		if (file_descriptors != NULL){//without this it dies (i don't know why)
			struct filedesc *start = *file_descriptors;
			while (start != NULL){
				start = start->next;
			}
		}
		return 0;
	}
	
	
	struct filedesc *start = *file_descriptors;
	struct filedesc *end = NULL;
	while (start != NULL){
		if (start->file == NULL) { return start->desc_num;}
		end = start;
		start = start->next;
	}
	
	struct filedesc *new_desc = (struct filedesc *)malloc(sizeof(struct filedesc));
	new_desc->desc_num = file_descriptor_capacity;
	end->next = new_desc;
	new_desc->prev = end;
	file_descriptor_capacity++;
	return new_desc->desc_num;
}


int
ufs_open(const char *filename, int flags)
{
	bool exist = false;
	struct file *fptr = file_list;
	while (fptr != NULL){
		if (fptr->name == filename){
			exist = true;
			break;
		}
		fptr = fptr->next;
	}
	
	if (!exist && flags != UFS_CREATE){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	
	if (!exist){
		struct file *new_file = (struct file *)malloc(sizeof(struct file));
		new_file->name = (char*)filename;
		new_file->refs = 0;
		new_file->max_file_size =  MAX_FILE_SIZE; 
		new_file->file_size = 0;
		if (file_list == NULL)
			file_list = new_file;
		fptr = new_file;
	}
	
	if (fptr == NULL){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	
	int fd = add_file_desc();
	struct filedesc* new_fd = get_file_desc(fd);
	if (new_fd == NULL){ printf("\naaa\n"); return 0;}
	new_fd->file = fptr;
	new_fd->file->refs++;
	new_fd->flag = (flags == UFS_CREATE) ? UFS_READ_WRITE : flags;
	return fd+1;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	if (fd <= 0 || fd > file_descriptor_count){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	fd--;
	struct filedesc *curr_fd = file_descriptors[fd];
	struct file *curr_file = curr_fd->file;
	if (!(curr_fd->flag & UFS_WRITE_ONLY || curr_fd->flag & UFS_READ_WRITE)){
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	
	if (curr_file->file_size + size > curr_file->max_file_size){
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}
	size_t unwritten_s = size;
	char* curr_buf = (char*)buf;
	while (unwritten_s > 0){
		struct block* curr_block = curr_fd->fd_ptr;
		size_t real_write = unwritten_s;
		if (BLOCK_SIZE - curr_block->occupied < real_write){
			real_write = BLOCK_SIZE - curr_block->occupied;
		}
		memcpy(&curr_block->memory[BLOCK_SIZE],curr_buf, real_write);
		unwritten_s = unwritten_s - real_write;
		curr_fd->fd_ptr->occupied = curr_fd->fd_ptr->occupied + real_write;
		if (curr_fd->fd_ptr->occupied == BLOCK_SIZE){
			if (curr_block->next == NULL){
				struct block* new_b = (struct block*)malloc(sizeof(struct block));
				new_b->prev = curr_block;
				new_b->next = NULL;
				new_b->memory = (char*)malloc(sizeof(char)* BLOCK_SIZE);
				
				curr_block->next = new_b;
			}
			curr_fd->fd_ptr = curr_block->next;
		}
	}
	curr_file->file_size = curr_file->file_size + (size - unwritten_s);
	return (size - unwritten_s);
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{	
	if (fd <= 0 || fd > file_descriptor_count){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	fd--;
	struct filedesc *curr_fd = file_descriptors[fd];
	struct file *curr_file = curr_fd->file;
	if (!(curr_fd->flag & UFS_READ_ONLY || curr_fd->flag & UFS_READ_WRITE)){
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	size_t readed_s = 0;
	char* curr_buf = buf;
	while (readed_s < size){
		struct block* curr_block = curr_fd->fd_ptr;
		size_t real_read = size - readed_s;
		if (real_read > BLOCK_SIZE - curr_block->occupied){
			real_read = BLOCK_SIZE - curr_block->occupied;
		}
		memcpy(curr_buf, &curr_block->memory[BLOCK_SIZE], real_read);
		readed_s = readed_s + real_read;
		curr_fd->fd_ptr->occupied = curr_fd->fd_ptr->occupied + real_read;
		if (curr_fd->fd_ptr->occupied == BLOCK_SIZE){
			curr_fd->fd_ptr = curr_block->next;
		}
		if (curr_fd->fd_ptr == NULL){
			return 0;
		}
		
	}
	return readed_s;
}

int
ufs_close(int fd)
{
	if (fd <= 0 || fd > file_descriptor_capacity){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	fd--;
	struct filedesc* file_desc = get_file_desc(fd);
	if (file_desc == NULL || file_desc->file == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct file* curr_file = file_desc->file;
	file_desc->file = NULL;
	curr_file->refs--;
	/*if (curr_file->refs == 0){
		ufs_delete(curr_file->name);
	}*/
	return 0;
}

int
ufs_delete(const char *filename)
{
	bool exist = false;
	struct file *fptr = file_list;
	while (fptr != NULL){
		if (fptr->name == filename){
			exist = true;
			break;
		}
		fptr = fptr->next;
	}
	
	if (!exist){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct block* cb = fptr->block_list;
	while (cb != NULL){
		struct block* nb = cb->next;
		free(cb->memory);
		free(cb);
		cb = nb;
		free(nb);
	}
	if (fptr->prev == NULL && fptr->next == NULL) file_list = NULL;
	if (fptr->prev != NULL) fptr->prev->next = fptr->next;
	if (fptr->next != NULL) fptr->next->prev = fptr->prev;
	return 0;
}

int
ufs_resize(int fd, size_t new_size){
	if (fd <= 0 || fd > file_descriptor_count){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	fd--;
	struct filedesc *curr_fd = file_descriptors[fd];
	struct file *curr_file = curr_fd->file;
	if (new_size > curr_file->max_file_size){
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}
	struct block* cb = curr_file->block_list;
	size_t curr_file_size = 0;
	while (cb != NULL && curr_file_size + cb->occupied <= new_size){
		curr_file_size = curr_file_size + cb->occupied;
		cb = cb->next;
	}
	while (cb != NULL){
		struct block* nb = cb->next;
		free(cb->memory);
		free(cb);
		cb = nb;
		free(nb);
	}
	curr_file->file_size = curr_file_size;
	curr_file->max_file_size = new_size;
	return 0;
}

void
ufs_destroy(void)
{
	for (int fd = 0; fd < file_descriptor_count; ++fd){
		ufs_close(fd);
	}
	free(*file_descriptors);
	struct file *cf = file_list;
	while (cf != NULL){
		struct file* nf = cf->next;
		ufs_delete(cf->name);
		free(cf);
		cf = nf;
		free(nf);
	}
	free(file_list);
}
