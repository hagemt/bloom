#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "monitor.h"

static GMainLoop *main_loop = NULL;
static struct sigaction signal_action;

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
	notify_uninit();

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

	/* Signal handling */
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

	/* Initialization */
	setlocale(LC_ALL, "");
	gtk_init(&argc, &argv);
	if (!notify_init(NOTIFY_APP_NAME)) {
		fprintf(stderr, "[FATAL] '%s' (notify_init)\n", NOTIFY_APP_NAME);
		return (EXIT_FAILURE);
	}
	list.head = NULL;
	list.tail = NULL;
	current = &list;

	/* Argument Parsing (TODO improve) */
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

	/* Main Loop */
	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);
	handle_signal(0);
	return (EXIT_SUCCESS);
}
