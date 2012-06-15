#ifndef FILE_INFO_H
#define FILE_INFO_H
#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include <libcalg-1.0/libcalg/bloom-filter.h>
#include <libcalg-1.0/libcalg/hash-string.h>
#include <libcalg-1.0/libcalg/slist.h>
#include <libcalg-1.0/libcalg/trie.h>

#include "file_entry.h"

struct file_info_t
{
	/* Store which files we will index */
	SListEntry *file_stack, *bad_files, *good_files;
	/* Store an index of the hashes */
	Trie *hash_trie; BloomFilter *hash_filter;
	/* Store statistical metadata */
	size_t total_files, invalid_files, protected_files, irregular_files;
};

/* Returns an entry if the path could be recorded */
inline struct file_entry_t *
record(const char *file_path, struct file_info_t *file_info)
{
	struct file_entry_t *file_entry = NULL;
	/* Require both parts to be valid */
	if (file_path && file_info) {
		/* This will be freed when the lists are destroyed */
		file_entry = malloc(sizeof(struct file_entry_t));
		/* Catch any storage issues at this stage */
		if (file_entry) {
			/* Check that this path is valid (fill the entry) */
			stat_entry(file_path, file_entry);
			switch (file_entry->type) {
			case DIRECTORY:
				/* Directories go on the stack instead */
				slist_prepend(&file_info->file_stack, file_entry);
				break;
			case REGULAR:
				/* Count these files as valid */
				++file_info->total_files;
				slist_prepend(&file_info->good_files, file_entry);
				break;
			default:
				/* Count these files as invalid */
				++file_info->total_files;
				slist_prepend(&file_info->bad_files, file_entry);
			}
		}
	}
	return file_entry;
}

/* Utilities */

#define FP 0.01
#define LN2 0.7

/* Create optimal bloom-filter for n elements */
inline BloomFilter *
create_filter(size_t n)
{
	/* size of the filter depends on n and Pr[false-positive] */
	size_t m = ceil(n / LN2 * -log2(FP));
	/* number of hash functions to minimize false-positives */
	size_t k = ceil(LN2 * m / n);
	/* ignore all case, since we are storing hashes */
	return bloom_filter_new(m, string_nocase_hash, k);
}

inline void
clear_info(struct file_info_t *file_info)
{
	if (file_info) {
		file_info->file_stack =
		file_info->bad_files =
		file_info->good_files = NULL;
		file_info->hash_trie = NULL;
		file_info->hash_filter = NULL;
		file_info->total_files =
		file_info->invalid_files =
		file_info->protected_files =
		file_info->irregular_files = 0;
	}
}


inline void
clear_list(SListEntry *list_entry)
{
	/* Starting with this list entry */
	SListEntry *e = list_entry;
	while (e) {
		destroy_entry(slist_data(e));
		free(slist_data(e));
		e = slist_next(e);
	}
	/* Free the backing objects */
	slist_free(list_entry);
}

inline void
destroy_info(struct file_info_t *file_info)
{
	if (file_info) {
		/* Purge table data */
		if (file_info->hash_trie) {
			trie_free(file_info->hash_trie);
		}
		if (file_info->hash_filter) {
			bloom_filter_free(file_info->hash_filter);
		}
		/* Purge file data */
		clear_list(file_info->file_stack);
		clear_list(file_info->bad_files);
		clear_list(file_info->good_files);
		/* Purge session data */
		clear_info(file_info);
	}
}

inline size_t
num_errors(const struct file_info_t *file_info)
{
	if (file_info) {
		return file_info->invalid_files +
			file_info->protected_files +
			file_info->irregular_files;
	}
	return 0;
}

inline size_t
num_files(const struct file_info_t *file_info)
{
	if (file_info) {
		size_t errors = num_errors(file_info);
		assert(errors <= file_info->total_files);
		return file_info->total_files - errors;
	}
	return 0;
}

#endif /* FILE_INFO_H */
