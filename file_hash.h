#ifndef FILE_HASH_H
#define FILE_HASH_H
#include <fcntl.h>
#include <sys/mman.h>

#include <openssl/md5.h>

#include "file_entry.h"
#include "file_info.h"

enum hash_depth_t {
	NONE    = 0x0,
	SHALLOW = 0x1,
	FULL    = 0x2
};

char *
hash_entry(struct file_entry_t *file_entry, enum hash_depth_t depth)
{
	unsigned char hash_buffer[MD5_DIGEST_LENGTH], *file_buffer;
	int fd; off_t pivot; char *hash_storage = NULL;
	if (!file_entry || (fd = open(file_entry->path, O_RDONLY)) < 0) {
		#ifndef NDEBUG
		fprintf(stderr, "[ERROR] '%s' (open failed)\n", file_entry->path);
		#endif
		return NULL;
	}

	switch (depth) {
		/* A shallow hash reads only the first part of the file */
		case SHALLOW:
			/* Entries should not be hashed twice */
			if (file_entry->shash) { break; }
			/* Read a small part of the file and compute the hash of that */
			file_buffer = malloc(MD5_DIGEST_LENGTH);
			if (!file_buffer) {
				#ifndef NDEBUG
				fprintf(stderr, "[ERROR] '%s' (buffer failed)\n", file_entry->path);
				#endif
				break;
			}
			memset(file_buffer, 0, MD5_DIGEST_LENGTH);
			pivot = (file_entry->size < MD5_DIGEST_LENGTH) ?
				file_entry->size : MD5_DIGEST_LENGTH;
			if (read(fd, file_buffer, MD5_DIGEST_LENGTH) < pivot) {
				#ifndef NDEBUG
				fprintf(stderr, "[ERROR] '%s' (read failed)\n", file_entry->path);
				#endif
				break;
			}
			MD5(file_buffer, MD5_DIGEST_LENGTH, hash_buffer);
			free(file_buffer);
			hash_storage = file_entry->shash = malloc(2 * MD5_DIGEST_LENGTH + 1);
			break;

		/* A full hash is computed for the entire file */
		case FULL:
			/* Entries should not be hashed twice */
			if (file_entry->hash) { break; }
			/* FIXME make it only mmap files of appropriate size */
			file_buffer = mmap(NULL, file_entry->size, PROT_READ, MAP_SHARED, fd, 0);
			if (file_buffer == (MAP_FAILED)) {
				#ifndef NDEBUG
				fprintf(stderr, "[ERROR] '%s' (map failed)\n", file_entry->path);
				#endif
				break;
			}
			MD5(file_buffer, file_entry->size, hash_buffer);
			if (munmap(file_buffer, file_entry->size)) {
				#ifndef NDEBUG
				fprintf(stderr, "[WARNING] '%s' (unmap failed)\n", file_entry->path);
				#endif
			}
			hash_storage = file_entry->hash = malloc(2 * MD5_DIGEST_LENGTH + 1);
			break;

		default:
			#ifndef NDEBUG
			fprintf(stderr, "[WARNING] '%X' (unknown depth)\n", depth);
			#endif
			break;
	}

	/* Close the file */
	if (close(fd)) {
	#ifndef NDEBUG
		fprintf(stderr, "[WARNING] '%s' (close failed)\n", file_entry->path);
	#endif
	}

	/* Convert the hash to ASCII */
	if (hash_storage) {
		for (pivot = 0; pivot < MD5_DIGEST_LENGTH; ++pivot) {
			snprintf(hash_storage, 3, "%02x", hash_buffer[pivot]);
			++hash_storage; ++hash_storage;
		}
		return (depth == FULL) ? file_entry->hash : file_entry->shash;
	}
	return NULL;
}

DIR *
traverse(struct file_info_t *file_info, DIR *directory, char *buffer, size_t offset) 
{
	size_t d_name_len;
	struct dirent *dir_entry;
	/* Try to open up a directory handle */
	if (directory) {
		/* For each entry in the directory */
		while ((dir_entry = readdir(directory))) {
			/* Fill in the file name */
			memset(buffer + offset, '\0', PATH_MAX_LEN - offset);
			d_name_len = strnlen(dir_entry->d_name, PATH_MAX_LEN - offset);
			/* Avoid overflows, and ignore dotfiles */
			if (offset + d_name_len == PATH_MAX_LEN || dir_entry->d_name[0] == '.') {
				continue;
			}
			/* Construct the full path; it will be terminated */
			memcpy(buffer + offset, dir_entry->d_name, d_name_len);
			if (!record(buffer, file_info)) {
				return directory;
			}
		}
	}
	return NULL;
}

#endif /* FILE_HASH_H */
