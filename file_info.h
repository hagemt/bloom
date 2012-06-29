#ifndef FILE_INFO_H
#define FILE_INFO_H
#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include <libcalg-1.0/libcalg/bloom-filter.h>
#include <libcalg-1.0/libcalg/hash-string.h>
#include <libcalg-1.0/libcalg/set.h>
#include <libcalg-1.0/libcalg/slist.h>
#include <libcalg-1.0/libcalg/trie.h>

#include "file_entry.h"

typedef unsigned int bloom_size_t;

struct file_info_t
{
	/* Store which files we will index */
	SListEntry *file_stack, *bad_files, *good_files, *duplicates;
	/* Store an index of the hashes */
	Trie *hash_trie, *shash_trie;
	bloom_size_t table_size;
	BloomFilter *shash_filter;
	/* Store statistical metadata */
	size_t total_files, invalid_files, protected_files, irregular_files;
};

/* Returns an entry if the path could be recorded */
struct file_entry_t *
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

inline void
clear_info(struct file_info_t *file_info)
{
	assert(file_info);
	file_info->file_stack =
	file_info->bad_files =
	file_info->good_files = 
	file_info->duplicates = NULL;
	file_info->hash_trie =
	file_info->shash_trie = NULL;
	file_info->shash_filter = NULL;
	file_info->total_files =
	file_info->invalid_files =
	file_info->protected_files =
	file_info->irregular_files = 0;
}

inline void
free_file_entry(void *file_entry)
{
	destroy_entry((struct file_entry_t *)(file_entry));
}

inline void
free_hash_set(void *set)
{
	set_free((Set *)(set));
}

inline void
destroy_list(SListEntry *list_entry, void (*destructor)(void *))
{
	/* Starting with this list entry */
	SListEntry *d, *e = list_entry;
	while (e) {
		d = slist_data(e);
		(*destructor)(d);
		e = slist_next(e);
	}
	/* Free the backing objects */
	slist_free(list_entry);
}

void
destroy_info(struct file_info_t *file_info)
{
	assert(file_info);
	/* Purge table data */
	if (file_info->hash_trie) {
		trie_free(file_info->hash_trie);
	}
	if (file_info->shash_trie) {
		trie_free(file_info->shash_trie);
	}
	if (file_info->shash_filter) {
		bloom_filter_free(file_info->shash_filter);
	}
	/* Purge duplicate lists */
	destroy_list(file_info->duplicates, &free_hash_set);
	/* Purge file data */
	destroy_list(file_info->file_stack, &free_file_entry);
	destroy_list(file_info->bad_files, &free_file_entry);
	destroy_list(file_info->good_files, &free_file_entry);
	/* Purge session data */
	clear_info(file_info);
}

inline size_t
num_errors(const struct file_info_t *file_info)
{
	assert(file_info);
	return file_info->invalid_files +
		file_info->protected_files +
		file_info->irregular_files;
}

inline size_t
num_files(const struct file_info_t *file_info)
{
	size_t errors = num_errors(file_info);
	assert(errors <= file_info->total_files);
	return file_info->total_files - errors;
}

/* Problem size, n, determines:
 * size of optimal filter, m
 * optimal number of hash functions, k
 * using: LN(2) ~ 0.7
 * p = Pr[false-positive] on (0,1)
 * note: p ~ 0 --> m, k >> n
 */
#define PR_FP 0.0001
#define LN2   0.7
/* Proof outline:
 * We are minimizing:
 *  p ~ (1 - exp(-kn/m))^k (see Wikipedia)
 * Let a = n/m and b = LN(2) / a
 *  p' = (1 - exp(-ka))^k * log(1 - exp(-ka)) * ...
 * The entire expression vanishes when k = b.
 * Thus, the optimal number of hash functions is:
 *  k = b = LN(2) / a = LN(2) / n/m = LN(2) * m / n
 * Substituting this in the first expression yields:
 *  p = 2^-k, which implies m = n / LN(2) * -log2(p)
 * Observe we have shown that k = -log2(p), so:
 *  m = n / LN(2) * k in simplest terms
 * One million elements would require < 2.3 MB.
 */

inline void
optimize_filter(struct file_info_t *file_info)
{
	double k = -log(PR_FP);
	/* Ensure preconditions */
	assert(file_info && k >= 1.0);
	if (file_info->shash_filter) {
		bloom_filter_free(file_info->shash_filter);
	}
	/* these filter parameters minimize false-positives */
	file_info->table_size = ceil(k / LN2) * num_files(file_info);
	/* ignore all case, since we are storing hashes */
	file_info->shash_filter = bloom_filter_new(
			file_info->table_size, string_nocase_hash, ceil(k));
	#ifndef NDEBUG
	printf("[DEBUG] '%d %0.1f' (bloom filter parameters)\n",
			file_info->table_size, k);
	#endif
}

#endif /* FILE_INFO_H */
