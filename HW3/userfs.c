#include "userfs.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
	int file_size;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;
	/* PUT HERE OTHER MEMBERS */
	
	struct filedesc *next;
	struct filedesc *prev;
	int desc_num;
	enum open_flags file_flags;
	
	struct block* curr_block;
	int shift;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc *file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

struct file* get_file_by_name(const char* filename){
	struct file *p = file_list;
	while (p != NULL){
		if (strcmp(p->name, filename) == 0){ break;}
		p = p->next;
	}
	return p;
}

struct filedesc* get_desc_by_num(int descnum){
	struct filedesc *p = file_descriptors;
	while (p != NULL){
		if (p->desc_num == descnum){ break;}
		p = p->next;
	}
	return p;
}

struct file* create_file(const char* filename){
	struct file* file = (struct file *)malloc(sizeof(struct file));
	file->name = (char *)malloc(sizeof(char) *32);
	strcpy(file->name, filename);
	file->block_list = NULL;
	file->last_block = NULL;
	file->refs = 0;
	file->file_size = 0;
	file->next = NULL;
	
	struct file *p = file_list;
	while (p != NULL){
		if (p->next == NULL) {break;}
		p = p->next;
	}
	file->prev = p;
	if (p != NULL){ p->next = file;}
	if (file_list == NULL){ file_list = file; }
	
	return file;	
}

struct filedesc* insert_filedesc(struct file* file_){
	struct filedesc *p = file_descriptors;
	struct filedesc *exist_desc = NULL;
	while (p != NULL){
		exist_desc = p;
		if (p->file == NULL){ break;}
		p = p->next;
	}
	if (p == NULL){
		struct filedesc *new_desc = (struct filedesc *)malloc(sizeof(struct filedesc));
		new_desc->desc_num = file_descriptor_capacity;
		file_descriptor_capacity++;
		new_desc->prev = exist_desc;
		new_desc->next = NULL;
		if (exist_desc != NULL){ exist_desc->next = new_desc; }
		
		exist_desc = new_desc;
	}
	exist_desc->file = file_;
	exist_desc->curr_block = NULL;
	exist_desc->shift = 0;
	
	if (file_descriptors == NULL) {file_descriptors = exist_desc; }
	return exist_desc;
}

struct block* add_new_block(struct file* file){
	struct block* new_block = (struct block *)malloc(sizeof(struct block));
	new_block->memory = (char *)malloc(BLOCK_SIZE);
	new_block->occupied = 0;
	new_block->next = NULL;
	new_block->prev = file->last_block;
	if (file->last_block != NULL) {file->last_block->next = new_block;}
	file->last_block = new_block;
	if (file->block_list == NULL) {file->block_list = new_block;}
	return new_block;
}

int
ufs_open(const char *filename, int flags)
{
	struct file* exist_file = get_file_by_name(filename);
	if (exist_file == NULL){
		if (flags != UFS_CREATE){
			ufs_error_code = UFS_ERR_NO_FILE;
			return -1;
		}
		exist_file = create_file(filename);
	}
	if (exist_file == NULL){ return -1;}
	struct filedesc *file_desc = insert_filedesc(exist_file);
	if (file_desc == NULL){ return -1; }
	file_descriptor_count++;
	if (flags == 0)
		file_desc->file_flags = UFS_READ_WRITE;
	else
		file_desc->file_flags = flags;
	exist_file->refs++;
	return file_desc->desc_num;
}

void destroy_file(struct file *file){
	if (file == NULL) return;
	struct block *curr = file->last_block;
	struct block *buf = NULL;
	while (curr != NULL){
		buf = curr->prev;
		free(curr->memory);
		free(curr);
		curr = buf;
	}
	free(file->name);
}

int
ufs_close(int fd)
{
	if (fd < 0 || fd >= file_descriptor_capacity){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct filedesc *desc = get_desc_by_num(fd);
	if (desc == NULL || desc->file == NULL){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	
	desc->file->refs--;
	
	desc->file = NULL;
	return 0;
}

int
ufs_delete(const char *filename)
{
	struct file* file = get_file_by_name(filename);
	if (file == NULL){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	
	if (file->next != NULL){ file->next->prev = file->prev;}
	if (file->prev != NULL){ file->prev->next = file->next;}
	if (file->prev == NULL && file->next == NULL) {file_list = NULL;}
	
	destroy_file(file);
	free(file);
	return 0;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{ 
	struct filedesc *desc = get_desc_by_num(fd);
	if (desc == NULL || desc->file == NULL){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	if (desc->file_flags == UFS_READ_ONLY){
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	if (desc->file->file_size + size > MAX_FILE_SIZE){
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}
	if (desc->curr_block == NULL) {
		desc->curr_block = desc->file->last_block;
	}
	struct block *curr = desc->curr_block;
	char *buf_write = (char *)buf;
	size_t written = 0;
	size_t size_to_write = 0;
	int num = 0;
	while (written < size){
		if (desc->shift >= BLOCK_SIZE){ 
			curr = curr->next;
			desc->shift = 0;
		}
		if (curr == NULL){
			curr = add_new_block(desc->file);
			desc->shift = 0;
		}
		num = num + 1;
		size_to_write = (size - written);
		if (curr->occupied < desc->shift) { desc->shift = curr->occupied;}
		if ((int)size_to_write + desc->shift > BLOCK_SIZE){
			size_to_write = (size_t)(BLOCK_SIZE - desc->shift);
		}
		char *to = curr->memory + desc->shift;
		memcpy(to, buf_write, size_to_write);
		written = written + size_to_write;
		desc->shift = desc->shift + (int)size_to_write;
		if (desc->shift > curr->occupied){
			desc->file->file_size = desc->file->file_size + (desc->shift - curr->occupied);
			curr->occupied = desc->shift;
		}
		buf_write = buf_write + (int)size_to_write;
		desc->curr_block = curr;
		if (desc->file->file_size == MAX_FILE_SIZE) {break;}
	}
	return written;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	struct filedesc *desc = get_desc_by_num(fd);
	if (desc == NULL || desc->file == NULL){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	if (desc->file_flags == UFS_WRITE_ONLY){
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	if (desc->curr_block == NULL) {desc->curr_block = desc->file->block_list;}
	if (desc->shift == BLOCK_SIZE){
		desc->curr_block = desc->curr_block->next;
		desc->shift = 0;
	}
	struct block *curr = desc->curr_block;
	char *buf_to_write = buf;
	size_t readed = 0;
	size_t size_to_read = 0;
	int num = 0;
	while (readed < size){
		if (curr == NULL){break;}
		num = num + 1;
		size_to_read = (size - readed);
		if ((int)size_to_read > curr->occupied - desc->shift){
			size_to_read = (size_t)(curr->occupied - desc->shift);
		}
		char *from = curr->memory + desc->shift;
		memcpy(buf_to_write, from, size_to_read);
		readed = readed + size_to_read;
		buf_to_write = buf_to_write + (int)size_to_read;
		desc->shift = desc->shift + (int)size_to_read;
		if (desc->shift >= curr->occupied || desc->shift >= BLOCK_SIZE){
			curr = curr->next;
			if (curr != NULL){
				desc->curr_block = curr;
				desc->shift = 0;
			}
		}
	}
	return readed;
}

void
ufs_destroy(void)
{
	struct filedesc *fd = file_descriptors;
	struct filedesc *buf = NULL;
	while (fd != NULL){
		buf = fd->next;
		destroy_file(fd->file);
		free(fd->file);
		free(fd);
		fd = buf;
	}
	
}

#ifdef NEED_RESIZE

/**
 * Resize a file opened by the file descriptor @a fd. If current
 * file size is less than @a new_size, then new empty blocks are
 * created and positions of opened file descriptors are not
 * changed. If the current size is bigger than @a new_size, then
 * the blocks are truncated. Opened file descriptors behind the
 * new file size should proceed from the new file end.
 **/
int
ufs_resize(int fd, size_t new_size){
	struct filedesc *desc = get_desc_by_num(fd);
	if (desc == NULL || desc->file == NULL){
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	
	struct file *file = desc->file;
	if ((int)new_size == file->file_size){ return 0;}
	if ((int)new_size > file->file_size){ //add empty blocks
		if ((int)new_size > MAX_FILE_SIZE){
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
		}

		int need_to_add = (int)new_size - file->file_size;
		if (need_to_add >= BLOCK_SIZE - file->last_block->occupied){
			need_to_add = need_to_add - (BLOCK_SIZE - file->last_block->occupied);
			file->last_block->occupied = BLOCK_SIZE;
		}
		while (need_to_add > 0){
			add_new_block(file);
			if (need_to_add >= BLOCK_SIZE - file->last_block->occupied){
				need_to_add = need_to_add - (BLOCK_SIZE - file->last_block->occupied);
				file->last_block->occupied = BLOCK_SIZE;
			}
			else{
				file->last_block->occupied = need_to_add;
				need_to_add = 0;
			}
		}
	}
	else{ //cut existed blocks
		struct block *curr = file->last_block;
		int need_to_cut = file->file_size - (int)new_size;
		while (need_to_cut > 0 && curr != NULL){
			if (need_to_cut >= curr->occupied){
				need_to_cut = need_to_cut - curr->occupied;
				curr->occupied = 0;
				curr = curr->prev;
				if (curr != NULL) {
					free(curr->next->memory);
					free(curr->next);
				}
				curr->next = NULL;
			}
			else{
				curr->occupied = curr->occupied - need_to_cut;
				need_to_cut = 0;
			}
		}
		file->file_size = (int)new_size;
		file->last_block = curr;
	}
	struct filedesc *curr_fd = file_descriptors;
	while (curr_fd != NULL){
		if (curr_fd->file == file){
			curr_fd->curr_block = file->last_block;
			curr_fd->shift = file->last_block->occupied;
		}
		curr_fd = curr_fd->next;
	}
	desc->curr_block = file->block_list;
	desc->shift = 0;
	return 0;
}

#endif
