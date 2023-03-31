#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum
{
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block
{
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

struct file
{
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
	const char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
	int deleted;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc
{
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
	int offset;
	int flag;
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

int ufs_open(const char *filename, int flags)
{
	/* checks */
	struct file* files = file_list;
	int found_file = 0;
	while (files != NULL) {
		if (strcmp(files->name, filename) == 0) {
			found_file = 1;
			break;
		}
		files = files->next;
	}
	if (!found_file && flags == UFS_CREATE) {
		/* file creation */
		struct file *file_str = (struct file*)malloc(sizeof(struct file));
		file_str->block_list = NULL;
		file_str->last_block = NULL;
		file_str->refs = 0;
		file_str->name = strdup(filename);
		file_str->next = NULL;
		file_str->prev = NULL;
		file_str->deleted = 0;
		if (file_list == NULL) {
			file_list = file_str;
		} else {
			struct file* f_file = file_list;
			while (f_file->next != NULL) {
				f_file = f_file->next;
			}
			f_file->next = file_str;
			file_str->prev = f_file;
		}


		if (file_descriptor_capacity == 0) {
			file_descriptors = (struct filedesc**)malloc(sizeof(struct filedesc));
			file_descriptors[0] = NULL;
			file_descriptor_capacity++;
		}
		if (file_descriptor_count < file_descriptor_capacity) {
			for (int i = 0; i < file_descriptor_capacity; i++) {
				if (file_descriptors[i] == NULL) {
					file_descriptors[i] = (struct filedesc*)malloc(sizeof(struct filedesc));
					file_descriptors[i]->file = file_str;
					file_descriptors[i]->file->refs++;
					file_descriptors[i]->offset = 0;
					file_descriptors[i]->flag = flags;
					file_descriptor_count++;
					ufs_error_code = UFS_ERR_NO_ERR;
					return i;
				}
			}
		} else {
			file_descriptor_capacity++;
			file_descriptors = (struct filedesc**)realloc(file_descriptors, file_descriptor_capacity * sizeof(struct filedesc*));
			file_descriptors[file_descriptor_count] = (struct filedesc*)malloc(sizeof(struct filedesc));
			file_descriptors[file_descriptor_count]->file = file_str;
			file_descriptors[file_descriptor_count]->file->refs++;
			file_descriptors[file_descriptor_count]->offset = 0;
			file_descriptors[file_descriptor_count]->flag = flags;
			file_descriptor_count++;
			ufs_error_code = UFS_ERR_NO_ERR;
			return file_descriptor_count - 1;
		}
	}
	if (found_file) {
		if (file_descriptor_capacity == 0) {
			file_descriptors = (struct filedesc**)malloc(sizeof(struct filedesc));
			file_descriptors[0] = NULL;
			file_descriptor_capacity++;
		}
		if (file_descriptor_count < file_descriptor_capacity) {
			for (int i = 0; i < file_descriptor_capacity; i++) {
				if (file_descriptors[i] == NULL) {
					file_descriptors[i] = (struct filedesc*)malloc(sizeof(struct filedesc));
					file_descriptors[i]->file = files;
					file_descriptors[i]->file->refs++;
					file_descriptors[i]->offset = 0;
					file_descriptors[i]->flag = flags;
					file_descriptor_count++;
					ufs_error_code = UFS_ERR_NO_ERR;
					return i;
				}
			}
		} else {
			file_descriptor_capacity++;
			file_descriptors = (struct filedesc**)realloc(file_descriptors, file_descriptor_capacity * sizeof(struct filedesc*));
			file_descriptors[file_descriptor_count] = (struct filedesc*)malloc(sizeof(struct filedesc));
			file_descriptors[file_descriptor_count]->file = files;
			file_descriptors[file_descriptor_count]->file->refs++;
			file_descriptors[file_descriptor_count]->offset = 0;
			file_descriptors[file_descriptor_count]->flag = flags;
			file_descriptor_count++;
			ufs_error_code = UFS_ERR_NO_ERR;
			return file_descriptor_count - 1;
		}
	}

	ufs_error_code = UFS_ERR_NO_FILE;
	return -1;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	if (fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (file_descriptors[fd]->flag == UFS_READ_ONLY) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}

	struct file* file_ptr = file_descriptors[fd]->file;
	if (file_ptr->block_list == NULL) {
		struct block* fir_bl = (struct block*)malloc(sizeof(struct block));
		fir_bl->next = NULL;
		fir_bl->prev = NULL;
		fir_bl->occupied = 0;
		fir_bl->memory = (char*)malloc(BLOCK_SIZE * sizeof(char));
		file_ptr->block_list = fir_bl;
		file_ptr->last_block = fir_bl;
	}

	struct block* curr_block = file_ptr->block_list;
	int ptr = 0;
	while (ptr + BLOCK_SIZE <= file_descriptors[fd]->offset && file_descriptors[fd]->offset > 0) {
		if (curr_block->next == NULL) {
			struct block* bl = (struct block*)malloc(sizeof(struct block));
			bl->next = NULL;
			bl->prev = curr_block;
			curr_block->next = bl;
			bl->occupied = 0;
			bl->memory = (char*)malloc(BLOCK_SIZE * sizeof(char));
			file_ptr->last_block = bl;
		}
		curr_block = curr_block->next;
		ptr = ptr + BLOCK_SIZE;
	}

	ptr = file_descriptors[fd]->offset % BLOCK_SIZE;

	for (int i = 0; i < size; i++) {
		if (file_descriptors[fd]->offset == MAX_FILE_SIZE) {
			ufs_error_code = UFS_ERR_NO_MEM;
			return -1;
		}
		if (ptr == BLOCK_SIZE) {
			if (curr_block->next == NULL) {
				struct block* bl = (struct block*)malloc(sizeof(struct block));
				bl->next = NULL;
				bl->prev = curr_block;
				curr_block->next = bl;
				bl->occupied = 0;
				bl->memory = (char*)malloc(BLOCK_SIZE * sizeof(char));
				file_ptr->last_block = bl;
			}
			curr_block = curr_block->next;
			ptr = 0;
		}
		curr_block->memory[ptr] = buf[i];
		ptr++;
		if (ptr > curr_block->occupied) {
			curr_block->occupied++;
		}
		file_descriptors[fd]->offset++;
	}
	
	ufs_error_code = UFS_ERR_NO_ERR;
	return size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	if (fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (file_descriptors[fd]->flag == UFS_WRITE_ONLY) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	
	struct file* file_ptr = file_descriptors[fd]->file;
	struct block* curr_block = file_ptr->block_list;
	int count_iter = 0;

	int ptr = 0;
	while (ptr + BLOCK_SIZE <= file_descriptors[fd]->offset && file_descriptors[fd]->offset > 0) {
		curr_block = curr_block->next;
		ptr = ptr + BLOCK_SIZE;
	}

	ptr = file_descriptors[fd]->offset % BLOCK_SIZE;

	if (curr_block == NULL) {
		ufs_error_code = UFS_ERR_NO_ERR;
		return count_iter;
	}
	for (int i = 0; i < size; i++) {
		if (file_descriptors[fd]->offset == MAX_FILE_SIZE) {
			break;
		}
		if (ptr == curr_block->occupied && ptr != BLOCK_SIZE) {
			break;
		}
		if (ptr == BLOCK_SIZE) {
			if (curr_block->next == NULL) {
				break;
			}
			curr_block = curr_block->next;
			ptr = 0;
		}
		buf[i] = curr_block->memory[ptr];
		ptr++;
		count_iter++;
		file_descriptors[fd]->offset++;
	}

	ufs_error_code = UFS_ERR_NO_ERR;
	return count_iter;
}

int ufs_close(int fd)
{
	if (fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	file_descriptors[fd]->file->refs--;
	if (file_descriptors[fd]->file->deleted == 1 &&  file_descriptors[fd]->file->refs == 0) {
		struct block* bl = file_descriptors[fd]->file->last_block;
		while (1) {
			if (bl == NULL) {
				break;
			}
			if (bl->prev == NULL) {
				free(bl->memory);
				free(bl);
				break;
			}
			bl = bl->prev;
			free(bl->next->memory);
			free(bl->next); 
		}
		free((char*)file_descriptors[fd]->file->name);
		free(file_descriptors[fd]->file);
	}
	free(file_descriptors[fd]);
	file_descriptors[fd] = NULL;
	file_descriptor_count--;
	if (fd == file_descriptor_capacity - 1) {
		while(file_descriptor_capacity > 0 &&  file_descriptors[file_descriptor_capacity - 1] == NULL) {
			file_descriptor_capacity--;
			file_descriptors = (struct filedesc**)realloc(file_descriptors, file_descriptor_capacity * sizeof(struct filedesc*));
		}
	}
	if (file_descriptor_capacity == 0) {
		free(file_descriptors);
		file_descriptors = NULL;
	}
	return 0;
}

int ufs_delete(const char *filename)
{
	/* checks */
	struct file* files = file_list;
	int found_file = 0;
	while (files != NULL) {
		if (strcmp(files->name, filename) == 0) {
			found_file = 1;
			break;
		}
		files = files->next;
	}

	if (!found_file) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (files->prev != NULL && files->next != NULL) {
		files->prev->next = files->next;
	} else if (files->prev != NULL && files->next == NULL) {
		files->prev->next = NULL;
	}
	if (file_list == files && files->next == NULL) {
		file_list = NULL;
	} else if (file_list == files && files->next != NULL) {
		file_list = files->next;
	}
	files->deleted = 1;
	if (files->refs == 0) {
		struct block* bl = files->last_block;
		while (1) {
			if (bl == NULL) {
				break;
			}
			if (bl->prev == NULL) {
				free(bl->memory);
				free(bl);
				break;
			}
			bl = bl->prev;
			free(bl->next->memory);
			free(bl->next); 
		}
		free((char*)files->name);
		free(files);
	}

	ufs_error_code = UFS_ERR_NO_ERR;
	return 0;
}

int ufs_resize(int fd, size_t new_size)
{
	if (fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	if (new_size > MAX_FILE_SIZE) {
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}	

	struct file* f_file = file_descriptors[fd]->file;
	struct block* f_bl = f_file->block_list;

	int size = 0;

	while (f_bl != NULL) {
		size = size + BLOCK_SIZE;
		f_bl = f_bl->next;
	}

	if (size > new_size) {
		while (size - BLOCK_SIZE > new_size && size >= 0) {
			struct block* last = f_file->last_block;
			if (last == NULL) break;
			free(last->memory);
			if (last->prev == NULL) {
				free(last);
				f_file->block_list = NULL;
				f_file->last_block = NULL;
			} else {
				last->prev->next = NULL;
				f_file->last_block = last->prev;
				free(last);
			}
			size = size - BLOCK_SIZE;
		}
		while (f_file->last_block->occupied > new_size % BLOCK_SIZE) {
			f_file->last_block->memory[f_file->last_block->occupied - 1] = 0;
			f_file->last_block->occupied--;
		}

		for (int i = 0; i < file_descriptor_capacity; i++) {
			if (file_descriptors[i] == NULL) continue;
			if (strcmp(file_descriptors[i]->file->name, f_file->name) == 0) {
				if (file_descriptors[i]->offset > new_size) file_descriptors[i]->offset = new_size;
			}
		}
	} else if (size < new_size) {
		while (size < new_size) {
			struct block* bl = (struct block*)malloc(sizeof(struct block));
			bl->next = NULL;
			bl->prev = f_file->last_block;
			f_file->last_block = bl;
			bl->occupied = 0;
			bl->memory = (char*)malloc(BLOCK_SIZE * sizeof(char));
			size = size + BLOCK_SIZE;
		}
	}


	ufs_error_code = UFS_ERR_NO_ERR;
	return 0;
}
