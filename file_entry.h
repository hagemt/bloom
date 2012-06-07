#ifndef FILE_ENTRY_H
#define FILE_ENTRY_H
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PATH_MAX_LEN 0x7FF7

/* Indicate a file's status */
enum file_entry_type_t
{
	INVALID      = 0x0,
	INACCESSIBLE = 0x1,
	NORMAL       = 0x2,
	DIRECTORY    = 0x4,
	OTHER        = 0x8
};

inline enum file_entry_type_t
entry_type(const char * path)
{
	struct stat buffer;
	if (lstat(path, &buffer)) {
		return INVALID;
	}
	if (access(path, R_OK)) {
		return INACCESSIBLE;
	}
	if (S_ISREG(buffer.st_mode)) {
		return NORMAL;
	}
	if (S_ISDIR(buffer.st_mode)) {
		return DIRECTORY;	
	}
	return OTHER;
}

/* A file entry consists of a path, a hash of the file
 * (potentially a full or short hash) and its type */
struct file_entry_t
{
	char *path, *hash, *shash;
	enum file_entry_type_t type;
};

inline enum file_entry_type_t
entry(const char *path, struct file_entry_t *file_entry)
{
	size_t path_len;
	/* stat the file at this path */
	enum file_entry_type_t type = entry_type(path);
	if (path && file_entry) {
		file_entry->type = type;
		file_entry->hash = file_entry->shash = NULL;
		/* Assure ourselves that path is terminal */
		path_len = strnlen(path, PATH_MAX_LEN) + 1;
		file_entry->path = malloc(path_len * sizeof(char));
		memset(file_entry->path, '\0', path_len);
		memcpy(file_entry->path, path, path_len);
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
	}
}

#endif /* FILE_ENTRY_H */
