#ifndef FILE_ENTRY_H
#define FILE_ENTRY_H
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PATH_MAX_LEN 0x7FF7
#define DEFAULT_SIZE (off_t)(-1)

/* Indicate a file's status */
enum file_entry_type_t
{
	INVALID      = 0x0,
	INACCESSIBLE = 0x1,
	REGULAR      = 0x2,
	DIRECTORY    = 0x4,
	OTHER        = 0x8
};

/* A file entry consists of a path, a hash of the file
 * (potentially a full or short hash) and its type */
struct file_entry_t
{
	char *path, *hash, *shash;
	enum file_entry_type_t type;
	off_t size;
};

inline enum file_entry_type_t
stat_entry(const char *path, struct file_entry_t *file_entry)
{
	size_t path_len;
	struct stat status;
	enum file_entry_type_t type;
	/* stat the file at this path */
	if (!path || lstat(path, &status)) {
		type = INVALID;
	} else if (access(path, R_OK)) {
		type = INACCESSIBLE;
	} else if (S_ISREG(status.st_mode)) {
		type = REGULAR;
	} else if (S_ISDIR(status.st_mode)) {
		type = DIRECTORY;
	} else {
		type = OTHER;
	}
	/* If appropriate, fill in the entry */
	if (file_entry) {
		memset(file_entry, 0, sizeof(struct file_entry_t));
		file_entry->hash = file_entry->shash = NULL;
		file_entry->size = (type == REGULAR) ? status.st_size : (DEFAULT_SIZE);
		file_entry->type = type;
		if (path) {
			/* Assure ourselves that the path is terminal */
			path_len = strnlen(path, PATH_MAX_LEN) + 1;
			file_entry->path = malloc(path_len * sizeof(char));
			memset(file_entry->path, '\0', path_len * sizeof(char));
			memcpy(file_entry->path, path, path_len * sizeof(char));
		}
	}
	return type;
}

inline void
destroy_entry(struct file_entry_t *file_entry)
{
	/* Free all dynamically-allocated memory in this entry */
	if (file_entry) {
		if (file_entry->hash) {
			free(file_entry->hash);
		}
		if (file_entry->shash) {
			free(file_entry->shash);
		}
		free(file_entry->path);
		free(file_entry);
	}
}

#endif /* FILE_ENTRY_H */
