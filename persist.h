#ifndef PERSIST_H
#define PERSIST_H
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <libcalg-1.0/libcalg/bloom-filter.h>
#include <libcalg-1.0/libcalg/hash-string.h>
#include <libcalg-1.0/libcalg/slist.h>

#include <gdbm.h>

#include <zlib.h>

#include "file_info.h"
#include "file_log.h"

#define BLOOM_PERSISTENCE_ERROR (-1)
#define BUFFER_SIZE 4096

#define BLOOM_EXT_BACKUP   "bak"
#define BLOOM_EXT_FILTER   "bbf"
#define BLOOM_EXT_CONFIG   "bcf"
#define BLOOM_EXT_DATABASE "bdb"

#define BLOOM_CHAR_HASH_INDICATOR  '+'
#define BLOOM_CHAR_SHASH_INDICATOR '-'

int
persist(char *backup_file, struct file_info_t *file_info)
{
	/* For file manipulation */
	time_t time_stamp;
	char buffer[BUFFER_SIZE];
	/* For table */
	int fd, status;
	bloom_size_t data_size;
	unsigned char *data_buffer;
	/* For entries */
	GDBM_FILE gdbmf;
	datum key, value;
	SListIterator slist_iterator;
	/* Check for valid arguments */
	if (!backup_file || !file_info
			|| *backup_file == '\0'
			|| !file_info->shash_filter) {
		return BLOOM_PERSISTENCE_ERROR;
	}
	time(&time_stamp);
	status = (int)(sizeof(bloom_size_t));
	/* If the file exists, move it to backup */
	if (!access(backup_file, F_OK)) {
		snprintf(buffer, BUFFER_SIZE, "%s.%lu.%s",
				backup_file, (long unsigned)(time_stamp), BLOOM_EXT_BACKUP);
		/* Inability to do this triggers a failure */
		if (rename(backup_file, buffer)) {
			#ifndef NDEBUG
			fprintf(stderr, "[ERROR] %s > %s (rename failed)\n",
					backup_file, buffer);
			#endif
			return BLOOM_PERSISTENCE_ERROR;
		}
	}
	/* Try to open the file */
	snprintf(buffer, BUFFER_SIZE, "%s.%lu.%s",
			backup_file, (long unsigned)(time_stamp), BLOOM_EXT_FILTER);
	if ((fd = creat(buffer, S_IRUSR | S_IWUSR)) < 0) {
		#ifndef NDEBUG
		fprintf(stderr, "[ERROR] %s (unable to persist)\n", backup_file);
		#endif
		return BLOOM_PERSISTENCE_ERROR;
	}
	/* Persist the filter data */
	data_size = (file_info->table_size + 7) / 8;
	if (write(fd, &data_size, status) < status) {
		#ifndef NDEBUG
		fprintf(stderr, "[WARNING] bytes lost (%s)\n", "bloom header persist");
		#endif
	}
	data_buffer = malloc(data_size * sizeof(unsigned char));
	bloom_filter_read(file_info->shash_filter, data_buffer);
	if (write(fd, data_buffer, data_size) < data_size) {
		#ifndef NDEBUG
		fprintf(stderr, "[WARNING] bytes lost (%s)\n", "bloom table persist");
		#endif
	}
	/* Cleanup */
	free(data_buffer);
	if (close(fd)) {
		#ifndef NDEBUG
		fprintf(stderr, "[WARNING] %s (close failed)\n", backup_file);
		#endif
	}
	/* Persist the data entries */
	snprintf(buffer, BUFFER_SIZE, "%s%c%lu.%s",
			backup_file, BLOOM_CHAR_HASH_INDICATOR,
			(long unsigned)(time_stamp), BLOOM_EXT_DATABASE);
	gdbmf = gdbm_open(buffer, BUFFER_SIZE,
			GDBM_NEWDB, S_IRUSR | S_IWUSR, log_message);
	if (!gdbmf) {
		#ifndef NDEBUG
		fprintf(stderr, "[ERROR] %s (open database failed)\n", buffer);
		#endif
		return BLOOM_PERSISTENCE_ERROR;
	}
	slist_iterate(&file_info->good_files, &slist_iterator);
	while (slist_iter_has_more(&slist_iterator)) {
		struct file_entry_t *file_entry = slist_iter_next(&slist_iterator);
		key.dptr = hash_entry(file_entry, FULL);
		key.dsize = sizeof(file_entry->hash);
		value.dptr = (char *)(file_entry);
		value.dsize = sizeof(struct file_entry_t);
		status = gdbm_store(gdbmf, key, value, GDBM_INSERT);
		assert(status == 0);
	}
	gdbm_close(gdbmf);
	/* TODO gzip everything into backup_file.BLOOM_EXT_CONFIG */
	return 0;
}

int
recover(char *backup_file, struct file_info_t *file_info)
{
	int fd, status;
	bloom_size_t data_size;
	unsigned char *data_buffer;
	if (!backup_file || !file_info) {
		return BLOOM_PERSISTENCE_ERROR;
	}
	status = (int)(sizeof(bloom_size_t));
	/* Make sure we can read from the file */
	if ((fd = open(backup_file, O_RDONLY)) < 0) {
		#ifndef NDEBUG
		fprintf(stderr, "[ERROR] %s (unable to recover)\n", backup_file);
		#endif
		return BLOOM_PERSISTENCE_ERROR;
	}
	/* Recover the filter data */
	if (read(fd, &data_size, status) < status) {
		#ifndef NDEBUG
		fprintf(stderr, "[WARNING] bytes lost (%s)\n", "bloom header recover");
		#endif
	}
	data_buffer = malloc(data_size * sizeof(unsigned char));
	if (read(fd, data_buffer, data_size) < data_size) {
		#ifndef NDEBUG
		fprintf(stderr, "[WARNING] bytes lost (%s)\n", "bloom table recover");
		#endif
	}
	/* Load it into the file information */
	optimize_filter(file_info);
	bloom_filter_load(file_info->shash_filter, data_buffer);
	/* Cleanup */
	free(data_buffer);
	if (close(fd)) {
		#ifndef NDEBUG
		fprintf(stderr, "[WARNING] %s (close failed)\n", backup_file);
		#endif
	}
	return 0;
}

#endif /* PERSIST_H */
