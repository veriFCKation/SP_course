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
	int shift;
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
/*
void look(){
	if (file_descriptors == NULL) {return;}
	struct filedesc *start = *file_descriptors;
	
	//struct filedesc *buf = *file_descriptors;
	
	printf("--start = %ld\n", (long int)(*file_descriptors));
	printf("-look:\n");
	while (start != NULL){
		printf("-[%ld] desc_num=%d -> (is null?) %d\n", (long int)start, start->desc_num, (start->file == NULL));
		printf("--prev = [%ld]\n", (start->prev == NULL) ? -1 : (long int)start->prev);
		printf("--next = [%ld]\n", (start->next == NULL) ? -1 : (long int)start->next);
		if (start->next == NULL) break;
		start = start->next;
	}
	while (start != NULL){
		if (start->prev == NULL) break;
		start = start->prev;
	}
	printf("-done\n");
	printf("--start = %ld\n", (long int)(*file_descriptors));
}

void look_file(){
	printf("-look:\n");
	if (file_list == NULL) {printf("-done\n"); return; }
	struct file *start = file_list;
	while (start != NULL){
		printf("-[%ld] name_%s -> size = %ld\n", (long int)start, start->name, start->file_size);
		start = start->next;
	}
	printf("-done\n");
}


void look_block(struct file* curr){
	struct block *start = curr->block_list;
	printf("-check blocks (addr -> %ld):\n", (long int)curr);
	int i = 0;
	while (start != NULL){
		printf("-%d -> %d/512  addr->%ld\n", i, start->occupied, (long int)start);
		start = start->next;
		++i;
	}
	printf("-done\n");
}*/

struct filedesc * get_file_desc(int fd){ //returns pointer to filedesc
	printf("--finding %d\n", fd);
	if (file_descriptors == NULL) {printf("\nbad *\n"); return NULL;}
	
	struct filedesc *start = *file_descriptors;
	
	while (start != NULL){
		printf("-[%ld] desc_num=%d -> (is null?) %d\n", (long int)start, start->desc_num, (start->file == NULL));
		printf("--prev = [%ld]\n", (start->prev == NULL) ? -1 : (long int)start->prev);
		printf("--next = [%ld]\n", (start->next == NULL) ? -1 : (long int)start->next);
		if (start->desc_num == fd) {return start; }
		start = start->next;
	}
	printf("--dno\n");
	return NULL;
}
/////
int add_file_desc(){ //returns number of free slot in the list
	if (file_descriptors == NULL) {
		struct filedesc *new_desc = (struct filedesc *)malloc(sizeof(struct filedesc));
		new_desc->desc_num = 0;
		new_desc->fd_ptr = NULL;
		new_desc->shift = 0;
		file_descriptors = &new_desc;
		file_descriptor_capacity++;
		if (file_descriptors != NULL){//without this it dies (i don't know why)
			struct filedesc *start = *file_descriptors;
			while (start != NULL){
				start = start->next;
			}
		}
		return new_desc->desc_num;
	}
	
	struct filedesc *start = *file_descriptors;
	
	struct filedesc *end = NULL;
	int num = 0;
	while (start != NULL){
		if (start->file == NULL) { return start->desc_num;}
		num++;
		end = start;
		start = start->next;
	}
	
	struct filedesc *new_desc = (struct filedesc *)malloc(sizeof(struct filedesc));
	new_desc->desc_num = num;
	new_desc->fd_ptr = NULL;
	new_desc->shift = 0;
	new_desc->next = NULL;
	if (end != NULL){
		end->next = new_desc;
		new_desc->prev = end;
	}
	else{
		printf("--aaaaa\n");
		new_desc->prev = NULL;
		*file_descriptors = new_desc;
	}
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
		if (file_descriptors != NULL) printf("----- [%ld] - good\n", (long int)(*file_descriptors));
		struct file *new_file = (struct file *)malloc(sizeof(struct file));
		if (file_descriptors != NULL) printf("----- [%ld] - bad\n", (long int)(*file_descriptors));
		new_file->name = (char*)filename;
		new_file->refs = 0;
		new_file->max_file_size =  MAX_FILE_SIZE; 
		new_file->file_size = 0;
		new_file->block_list = NULL;
		new_file->last_block = NULL;
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
	if (new_fd == NULL){ 
		printf("-aaa");
		return -1;
	}
	new_fd->file = fptr;
	new_fd->file->refs++;
	new_fd->flag = (flags == UFS_CREATE || flags == 0) ? UFS_READ_WRITE : flags;
	file_descriptor_count++;
	return fd;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	if (fd < 0 || fd >= file_descriptor_capacity){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct filedesc *curr_fd = get_file_desc(fd);
	if (curr_fd == NULL || curr_fd->file == NULL){ ufs_error_code = UFS_ERR_NO_FILE; return -1;}
	if (!(curr_fd->flag == UFS_WRITE_ONLY || curr_fd->flag == UFS_READ_WRITE)) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	struct file *curr_file = curr_fd->file;
	if (curr_file == NULL){ ufs_error_code = UFS_ERR_NO_FILE; return -1;}
	if (curr_file->file_size + size > curr_file->max_file_size){
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}
	
	size_t written = 0;
	if (curr_fd->fd_ptr == NULL){
		curr_fd->fd_ptr = curr_file->block_list;
	}
	struct block * curr_block = curr_fd->fd_ptr;
	while (written < size){
		if (curr_block == NULL){
			struct block *new_block = (struct block *)malloc(sizeof(struct block));
			new_block->occupied = 0;
			new_block->memory = (char *)malloc(BLOCK_SIZE);
			new_block->next = NULL;
			curr_fd->shift = 0;
			if (curr_file->block_list == NULL){
				
				new_block->prev = NULL;
				curr_file->block_list = new_block;
				curr_file->last_block = new_block;
			}
			else{
				if (curr_file->last_block != NULL){
					curr_file->last_block = new_block;
					new_block->prev = curr_file->last_block;
				}
				else {
					new_block->prev = NULL;
					curr_file->last_block = new_block;
				}
			}
			curr_block = new_block;
		}
		size_t free_mem = BLOCK_SIZE - curr_fd->shift;
		size_t size_to_write = (free_mem > (size-written)) ? (size-written) : free_mem;
		//actual writting to curr_block->memory
		char* str = memcpy(&curr_block->memory[curr_fd->shift], &buf[written], size_to_write);
		//curr_file->file_size = curr_file->file_size - curr_block->occupied;
		if (curr_block->occupied < curr_fd->shift + size_to_write) 
			curr_block->occupied = curr_fd->shift + size_to_write;
		//curr_file->file_size = curr_file->file_size + curr_block->occupied;
		written = written + size_to_write;
		curr_fd->shift = curr_fd->shift + size_to_write;
		
		curr_fd->fd_ptr = curr_block;
		if (written >= curr_file->max_file_size)
			break;

		if (curr_block->next == NULL){
			struct block *new_block = (struct block *)malloc(sizeof(struct block));
			new_block->occupied = 0;
			new_block->memory = (char *)malloc(BLOCK_SIZE);
			new_block->next = NULL;
			new_block->prev = curr_block;
			curr_block->next = new_block;
			if (curr_block == curr_file->last_block)
				curr_file->last_block = new_block;
		}
		
		if (curr_fd->shift == BLOCK_SIZE) {
			curr_fd->shift = 0;
			curr_block = curr_block->next;
		}
	}
	return written;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{	
	if (fd < 0 || fd >= file_descriptor_capacity){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct filedesc *curr_fd = get_file_desc(fd);
	if (curr_fd == NULL || curr_fd->file == NULL){ ufs_error_code = UFS_ERR_NO_FILE; return -1;}
	if (!(curr_fd->flag == UFS_READ_ONLY || curr_fd->flag == UFS_READ_WRITE)) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	struct file *curr_file = curr_fd->file;
	if (curr_file == NULL){ ufs_error_code = UFS_ERR_NO_FILE; return -1;}
	
	size_t readed = 0;
	if (curr_fd->fd_ptr == NULL){
		curr_fd->fd_ptr = curr_file->block_list;
	}
	struct block * curr_block = curr_fd->fd_ptr;
	while (readed < size){
	
		if (curr_block == NULL){
			break; //we can read empty file
		}
		
		if (curr_block->occupied == 0) {break;}
		if (curr_fd->shift == BLOCK_SIZE){
			curr_fd->shift = 0;
			curr_block = curr_block->next;
			continue;
		}
		size_t rest_mem = curr_block->occupied - curr_fd->shift;
		size_t size_to_read = (rest_mem > (size-readed)) ? (size-readed) : rest_mem;
		//actual reading to buf
		char* str = memcpy(&buf[readed], &curr_block->memory[curr_fd->shift], size_to_read);

		readed = readed + size_to_read;
		curr_fd->shift = curr_fd->shift + size_to_read;
		
		curr_fd->fd_ptr = curr_block;
		if (readed >= curr_file->file_size)
			break;
	}
	return readed;
}
/*
93981824431872 -> 139755526897728
94510942177024 -> 140485849333824
*/
int
ufs_close(int fd)
{
	if (fd < 0 || fd >= file_descriptor_capacity){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct filedesc *file_desc= get_file_desc(fd);
	if (file_desc == NULL || file_desc->file == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	
	struct file* curr_file = file_desc->file;
	file_desc->file = NULL;
	file_desc->shift = 0;
	file_desc->fd_ptr = NULL;
	if (curr_file != NULL) curr_file->refs--;
	file_descriptor_count--;
	file_desc= get_file_desc(fd);
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
		free(cb->memory);
		if (cb->next == NULL) break;
		cb = cb->next;
		free(cb->prev);
	}
	free(cb);
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
