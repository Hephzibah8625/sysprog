#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum
{
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 1024,
};

int MAX_BLOCK_NUMBER = MAX_FILE_SIZE / BLOCK_SIZE;

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
	struct 
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
			files->next = file_str;
			file_str->prev = files;
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
					file_descriptor_count++;
					ufs_error_code = UFS_ERR_NO_ERR;
					return i;
				}
			}
		} else {
			file_descriptor_capacity++;
			file_descriptors = (struct filedesc**)realloc(file_descriptors, file_descriptor_capacity * sizeof(struct filedesc*));
			file_descriptors[file_descriptor_count] = (struct filedesc*)malloc(sizeof(struct filedesc));
			file_descriptors[file_descriptor_count]->file = file_list;
			file_descriptors[file_descriptor_count]->file->refs++;
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

	for (int i = 0; i < size; i++) {
		if 
	}


	
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	if (fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	/* IMPLEMENT THIS FUNCTION */
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
	return -1;
}

int ufs_close(int fd)
{
	if (fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	// TODO: проверить есть ли файл в списке файлов
	file_descriptors[fd]->file->refs--;
	if (file_descriptors[fd]->file->deleted && file_descriptors[fd]->file->refs == 0) {
		free(file_descriptors[fd]->file);
	}
	file_descriptors[fd] = NULL;
	file_descriptor_count--;
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

	if (files->prev != NULL) {
		files->prev->next = files->next;
	}
	if (file_list == files) {
		file_list = NULL;
	}
	files->deleted = 1;

	ufs_error_code = UFS_ERR_NO_ERR;
	return 0;
}
