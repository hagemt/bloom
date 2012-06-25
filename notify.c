#include <libnotify/notify.h>

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>

#define NOTIFY_APP_NAME "bloom"
#define NOTIFY_TIMEOUT 3000
#define NOTIFY_TYPE "dialog-information"

int
main(int argc, char *argv[])
{
	NotifyNotification *example;
	GtkStatusIcon *icon;

	setlocale(LC_ALL, "");
	gtk_init(&argc, &argv);
	if (!notify_init(NOTIFY_APP_NAME)) {
		fprintf(stderr, "[FATAL] '%s' (unable to initialize libnotify)\n", NOTIFY_APP_NAME);
		return (EXIT_FAILURE);
	}

	/* Show a notification and quit */
	example = notify_notification_new(NOTIFY_APP_NAME, NOTIFY_APP_NAME, NOTIFY_TYPE, NULL);
	icon = gtk_status_icon_new_from_stock(GTK_STOCK_YES);
	gtk_status_icon_set_visible(icon, TRUE);
	notify_notification_attach_to_status_icon(example, icon);
	notify_notification_set_timeout(example, NOTIFY_TIMEOUT);
	notify_notification_set_category(example, NOTIFY_APP_NAME);
	notify_notification_set_urgency(example, NOTIFY_URGENCY_CRITICAL);
	notify_notification_show(example, NULL);

	notify_uninit();
	return (EXIT_SUCCESS);
}
