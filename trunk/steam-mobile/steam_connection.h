
#ifndef STEAM_CONNECTION_H
#define STEAM_CONNECTION_H

#include "libsteam.h"

typedef void (*SteamProxyCallbackFunc)(SteamAccount *sa, JsonObject *obj, gpointer user_data);

/*
 * This is a bitmask.
 */
typedef enum
{
	STEAM_METHOD_GET  = 0x0001,
	STEAM_METHOD_POST = 0x0002,
	STEAM_METHOD_SSL  = 0x0004
} SteamMethod;

typedef struct _SteamConnection SteamConnection;
struct _SteamConnection {
	SteamAccount *sa;
	SteamMethod method;
	gchar *hostname;
	gchar *url;
	GString *request;
	SteamProxyCallbackFunc callback;
	gpointer user_data;
	char *rx_buf;
	size_t rx_len;
	PurpleProxyConnectData *connect_data;
	PurpleSslConnection *ssl_conn;
	int fd;
	guint input_watcher;
	gboolean connection_keepalive;
	time_t request_time;
};

void steam_connection_destroy(SteamConnection *steamcon);
void steam_post_or_get(SteamAccount *sa, SteamMethod method,
		const gchar *host, const gchar *url, const gchar *postdata,
		SteamProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive);
gchar *steam_cookies_to_string(SteamAccount *sa);

#endif /* STEAM_CONNECTION_H */
