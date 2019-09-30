/*
 *  Steam Mobile Plugin for Pidgin
 *  Copyright (C) 2012-2016 Eion Robb
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef LIBSTEAM_H
#define LIBSTEAM_H

/* Maximum number of simultaneous connections to a server */
#define STEAM_MAX_CONNECTIONS 16

#include <glib.h>

#include <errno.h>
#include <string.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#ifdef __GNUC__
	#include <unistd.h>
#endif

#ifndef G_GNUC_NULL_TERMINATED
#	if __GNUC__ >= 4
#		define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#	else
#		define G_GNUC_NULL_TERMINATED
#	endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#ifdef _WIN32
#	include "win32dep.h"
#	define dlopen(a,b) LoadLibrary(a)
#	define RTLD_LAZY
#	define dlsym(a,b) GetProcAddress(a,b)
#	define dlclose(a) FreeLibrary(a)
#else
#	include <arpa/inet.h>
#	include <dlfcn.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#endif

#include <json-glib/json-glib.h>

#define json_object_get_int_member(JSON_OBJECT, MEMBER) \
	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_int_member(JSON_OBJECT, MEMBER) : 0)
#define json_object_get_string_member(JSON_OBJECT, MEMBER) \
	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_string_member(JSON_OBJECT, MEMBER) : NULL)
#define json_object_get_array_member(JSON_OBJECT, MEMBER) \
	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_array_member(JSON_OBJECT, MEMBER) : NULL)
#define json_object_get_object_member(JSON_OBJECT, MEMBER) \
	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_object_member(JSON_OBJECT, MEMBER) : NULL)
#define json_object_get_boolean_member(JSON_OBJECT, MEMBER) \
	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_boolean_member(JSON_OBJECT, MEMBER) : FALSE)

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "accountopt.h"
#include "blist.h"
#include "core.h"
#include "connection.h"
#include "debug.h"
#include "dnsquery.h"
#include "proxy.h"
#include "prpl.h"
#include "request.h"
#include "savedstatuses.h"
#include "sslconn.h"
#include "version.h"

#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 12
#	define atoll(a) g_ascii_strtoll(a, NULL, 0)
#endif

#define FB_MAX_MSG_RETRY 2

#define STEAM_PLUGIN_ID "prpl-steam-mobile"
#define STEAM_PLUGIN_VERSION "1.7"

#define STEAM_CAPTCHA_URL "https://steamcommunity.com/public/captcha.php?gid=%s"

typedef struct _SteamAccount SteamAccount;
typedef struct _SteamBuddy SteamBuddy;

typedef void (*SteamFunc)(SteamAccount *sa);

struct _SteamAccount {
	PurpleAccount *account;
	PurpleConnection *pc;
	GSList *conns; /**< A list of all active SteamConnections */
	GQueue *waiting_conns; /**< A list of all SteamConnections waiting to process */
	GSList *dns_queries;
	GHashTable *cookie_table;
	GHashTable *hostname_ip_cache;
	
	GHashTable *sent_messages_hash;
	guint poll_timeout;
	
	gchar *umqid;
	guint message;
	gchar *steamid;
	gchar *sessionid;
	gint idletime;
	guint last_message_timestamp;
	gchar *cached_access_token;
	
	guint watchdog_timeout;
	
	gchar *captcha_gid;
	gchar *captcha_text;
	gchar *twofactorcode;
};

struct _SteamBuddy {
	SteamAccount *sa;
	PurpleBuddy *buddy;
	
	gchar *steamid;
	gchar *personaname;
	gchar *realname;
	gchar *profileurl;
	guint lastlogoff;
	gchar *avatar;
	guint personastateflags;
	
	gchar *gameid;
	gchar *gameextrainfo;
	gchar *gameserversteamid;
	gchar *lobbysteamid;
	gchar *gameserverip;
};

#define STEAMID_IS_GROUP(id) G_UNLIKELY(((g_ascii_strtoll((id), NULL, 10) >> 52) & 0x0F) == 7)

#endif /* LIBSTEAM_H */
