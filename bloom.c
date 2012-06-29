#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <libcalg-1.0/libcalg/bloom-filter.h>
#include <libcalg-1.0/libcalg/hash-pointer.h>
#include <libcalg-1.0/libcalg/compare-pointer.h>
#include <libcalg-1.0/libcalg/set.h>
#include <libcalg-1.0/libcalg/slist.h>
#include <libcalg-1.0/libcalg/trie.h>

#include "file_entry.h"
#include "file_info.h"
#include "file_hash.h"

#include "persist.h"

void
archive(struct file_info_t *info, struct file_entry_t *entry)
{
	Set *hash_set = trie_lookup(info->hash_trie, entry->hash);
	if (hash_set == TRIE_NULL) {
		/* Otherwise, the value needs a new list */
		hash_set = set_new(&pointer_hash, &pointer_equal);
		slist_prepend(&info->duplicates, hash_set);
		trie_insert(info->hash_trie, entry->hash, hash_set);
	}
	if (!set_insert(hash_set, entry)) {
		#ifndef NDEBUG
		fprintf(stderr, "[DEBUG] '%s' (extra file)\n", entry->path);
		#endif
	}
}

int
main(int argc, char *argv[])
{
	size_t path_len, total_files;
	off_t bytes_wasted, total_wasted;
	char path_buffer[PATH_MAX_LEN], *hash_value;
	struct file_entry_t *file_entry, *trie_entry;

	SListIterator slist_iterator;
	SetIterator set_iterator;

	/* Step 0: Session data */
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
			DIR *directory = opendir(file_entry->path);
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
	}

	/* Step 3: Warn about any ignored files */
	if (slist_length(file_info.bad_files) > 0) {
		slist_iterate(&file_info.bad_files, &slist_iterator);
		while (slist_iter_has_more(&slist_iterator)) {
			file_entry = slist_iter_next(&slist_iterator);
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
		optimize_filter(&file_info);
		/* Extract each file from the list (they should all be regular) */
		slist_iterate(&file_info.good_files, &slist_iterator);
		while (slist_iter_has_more(&slist_iterator)) {
			file_entry = slist_iter_next(&slist_iterator);
			assert(file_entry->type == REGULAR);
			/* Perform a "shallow" hash of the file */
			hash_value = hash_entry(file_entry, SHALLOW);
			#ifndef NDEBUG
			printf("[SHASH] %s\t*%s\n", file_entry->path, hash_value);
			#endif
			/* Check to see if we might have seen this file before */
			if (bloom_filter_query(file_info.shash_filter, hash_value)) {
				/* Get the full hash of the new file */
				hash_value = hash_entry(file_entry, FULL);
				#ifndef NDEBUG
				printf("[+HASH] %s\t*%s\n", file_entry->path, hash_value);
				#endif
				archive(&file_info, file_entry);
				/* Check to see if bloom failed us */
				trie_entry = trie_lookup(file_info.shash_trie, file_entry->shash);
				if (trie_entry == TRIE_NULL) {
					#ifndef NDEBUG
					printf("[DEBUG] '%s' (false positive)\n", file_entry->path);
					#endif
					trie_insert(file_info.shash_trie, file_entry->shash, file_entry);
				} else {
					/* Get the full hash of the old file */
					hash_value = hash_entry(trie_entry, FULL);
					#ifndef NDEBUG
					if (hash_value) {
						printf("[-HASH] %s\t*%s\n", trie_entry->path, hash_value);
					}
					#endif
					archive(&file_info, trie_entry);
				}
			} else {
				/* Add a record of this shash to the filter */
				bloom_filter_insert(file_info.shash_filter, hash_value);
				trie_insert(file_info.shash_trie, hash_value, file_entry);
			}
		}
		persist("filter.bloom", &file_info);
	}

	/* Step 5: Output results and cleanup before exit */
	printf("[EXTRA] Found %lu sets of duplicates...\n",
		(unsigned long)(slist_length(file_info.duplicates)));
	slist_iterate(&file_info.duplicates, &slist_iterator);
	for (total_files = total_wasted = bytes_wasted = 0;
		slist_iter_has_more(&slist_iterator);
		total_wasted += bytes_wasted)
	{
		Set *set = slist_iter_next(&slist_iterator);
		int size = set_num_entries(set);
		if (size < 2) { continue; }
		printf("[EXTRA] %lu files (w/ same hash):\n", (unsigned long)(size));
		set_iterate(set, &set_iterator);
		for (bytes_wasted = 0;
			set_iter_has_more(&set_iterator);
			bytes_wasted += file_entry->size,
			++total_files)
		{
			file_entry = set_iter_next(&set_iterator);
			printf("\t%s (%lu bytes)\n",
				file_entry->path,
				(unsigned long)(file_entry->size));
		}
	}
	printf("[EXTRA] %lu bytes in %lu files (wasted)\n",
		(unsigned long)(total_wasted),
		(unsigned long)(total_files));
	destroy_info(&file_info);
	return (EXIT_SUCCESS);
}
