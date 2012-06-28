#ifndef MONITOR_H
#define MONITOR_H
#include <assert.h>
#include <stdlib.h>
#ifndef NDEBUG
#include <stdio.h>
#endif

#include <gio/gio.h>

#include <libnotify/notify.h>

#define NOTIFY_APP_NAME "bloomd"
#define NOTIFY_TIMEOUT 1000
#define NOTIFY_TYPE "dialog-information"

void
display(char *title, char *message)
{
	NotifyNotification *n;
	assert(notify_is_initted());
	/* Use a notification to alert the user */
	n = notify_notification_new(title, message, NOTIFY_TYPE, NULL);
	notify_notification_set_timeout(n, NOTIFY_TIMEOUT);
	notify_notification_set_category(n, NOTIFY_APP_NAME);
	notify_notification_set_urgency(n, NOTIFY_URGENCY_CRITICAL);
	notify_notification_show(n, NULL);
	g_object_unref(n);
}

/* A simple list entry */
struct slent_t
{
	GFile *file;
	GFileMonitor *file_monitor;
	gulong connection_id;
	/* TODO needs mutex? */
};

/* Simple list entry manipulations */
inline void monitor(struct slent_t *);
inline void unmonitor(struct slent_t *);

/* Construct entries */
gboolean
create(char *arg, struct slent_t *entry)
{
	if (entry) {
		entry->file = g_file_new_for_commandline_arg(arg);
		entry->file_monitor = NULL;
		entry->connection_id = 0;
		/* Try to monitor the file */
		if (entry->file) {
			/* TODO only monitor directories? */
			monitor(entry);
			return TRUE;
		}
		g_object_unref(entry->file);
		entry->file = NULL;
	}
	return FALSE;
}

/* Cleanup entries */
void
destroy(struct slent_t *entry)
{
	if (entry) {
		if (entry->file_monitor) {
			unmonitor(entry);
		}
		if (entry->file) {
			g_object_unref(entry->file);
		}
		free(entry);
	}
}

/* A simple entry list */
struct slist_t
{
	struct slent_t *head;
	struct slist_t *tail;
	/* TODO needs mutex? */
} list;

/* Callback and helper functions */

#define EVENT_FORMAT "[EVENT %p] %s %c %s\n"

void
changed(GFileMonitor *file_monitor,
		GFile *file1, GFile *file2,
		GFileMonitorEvent type,
		gpointer data)
{
	char *filepath1, *filepath2;
	struct slist_t *node;
	struct slent_t *entry;
	/* Some state to get us through */
	gboolean show = TRUE;
	entry = (struct slent_t *)(data);
	filepath1 = g_file_get_uri(file1);
	/* Behavior is dependent on event type */
	switch (type) {
		case G_FILE_MONITOR_EVENT_MOVED:
			#ifndef NDEBUG
			filepath2 = g_file_get_uri(file2);
			fprintf(stderr, EVENT_FORMAT, (void *)(file_monitor), filepath1, '>', filepath2);
			g_free(filepath2);
			#endif
			/* Replace the entry */
			assert(file1 == entry->file);
			unmonitor(entry);
			g_object_unref(entry->file);
			entry->file = g_file_dup(file2);
			monitor(entry);
			break;
		case G_FILE_MONITOR_EVENT_CREATED:
			#ifndef NDEBUG
			fprintf(stderr, EVENT_FORMAT, (void *)(file_monitor), filepath1, '.', "created");
			#endif
			/* Create new entry */
			entry = malloc(sizeof(struct slent_t));
			if (create(filepath1, entry)) {
				node = malloc(sizeof(struct slist_t));
				node->head = entry;
				node->tail = list.tail;
				list.tail = node;
			} else {
				free(entry);
			}
			break;
		case G_FILE_MONITOR_EVENT_DELETED:
			#ifndef NDEBUG
			fprintf(stderr, EVENT_FORMAT, (void *)(file_monitor), filepath1, '!', "deleted");
			#endif
			/* TODO remove monitors in descendants */
			break;
		default:
			#ifndef NDEBUG
			fprintf(stderr, EVENT_FORMAT, (void *)(file_monitor), filepath1, '?', "unknown");
			#endif
			show = FALSE;
	}
	/* Display a notification if necessary */
	if (show) {
		#define BUFFER_SIZE 1024
		filepath2 = calloc(BUFFER_SIZE, sizeof(char));
		snprintf(filepath2, BUFFER_SIZE, "%s reports [%i:%p]", filepath1, (int)(type), (void *)(file_monitor));
		#ifndef NDEBUG
		fprintf(stderr, "[NOTIFY] %s: %s\n", NOTIFY_APP_NAME, filepath2);
		#else
		display(NOTIFY_APP_NAME, filepath2);
		#endif
		free(filepath2);
	}
	g_free(filepath1);
}

inline void
monitor(struct slent_t *entry)
{
	/* FIXME using (G_FILE_MONITOR_WATCH_MOUNTS | G_FILE_MONITOR_SEND_MOVED) */
	assert(entry && entry->file);
	if (entry->file_monitor) {
		unmonitor(entry);
	}
	/* Last two nulls disregard cancel and error */
	entry->file_monitor = g_file_monitor(entry->file, G_FILE_MONITOR_SEND_MOVED, NULL, NULL);
	entry->connection_id = g_signal_connect_after(entry->file_monitor, "changed", G_CALLBACK(changed), entry);
	#ifndef NDEBUG
	char *uri = g_file_get_uri(entry->file);
	if (g_signal_handler_is_connected(entry->file_monitor, entry->connection_id)) {
		fprintf(stderr, "[CONNECTED %p] %s\n", (void *)(entry), uri);
	} else {
		fprintf(stderr, "[ERROR %p] %s (failure to connect)\n", (void *)(entry), uri);
	}
	g_free(uri);
	#endif
}

inline void
unmonitor(struct slent_t *entry)
{
	assert(entry && entry->file);
	#ifndef NDEBUG
	char *uri = g_file_get_uri(entry->file);
	#endif
	if (entry->file_monitor) {
		if (g_signal_handler_is_connected(entry->file_monitor, entry->connection_id)) {
			g_signal_handler_disconnect(entry->file_monitor, entry->connection_id);
			#ifndef NDEBUG
			fprintf(stderr, "[DISCONNECTED %p] %s\n", (void *)(entry), uri);
			#endif
		}
		if (!g_file_monitor_is_cancelled(entry->file_monitor)) {
			if (g_file_monitor_cancel(entry->file_monitor)) {
				#ifndef NDEBUG
				fprintf(stderr, "[CANCELLED %p] %s\n", (void *)(entry), uri);
				#endif
			} else {
				#ifndef NDEBUG
				fprintf(stderr, "[WARNING %p] %s (failure to cancel)\n", (void *)(entry), uri);
				#endif
			}
		}
		g_object_unref(entry->file_monitor);
		entry->file_monitor = NULL;
		entry->connection_id = 0;
	}
	#ifndef NDEBUG
	g_free(uri);
	#endif
}

#endif /* MONITOR_H */
