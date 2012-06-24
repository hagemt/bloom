#include <gio/gio.h>
#include <glib.h>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static GMainLoop *main_loop = NULL;
static struct sigaction signal_action;

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

/* Callback */
#define EVENT_FORMAT "[EVENT %p] %s %c %s\n"
void
entry_changed(GFileMonitor *monitor,
		GFile *file1, GFile *file2,
		GFileMonitorEvent type,
		gpointer __attribute__ ((unused)) data)
{
	char *filepath1, *filepath2;
	filepath1 = g_file_get_uri(file1);
	switch (type) {
		case G_FILE_MONITOR_EVENT_MOVED:
			filepath2 = g_file_get_uri(file2);
			fprintf(stderr, EVENT_FORMAT, (void *)(monitor), filepath1, '>', filepath2);
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
					NULL);
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

void
handle_signal(int signum)
{
	struct slist_t *current, *next;

	/* Notify user */
	putchar('\n');
	if (signum) {
		fprintf(stderr, "[WARNING] signal caught [code %i: %s]\n",
				signum, strsignal(signum));
	}

	/* Stop the main loop */
	if (main_loop) {
		if (g_main_loop_is_running(main_loop)) {
			g_main_loop_quit(main_loop);
		}
		g_main_loop_unref(main_loop);
	}

	/* Cleanup the list */
	current = list.tail;
	while (current) {
		#ifndef NDEBUG
		fprintf(stderr, "[ENTRY] { %p, %p }\n",
				(void *)(current->head->file),
				(void *)(current->head->file_monitor));
		#endif
		destroy(current->head);
		next = current->tail;
		free(current);
		current = next;
	}

	/* Re-throw termination signals */
	if (sigismember(&signal_action.sa_mask, signum)) {
		sigemptyset(&signal_action.sa_mask);
		signal_action.sa_handler = SIG_DFL;
		sigaction(signum, &signal_action, NULL);
		raise(signum);
	}
}

int
main(int argc, char *argv[])
{
	struct slist_t *current, *next;

	struct sigaction old_signal_action;
	sigset_t termination_signals;
	sigemptyset(&termination_signals);
	memset(&signal_action, '\0', sizeof(struct sigaction));
	sigfillset(&signal_action.sa_mask);
	signal_action.sa_handler = &handle_signal;
	sigaction(SIGTERM, NULL, &old_signal_action);
	if (old_signal_action.sa_handler != SIG_IGN) {
		sigaction(SIGTERM, &signal_action, NULL);
		sigaddset(&termination_signals, SIGTERM);
	}
	sigaction(SIGINT, NULL, &old_signal_action);
	if (old_signal_action.sa_handler != SIG_IGN) {
		sigaction(SIGINT, &signal_action, NULL);
		sigaddset(&termination_signals, SIGINT);
	}
	memcpy(&signal_action.sa_mask, &termination_signals, sizeof(sigset_t));

	/* Init */
	g_type_init();
	list.head = NULL;
	list.tail = NULL;
	current = &list;

	/* Parse */
	while (--argc) {
		next = malloc(sizeof(struct slist_t));
		next->head = malloc(sizeof(struct slent_t));
		next->tail = NULL;
		if (create(argv[argc], next->head)) {
			current->tail = next;
			current = current->tail;
		} else {
			fprintf(stderr, "[ERROR] '%s' (does not exist)\n", argv[argc]);
			free(next->head);
			free(next);
		}
	}

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);
	handle_signal(0);
	return (EXIT_SUCCESS);
}
