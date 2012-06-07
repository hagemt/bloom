#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <libcalg-1.0/libcalg/slist.h>
#include <libcalg-1.0/libcalg/bloom-filter.h>
#include <libcalg-1.0/libcalg/trie.h>

#include "file_entry.h"
#include "file_info.h"
#include "file_hash.h"

int
main(int argc, char *argv[])
{
	/* For traversing lists and directories */
	struct file_entry_t *file_entry;
	SListIterator list_iterator;
	DIR *directory;
	size_t path_len;
	char buffer[PATH_MAX_LEN];
	/* Session data */
	struct file_info_t file_info;
	clear_info(&file_info);

	/* Step 1: Parse arguments */
	while (--argc) {
		/* Being unable to record implies insufficient resources */
		if (!record(argv[argc], &file_info)){
			fprintf(stderr, "[FATAL] out of memory\n");
			destroy_info(&file_info);
			return (EXIT_FAILURE);
		}
	}

	/* Step 2: Fully explore any directories specified */
	#ifndef NDEBUG
	fprintf(stderr, "[DEBUG] Creating file list... ");
	#endif
	while (slist_length(file_info.file_stack) > 0) {
		/* Pick off the top of the file stack */
		file_entry = (struct file_entry_t *)(slist_data(file_info.file_stack));
		slist_remove_entry(&file_info.file_stack, file_info.file_stack);
		assert(file_entry->type == DIRECTORY);
		/* Copy the basename to a buffer */
		memset(buffer, '\0', PATH_MAX_LEN);
		path_len = strnlen(file_entry->path, PATH_MAX_LEN);
		memcpy(buffer, file_entry->path, path_len);
		/* Ignore cases that would cause overflow */
		if (path_len < PATH_MAX_LEN) {
			/* Append a trailing slash */
			buffer[path_len] = '/';
			/* Record all contents (this may push onto file_stack or one of the lists) */
			directory = opendir(file_entry->path);
			if (traverse(&file_info, directory, buffer, ++path_len)) {
				fprintf(stderr, "[FATAL] out of memory\n");
				destroy_info(&file_info);
				return (EXIT_FAILURE);
			} else if (closedir(directory)) {
				fprintf(stderr, "[WARNING] '%s' (closedir)\n", file_entry->path);
			}
		}
		/* Discard this entry */
		destroy_entry(file_entry);
		free(file_entry);
	}
	#ifndef NDEBUG
	fprintf(stderr, "done\n");
	#endif

	/* Step 3: Warn about any ignored files */
	if (slist_length(file_info.bad_files) > 0) {
		slist_iterate(&file_info.bad_files, &list_iterator);
		while (slist_iter_has_more(&list_iterator)) {
			file_entry = (struct file_entry_t *)(slist_iter_next(&list_iterator));
			fprintf(stderr, "[WARNING] '%s' ", file_entry->path);
			switch (file_entry->type) {
			case INVALID:
				++file_info.invalid_files;
				fprintf(stderr, "(invalid file)\n");
				break;
			case INACCESSIBLE:
				++file_info.protected_files;
				fprintf(stderr, "(protected file)\n");
				break;
			default:
				++file_info.irregular_files;
				fprintf(stderr, "(irregular file)\n");
				break;
			}
		}
		fprintf(stderr, "[WARNING] files that could not be handled were ignored\n");
	}
	#ifndef NDEBUG
	/* Abort if necessary */
	if (num_errors(&file_info) > 0) {
		fprintf(stderr, "[FATAL] cannot parse entire file tree\n");
		destroy_info(&file_info);
		return (EXIT_FAILURE);
	}
	printf("[DEBUG] Found %lu / %lu valid files\n",
		(unsigned long)(num_files(&file_info)),
		(unsigned long)(file_info.total_files));
	#endif

	/* Step 4: Begin the filtering process */
	#ifndef NDEBUG
	fprintf(stderr, "[DEBUG] Creating file table... ");
	#endif
	if (slist_length(file_info.good_files) > 0) {
		file_info.hash_trie = trie_new();
		file_info.hash_filter = create_filter(slist_length(file_info.good_files));
		/* Extract each file from the list (they should all be regular) */
		slist_iterate(&file_info.good_files, &list_iterator);
		while (slist_iter_has_more(&list_iterator)) {
			file_entry = (struct file_entry_t *)(slist_iter_next(&list_iterator));
			assert(file_entry->type == NORMAL);
			/* Generate a hash and record it to the trie and filter */
			trie_insert(file_info.hash_trie, shash(file_entry), file_entry->path);
			bloom_filter_insert(file_info.hash_filter, shash(file_entry));
			#ifdef NDEBUG
			fprintf(stderr, "%s\t*%s\n", shash(file_entry), file_entry->path);
			#endif
		}
	}
	#ifndef NDEBUG
	fprintf(stderr, "done\n");
	#endif

	/* Step 5: Cleanup structures before exit */
	destroy_info(&file_info);
	return (EXIT_SUCCESS);
}
