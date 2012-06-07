#ifndef FILE_HASH_H
#define FILE_HASH_H

#include "file_entry.h"
#include "file_info.h"

#define DIGEST_LENGTH  42

inline char *
shash(struct file_entry_t *file_entry) {
	char *begin, *end;
	if (!file_entry) {
		return NULL;
	}
	if (!file_entry->shash) {
		/* TODO do actual hashing */
		begin = file_entry->shash = malloc(DIGEST_LENGTH + 1);
		end = file_entry->shash + DIGEST_LENGTH;
		while (begin < end) {
			snprintf(begin, 3, "%02x", rand() % 0xFF);
			++begin; ++begin;
		}
	}
	return file_entry->shash;
}

inline DIR *
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
