#ifndef MONITOR_H
#define MONITOR_H
#include <libnotify/notify.h>

void display_event(char *, char *);
#define EVENT_FORMAT "[EVENT %p] %s %c %s\n"
void entry_changed(GFileMonitor *, GFile *, GFile *, GFileMonitorEvent, gpointer);

/* A simple entry list */
struct slent_t
{
	GFile *file;
	GFileMonitor *file_monitor;
};

struct slist_t
{
	struct slent_t *head;
	struct slist_t *tail;
} list;

/* Construct entries */
inline gboolean
create(char *args, struct slent_t *entry)
{
	if (entry) {
		entry->file = g_file_new_for_commandline_arg(args);
		if (g_file_query_exists(entry->file, NULL)) {
			/* Last two nulls disregard cancel and error */
			entry->file_monitor = g_file_monitor(
					entry->file,
					(G_FILE_MONITOR_WATCH_MOUNTS | G_FILE_MONITOR_SEND_MOVED),
					NULL,
					NULL);
			g_signal_connect_after(
					entry->file_monitor,
					"changed",
					G_CALLBACK(entry_changed),
					entry);
			return TRUE;
		}
		g_object_unref(entry->file);
	}
	return FALSE;
}

/* Cleanup entries */
inline void
destroy(struct slent_t *entry)
{
	if (entry) {
		if (entry->file_monitor) {
			g_file_monitor_cancel(entry->file_monitor);
			g_object_unref(entry->file_monitor);
		}
		if (entry->file) {
			g_object_unref(entry->file);
		}
		free(entry);
	}
}

/* Callbacks */

#define NOTIFY_APP_NAME "bloomd"
#define NOTIFY_TIMEOUT 3000
#define NOTIFY_TYPE "dialog-information"

void
display_event(char *title, char * message)
{
	NotifyNotification *n = notify_notification_new(title, message, NOTIFY_TYPE, NULL);
	notify_notification_set_timeout(n, NOTIFY_TIMEOUT);
	notify_notification_set_category(n, NOTIFY_APP_NAME);
	notify_notification_set_urgency(n, NOTIFY_URGENCY_CRITICAL);
	notify_notification_show(n, NULL);
}

void
entry_changed(GFileMonitor *monitor,
		GFile *file1, GFile *file2,
		GFileMonitorEvent type,
		gpointer __attribute__ ((unused)) data)
{
	char *filepath1, *filepath2;
	display_event(NOTIFY_APP_NAME, filepath1 = g_file_get_uri(file1));
	switch (type) {
		case G_FILE_MONITOR_EVENT_MOVED:
			filepath2 = g_file_get_uri(file2);
			fprintf(stderr, EVENT_FORMAT, (void *)(monitor), filepath1, '>', filepath2);
			g_file_monitor_cancel(monitor);
			/* TODO use data to update the file monitor */
			g_free(filepath2);
			break;
		case G_FILE_MONITOR_EVENT_CREATED:
			fprintf(stderr, EVENT_FORMAT, (void *)(monitor), filepath1, '.', "created");
			break;
		case G_FILE_MONITOR_EVENT_DELETED:
			fprintf(stderr, EVENT_FORMAT, (void *)(monitor), filepath1, '!', "deleted");
			break;
		default:
			fprintf(stderr, EVENT_FORMAT, (void *)(monitor), filepath1, '?', "unknown");
	}
	g_free(filepath1);
}

#endif /* MONITOR_H */
