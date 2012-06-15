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
	SListIterator list_iterator;
	DIR *directory;
	/* For dealing with entries */
	size_t path_len;
	char path_buffer[PATH_MAX_LEN], *hash_value;
	struct file_entry_t *file_entry, *trie_entry;
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
	printf("[DEBUG] Creating file list...\n");
	#endif
	while (slist_length(file_info.file_stack) > 0) {
		/* Pick off the top of the file stack */
		file_entry = (struct file_entry_t *)(slist_data(file_info.file_stack));
		slist_remove_entry(&file_info.file_stack, file_info.file_stack);
		assert(file_entry->type == DIRECTORY);
		/* Copy the basename to a buffer */
		memset(path_buffer, '\0', PATH_MAX_LEN);
		path_len = strnlen(file_entry->path, PATH_MAX_LEN);
		memcpy(path_buffer, file_entry->path, path_len);
		/* Ignore cases that would cause overflow */
		if (path_len < PATH_MAX_LEN) {
			/* Append a trailing slash */
			path_buffer[path_len] = '/';
			/* Record all contents (may push onto file stack or one of the lists) */
			directory = opendir(file_entry->path);
			if (traverse(&file_info, directory, path_buffer, ++path_len)) {
				fprintf(stderr, "[FATAL] out of memory\n");
				destroy_info(&file_info);
				return (EXIT_FAILURE);
			} else if (closedir(directory)) {
				fprintf(stderr, "[WARNING] '%s' (close failed)\n", file_entry->path);
			}
		}
		/* Discard this entry */
		destroy_entry(file_entry);
		free(file_entry);
	}

	/* Step 3: Warn about any ignored files */
	if (slist_length(file_info.bad_files) > 0) {
		slist_iterate(&file_info.bad_files, &list_iterator);
		while (slist_iter_has_more(&list_iterator)) {
			file_entry = slist_iter_next(&list_iterator);
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
		fprintf(stderr, "[WARNING] %lu file(s) ignored\n",
			(long unsigned)(num_errors(&file_info)));
	}
	#ifndef NDEBUG
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
	printf("[DEBUG] Creating file table...\n");
	#endif
	if (slist_length(file_info.good_files) > 0) {
		file_info.hash_trie = trie_new();
		file_info.shash_trie = trie_new();
		/* FIXME is this ^ really necessary, two swipes? */
		file_info.shash_filter = create_filter(slist_length(file_info.good_files));
		/* Extract each file from the list (they should all be regular) */
		slist_iterate(&file_info.good_files, &list_iterator);
		while (slist_iter_has_more(&list_iterator)) {
			file_entry = slist_iter_next(&list_iterator);
			assert(file_entry->type == REGULAR);
			/* Perform a "shallow" hash of the file */
			hash_value = hash_entry(file_entry, SHALLOW);
			#ifndef NDEBUG
			printf("[-HASH] %s\t*%s\n", file_entry->path, hash_value);
			#endif
			/* Check to see if we might have seen this file before */
			if (bloom_filter_query(file_info.shash_filter, hash_value)) {
				hash_value = hash_entry(file_entry, FULL);
				#ifndef NDEBUG
				printf("[+HASH] %s\t*%s\n", file_entry->path, hash_value);
				#endif
				trie_entry = trie_lookup(file_info.hash_trie, hash_value);
				if (trie_entry != TRIE_NULL) {
					/* TODO handle collision with a list */
					printf("%s == %s\n", trie_entry->path, file_entry->path);
				} else {
					trie_insert(file_info.hash_trie, hash_value, file_entry);
				}
			} else {
				/* Add a record of this shash to the filter */
				bloom_filter_insert(file_info.shash_filter, hash_value);
			}
		}
	}

	/* Step 5: Cleanup structures before exit */
	destroy_info(&file_info);
	return (EXIT_SUCCESS);
}
