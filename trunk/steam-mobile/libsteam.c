

#include "libsteam.h"
#include "steam_connection.h"

#if !PURPLE_VERSION_CHECK(3, 0, 0)
	#define purple_connection_error purple_connection_error_reason
	#define purple_notify_user_info_add_pair_html purple_notify_user_info_add_pair
#endif

static const gchar *
steam_personastate_to_statustype(gint64 state)
{
	const char *status_id;
	PurpleStatusPrimitive prim;
	switch(state)
	{
		default:
		case 0:
			prim = PURPLE_STATUS_OFFLINE; 
			break;
		case 1:
			prim = PURPLE_STATUS_AVAILABLE;
			break;
		case 2:
			prim = PURPLE_STATUS_UNAVAILABLE;
			break;
		case 3:
			prim = PURPLE_STATUS_AWAY;
			break;
		case 4:
			prim = PURPLE_STATUS_EXTENDED_AWAY;
			break;
	}
	status_id = purple_primitive_get_id_from_type(prim);
	return status_id;
}

static void steam_fetch_new_sessionid(SteamAccount *sa);
static void steam_get_friend_summaries(SteamAccount *sa, const gchar *who);

static void
steam_friend_action(SteamAccount *sa, const gchar *who, const gchar *action)
{
	//Possible actions: add, remove
	GString *postdata = g_string_new(NULL);
	const gchar *url;

	if (g_str_equal(action, "remove"))
		url = "/actions/RemoveFriendAjax";
	else
		url = "/actions/AddFriendAjax";
	
	g_string_append_printf(postdata, "steamid=%s&", purple_url_encode(who));
	g_string_append_printf(postdata, "sessionID=%s", purple_url_encode(sa->sessionid));
	
	steam_post_or_get(sa, STEAM_METHOD_POST | STEAM_METHOD_SSL, "steamcommunity.com", url, postdata->str, NULL, NULL, FALSE);
	
	g_string_free(postdata, TRUE);
	
	if (g_str_equal(action, "add"))
	{
		steam_get_friend_summaries(sa, who);
	}
}

static void
steam_friend_invite_action(SteamAccount *sa, const gchar *who, const gchar *action)
{
	//Possible actions:  accept, ignore, block
	GString *postdata = g_string_new(NULL);
	gchar *url = g_strdup_printf("/profiles/%s/home_process", purple_url_encode(sa->steamid));
	
	g_string_append(postdata, "json=1&");
	g_string_append(postdata, "xml=1&");
	g_string_append(postdata, "action=approvePending&");
	g_string_append(postdata, "itype=friend&");
	g_string_append_printf(postdata, "perform=%s&", purple_url_encode(action));
	g_string_append_printf(postdata, "sessionID=%s&", purple_url_encode(sa->sessionid));
	g_string_append_printf(postdata, "id=%s", purple_url_encode(who));
	
	steam_post_or_get(sa, STEAM_METHOD_POST | STEAM_METHOD_SSL, "steamcommunity.com", url, postdata->str, NULL, NULL, FALSE);
	
	g_free(url);
	g_string_free(postdata, TRUE);
}

static void
steam_fetch_new_sessionid_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
{
	if (g_hash_table_lookup(sa->cookie_table, "sessionid"))
	{
		g_free(sa->sessionid);
		sa->sessionid = g_strdup(g_hash_table_lookup(sa->cookie_table, "sessionid"));
	}
}

static void
steam_fetch_new_sessionid(SteamAccount *sa)
{
	gchar *steamLogin;
	
	steamLogin = g_strconcat(sa->steamid, "||oauth:", purple_account_get_string(sa->account, "access_token", ""), NULL);
	
	g_hash_table_replace(sa->cookie_table, g_strdup("forceMobile"), g_strdup("1"));
	g_hash_table_replace(sa->cookie_table, g_strdup("mobileClient"), g_strdup("ios"));
	g_hash_table_replace(sa->cookie_table, g_strdup("mobileClientVersion"), g_strdup("1291812"));
	g_hash_table_replace(sa->cookie_table, g_strdup("Steam_Language"), g_strdup("english"));
	g_hash_table_replace(sa->cookie_table, g_strdup("steamLogin"), steamLogin);
	
	steam_post_or_get(sa, STEAM_METHOD_GET | STEAM_METHOD_SSL, "steamcommunity.com", "/mobilesettings/GetManifest/v0001", NULL, steam_fetch_new_sessionid_cb, NULL, FALSE);
}

static void
steam_get_icon_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	PurpleBuddy *buddy = user_data;
	SteamBuddy *sbuddy;
	
	if (!buddy || !buddy->proto_data)
		return;
	
	sbuddy = buddy->proto_data;
	
	purple_buddy_icons_set_for_user(buddy->account, buddy->name, g_memdup(url_text, len), len, sbuddy->avatar);
}

static void
steam_get_icon(PurpleBuddy *buddy)
{
	const gchar *old_avatar = purple_buddy_icons_get_checksum_for_user(buddy);
	SteamBuddy *sbuddy;
	
	purple_debug_info("steam", "getting new buddy icon for %s\n", buddy->name);
	
	if (!buddy || !buddy->proto_data)
	{
		purple_debug_info("steam", "no buddy proto_data :(\n");
		return;
	}
	
	sbuddy = buddy->proto_data;
	if (!sbuddy->avatar || (old_avatar && g_str_equal(sbuddy->avatar, old_avatar)))
		return;
	
#if PURPLE_VERSION_CHECK(3, 0, 0)
	purple_util_fetch_url_request(buddy->account, sbuddy->avatar, TRUE, NULL, FALSE, NULL, FALSE, -1, steam_get_icon_cb, buddy);
#else
	purple_util_fetch_url_request(sbuddy->avatar, TRUE, NULL, FALSE, NULL, FALSE, steam_get_icon_cb, buddy);
#endif
}

static void steam_poll(SteamAccount *sa, gboolean secure, guint message);
gboolean steam_timeout(gpointer userdata)
{
	SteamAccount *sa = userdata;
	steam_poll(sa, FALSE, sa->message);
	return FALSE;
}

static void
steam_auth_accept_cb(gpointer user_data)
{
	PurpleBuddy *temp_buddy = user_data;
	PurpleAccount *account = purple_buddy_get_account(temp_buddy);
	PurpleConnection *pc = purple_account_get_connection(account);
	SteamAccount *sa = pc->proto_data;
	
	steam_friend_invite_action(sa, temp_buddy->name, "accept");
	
	purple_buddy_destroy(temp_buddy);
}

static void
steam_auth_reject_cb(gpointer user_data)
{
	PurpleBuddy *temp_buddy = user_data;
	PurpleAccount *account = purple_buddy_get_account(temp_buddy);
	PurpleConnection *pc = purple_account_get_connection(account);
	SteamAccount *sa = pc->proto_data;
	
	steam_friend_invite_action(sa, temp_buddy->name, "ignore");
	
	purple_buddy_destroy(temp_buddy);
}

void
steam_search_results_add_buddy(PurpleConnection *pc, GList *row, void *user_data)
{
	PurpleAccount *account = purple_connection_get_account(pc);

	if (!purple_find_buddy(account, g_list_nth_data(row, 0)))
		purple_blist_request_add_buddy(account, g_list_nth_data(row, 0), "Steam", g_list_nth_data(row, 1));
}

void
steam_search_display_results(SteamAccount *sa, JsonObject *obj, gpointer user_data)
{
	PurpleNotifySearchResults *results;
	PurpleNotifySearchColumn *column;
	JsonArray *players = NULL;
	guint index;
	gchar *search_term = user_data;
	
	if (!json_object_has_member(obj, "players"))
	{
		g_free(search_term);
		return;
	}
	
	results = purple_notify_searchresults_new();
	if (results == NULL)
	{
		g_free(search_term);
		return;
	}
		
	/* columns: Friend ID, Name, Network */
	column = purple_notify_searchresults_column_new(_("ID"));
	purple_notify_searchresults_column_add(results, column);
	column = purple_notify_searchresults_column_new(_("Persona"));
	purple_notify_searchresults_column_add(results, column);
	column = purple_notify_searchresults_column_new(_("Real name"));
	purple_notify_searchresults_column_add(results, column);
	column = purple_notify_searchresults_column_new(_("Profile"));
	purple_notify_searchresults_column_add(results, column);
	
	purple_notify_searchresults_button_add(results,
			PURPLE_NOTIFY_BUTTON_ADD,
			steam_search_results_add_buddy);
	
	players = json_object_get_array_member(obj, "players");
	for(index = 0; index < json_array_get_length(players); index++)
	{
		JsonObject *player = json_array_get_object_element(players, index);
		
		/* the row in the search results table */
		/* prepend to it backwards then reverse to speed up adds */
		GList *row = NULL;
		
		row = g_list_prepend(row, g_strdup(json_object_get_string_member(player, "steamid")));
		row = g_list_prepend(row, g_strdup(json_object_get_string_member(player, "personaname")));
		row = g_list_prepend(row, g_strdup(json_object_get_string_member(player, "realname")));
		row = g_list_prepend(row, g_strdup(json_object_get_string_member(player, "profileurl")));
		
		row = g_list_reverse(row);
		
		purple_notify_searchresults_row_add(results, row);
	}
	
	purple_notify_searchresults(sa->pc, NULL, search_term, NULL,
			results, NULL, NULL);
}

void
steam_search_users_text_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
{
	JsonArray *results = NULL;
	guint index;
	GString *userids;
	gchar *search_term = user_data;
	
	if (json_object_get_int_member(obj, "count") == 0 ||
		!json_object_has_member(obj, "results"))
	{
		g_free(search_term);
		return;
	}
	
	userids = g_string_new("");
	
	results = json_object_get_array_member(obj, "results");
	for(index = 0; index < json_array_get_length(results); index++)
	{
		JsonObject *result = json_array_get_object_element(results, index);
		g_string_append_printf(userids, "%s,", json_object_get_string_member(result, "steamid"));
	}
	
	if (userids && userids->str && *userids->str)
	{
		GString *url = g_string_new("/ISteamUserOAuth/GetUserSummaries/v0001?");
		g_string_append_printf(url, "access_token=%s&", purple_url_encode(purple_account_get_string(sa->account, "access_token", "")));
		g_string_append_printf(url, "steamids=%s", purple_url_encode(userids->str));
		
		steam_post_or_get(sa, STEAM_METHOD_GET | STEAM_METHOD_SSL, NULL, url->str, NULL, steam_search_display_results, search_term, TRUE);
		
		g_string_free(url, TRUE);
	} else {
		g_free(search_term);
	}
	
	g_string_free(userids, TRUE);
}

void
steam_search_users_text(gpointer user_data, const gchar *text)
{
	SteamAccount *sa = user_data;
	GString *url = g_string_new("/ISteamUserOAuth/Search/v0001?");
	
	g_string_append_printf(url, "access_token=%s&", purple_url_encode(purple_account_get_string(sa->account, "access_token", "")));
	g_string_append_printf(url, "keywords=%s&", purple_url_encode(text));
	g_string_append(url, "offset=0&");
	g_string_append(url, "count=50&");
	g_string_append(url, "targets=users&");
	g_string_append(url, "fields=all&");
	
	steam_post_or_get(sa, STEAM_METHOD_GET | STEAM_METHOD_SSL, "api.steampowered.com", url->str, NULL, steam_search_users_text_cb, g_strdup(text), FALSE);
	
	g_string_free(url, TRUE);
}

void
steam_search_users(PurplePluginAction *action)
{
	PurpleConnection *pc = (PurpleConnection *) action->context;
	SteamAccount *sa = pc->proto_data;
	
	purple_request_input(pc, "Search for Steam Friends",
					   "Search for Steam Friends",
					   NULL,
					   NULL, FALSE, FALSE, NULL,
					   _("_Search"), G_CALLBACK(steam_search_users_text),
					   _("_Cancel"), NULL,
					   purple_connection_get_account(pc), NULL, NULL,
					   sa);

}

static void
steam_poll_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
{
	JsonArray *messages = NULL;
	guint index;
	gint secure = GPOINTER_TO_INT(user_data);
	
	if (json_object_has_member(obj, "messages"))
		messages = json_object_get_array_member(obj, "messages");
	
	if (messages != NULL)
	for(index = 0; index < json_array_get_length(messages); index++)
	{
		JsonObject *message = json_array_get_object_element(messages, index);
		const gchar *type = json_object_get_string_member(message, "type");
		purple_debug_info("steam", "new message of type %s\n", type);
		if (g_str_equal(type, "typing"))
		{
			serv_got_typing(sa->pc, json_object_get_string_member(message, "steamid_from"), 20, PURPLE_TYPING);
		} else if (g_str_equal(type, "saytext") || g_str_equal(type, "emote") || g_str_equal(type, "my_saytext") || g_str_equal(type, "my_emote"))
		{
			if (json_object_has_member(message, "secure_message_id"))
			{
				guint secure_message_id = (guint) json_object_get_int_member(message, "secure_message_id");
				steam_poll(sa, TRUE, secure_message_id);
				sa->message = MAX(sa->message, secure_message_id);
			} else {
				guint new_timestamp = (guint) json_object_get_int_member(message, "timestamp");
				if (new_timestamp > sa->last_message_timestamp)
				{
					gchar *text, *html;
					PurpleMessageFlags flags;
					if (g_str_equal(type, "emote") || g_str_equal(type, "my_emote"))
					{
						text = g_strconcat("/me ", json_object_get_string_member(message, "text"), NULL);
					} else {
						text = g_strdup(json_object_get_string_member(message, "text"));
					}
					html = purple_strdup_withhtml(text);
					if (g_str_has_prefix(type, "my_"))
						flags = PURPLE_MESSAGE_SEND;
					else
						flags = PURPLE_MESSAGE_RECV;
					serv_got_im(sa->pc, json_object_get_string_member(message, "steamid_from"), html, flags, time(NULL));
					g_free(html);
					g_free(text);
					
					sa->last_message_timestamp = new_timestamp;
				}
			}
		} else if (g_str_equal(type, "personastate"))
		{
			gint64 personastate = json_object_get_int_member(message, "persona_state");
			const gchar *steamid = json_object_get_string_member(message, "steamid_from");
			purple_prpl_got_user_status(sa->account, steamid, steam_personastate_to_statustype(personastate), NULL);
			serv_got_alias(sa->pc, steamid, json_object_get_string_member(message, "persona_name"));
			steam_get_friend_summaries(sa, steamid);
		} else if (g_str_equal(type, "personarelationship"))
		{
			const gchar *steamid = json_object_get_string_member(message, "steamid_from");
			gint64 persona_state = json_object_get_int_member(message, "persona_state");
			if (persona_state == 0)
				purple_blist_remove_buddy(purple_find_buddy(sa->account, steamid));
			else if (persona_state == 2)
				purple_account_request_authorization(
					sa->account, steamid, NULL,
					NULL, NULL, TRUE,
					steam_auth_accept_cb, steam_auth_reject_cb, purple_buddy_new(sa->account, steamid, NULL));
			else if (persona_state == 3)
				if (!purple_find_buddy(sa->account, steamid))
					purple_blist_add_buddy(purple_buddy_new(sa->account, steamid, NULL), NULL, purple_find_group("Steam"), NULL);
		} else if (g_str_equal(type, "leftconversation"))
		{
			const gchar *steamid = json_object_get_string_member(message, "steamid_from");
			PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, steamid, sa->account);
			const gchar *alias = purple_buddy_get_alias(purple_find_buddy(sa->account, steamid));
			gchar *has_left_msg = g_strdup_printf("%s has left the conversation", alias ? alias : "User");
			purple_conversation_write(conv, "", has_left_msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
			g_free(has_left_msg);
		}
	}
	
	if (json_object_has_member(obj, "messagelast"))
		sa->message = MAX(sa->message, (guint) json_object_get_int_member(obj, "messagelast"));
	
	if (json_object_has_member(obj, "error") && g_str_equal(json_object_get_string_member(obj, "error"), "Not Logged On"))
	{
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Reconnect needed"));
	}
	
	if (!secure)
	{
		sa->poll_timeout = purple_timeout_add_seconds(1, steam_timeout, sa);
	}
}

static void
steam_poll(SteamAccount *sa, gboolean secure, guint message)
{
	GString *post = g_string_new(NULL);
	SteamMethod method = STEAM_METHOD_POST;
	const gchar *url = NULL;
	
	if (secure == TRUE)
	{
		method |= STEAM_METHOD_SSL;
		g_string_append_printf(post, "access_token=%s&", purple_url_encode(purple_account_get_string(sa->account, "access_token", "")));
	} else {
		g_string_append_printf(post, "steamid=%s&", purple_url_encode(sa->steamid));
	}
	g_string_append_printf(post, "umqid=%s&", purple_url_encode(sa->umqid));
	g_string_append_printf(post, "message=%u", message?message:sa->message);
	g_string_append_printf(post, "secidletime=%d", sa->idletime);
	
	url = "/ISteamWebUserPresenceOAuth/PollStatus/v0001";
	if (secure == TRUE)
		url = "/ISteamWebUserPresenceOAuth/Poll/v0001";
	steam_post_or_get(sa, method, NULL, url, post->str, steam_poll_cb, GINT_TO_POINTER(secure?1:0), TRUE);
	
	g_string_free(post, TRUE);
}

static void
steam_got_friend_summaries(SteamAccount *sa, JsonObject *obj, gpointer user_data)
{
	JsonArray *players = json_object_get_array_member(obj, "players");
	PurpleBuddy *buddy;
	SteamBuddy *sbuddy;
	guint index;
	
	for(index = 0; index < json_array_get_length(players); index++)
	{
		JsonObject *player = json_array_get_object_element(players, index);
		const gchar *steamid = json_object_get_string_member(player, "steamid");
		gint64 personastate = -1;
		buddy = purple_find_buddy(sa->account, steamid);
		if (!buddy)
			continue;
		sbuddy = buddy->proto_data;
		if (sbuddy == NULL)
		{
			sbuddy = g_new0(SteamBuddy, 1);
			buddy->proto_data = sbuddy;
			sbuddy->steamid = g_strdup(steamid);
		}
		
		g_free(sbuddy->personaname); sbuddy->personaname = g_strdup(json_object_get_string_member(player, "personaname"));
		serv_got_alias(sa->pc, steamid, sbuddy->personaname);
	
		g_free(sbuddy->realname); sbuddy->realname = g_strdup(json_object_get_string_member(player, "realname"));
		g_free(sbuddy->profileurl); sbuddy->profileurl = g_strdup(json_object_get_string_member(player, "profileurl"));
		g_free(sbuddy->avatar); sbuddy->avatar = g_strdup(json_object_get_string_member(player, "avatarfull"));
		g_free(sbuddy->gameid); sbuddy->gameid = g_strdup(json_object_get_string_member(player, "gameid"));
		g_free(sbuddy->gameextrainfo); sbuddy->gameextrainfo = g_strdup(json_object_get_string_member(player, "gameextrainfo"));
		g_free(sbuddy->gameserversteamid); sbuddy->gameserversteamid = g_strdup(json_object_get_string_member(player, "gameserversteamid"));
		g_free(sbuddy->lobbysteamid); sbuddy->lobbysteamid = g_strdup(json_object_get_string_member(player, "lobbysteamid"));
		
		sbuddy->lastlogoff = (guint) json_object_get_int_member(player, "lastlogoff");
		
		personastate = json_object_get_int_member(player, "personastate");
		purple_prpl_got_user_status(sa->account, steamid, steam_personastate_to_statustype(personastate), NULL);
		
		steam_get_icon(buddy);
	}
}

static void
steam_get_friend_summaries(SteamAccount *sa, const gchar *who)
{
	GString *url;
	
	g_return_if_fail(sa && who && *who);
	
	url = g_string_new("/ISteamUserOAuth/GetUserSummaries/v0001?");
	g_string_append_printf(url, "access_token=%s&", purple_url_encode(purple_account_get_string(sa->account, "access_token", "")));
	g_string_append_printf(url, "steamids=%s", purple_url_encode(who));
	
	steam_post_or_get(sa, STEAM_METHOD_GET | STEAM_METHOD_SSL, NULL, url->str, NULL, steam_got_friend_summaries, NULL, TRUE);
	
	g_string_free(url, TRUE);
}

static void
steam_get_friend_list_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
{
	JsonArray *friends = json_object_get_array_member(obj, "friends");
	PurpleGroup *group = NULL;
	gchar *users_to_fetch = g_strdup(""), *temp;
	guint index;
	
	for(index = 0; index < json_array_get_length(friends); index++)
	{
		JsonObject *friend = json_array_get_object_element(friends, index);
		const gchar *steamid = json_object_get_string_member(friend, "steamid");
		const gchar *relationship = json_object_get_string_member(friend, "relationship");
		if (g_str_equal(relationship, "friend"))
		{
			if (!purple_find_buddy(sa->account, steamid))
			{
				if (!group)
				{
					group = purple_find_group("Steam");
					if (!group)
					{
						group = purple_group_new("Steam");
						purple_blist_add_group(group, NULL);
					}
				}
				purple_blist_add_buddy(purple_buddy_new(sa->account, steamid, NULL), NULL, group, NULL);
			}
			temp = users_to_fetch;
			users_to_fetch = g_strconcat(users_to_fetch, ",", steamid, NULL);
			g_free(temp);
		} else if (g_str_equal(relationship, "requestrecipient"))
		{
			purple_account_request_authorization(
					sa->account, steamid, NULL,
					NULL, NULL, TRUE,
					steam_auth_accept_cb, steam_auth_reject_cb, purple_buddy_new(sa->account, steamid, NULL));
		}
	}
	
	if (users_to_fetch && *users_to_fetch)
	{
		steam_get_friend_summaries(sa, users_to_fetch);
	}
	g_free(users_to_fetch);
}

static void
steam_get_friend_list(SteamAccount *sa)
{
	GString *url = g_string_new("/ISteamUserOAuth/GetFriendList/v0001?");
	
	g_string_append_printf(url, "access_token=%s&", purple_url_encode(purple_account_get_string(sa->account, "access_token", "")));
	g_string_append_printf(url, "steamid=%s&", purple_url_encode(sa->steamid));
	g_string_append(url, "relationship=friend,requestrecipient");
	
	steam_post_or_get(sa, STEAM_METHOD_GET | STEAM_METHOD_SSL, NULL, url->str, NULL, steam_get_friend_list_cb, NULL, TRUE);
	
	g_string_free(url, TRUE);

}

/******************************************************************************/
/* PRPL functions */
/******************************************************************************/

static const char *steam_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "steam";
}

static gchar *steam_status_text(PurpleBuddy *buddy)
{
	SteamBuddy *sbuddy = buddy->proto_data;

	if (sbuddy && sbuddy->gameextrainfo)
		return g_markup_printf_escaped("In game %s", sbuddy->gameextrainfo);

	return NULL;
}

void
steam_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	SteamBuddy *sbuddy = buddy->proto_data;
	
	if (sbuddy)
	{
		purple_notify_user_info_add_pair_html(user_info, "Name", sbuddy->personaname);
		purple_notify_user_info_add_pair_html(user_info, "Real Name", sbuddy->realname);
		if (sbuddy->gameextrainfo)
			purple_notify_user_info_add_pair_html(user_info, "In game", sbuddy->gameextrainfo);
	}
}

const gchar *
steam_list_emblem(PurpleBuddy *buddy)
{
	SteamBuddy *sbuddy = buddy->proto_data;
	
	if (sbuddy && sbuddy->gameid)
	{
		return "game";
	}
	
	return NULL;
}

GList *
steam_status_types(PurpleAccount *account)
{
	GList *types = NULL;
	PurpleStatusType *status;

	purple_debug_info("steam", "status_types\n");
	
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, "Online", TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, "Offline", TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	status = purple_status_type_new_full(PURPLE_STATUS_UNAVAILABLE, NULL, "Busy", TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	status = purple_status_type_new_full(PURPLE_STATUS_AWAY, NULL, "Away", TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	status = purple_status_type_new_full(PURPLE_STATUS_EXTENDED_AWAY, NULL, "Snoozing", TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	return types;
}

static void
steam_login_access_token_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
{
	if (!g_str_equal(json_object_get_string_member(obj, "error"), "OK"))
	{
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Bad username/password or Steam Guard Code required"));
		return;
	}

	if (json_object_has_member(obj, "umqid"))
	{
		g_free(sa->umqid);
		sa->umqid = g_strdup(json_object_get_string_member(obj, "umqid"));
	}
	if (json_object_has_member(obj, "steamid"))
	{
		g_free(sa->steamid);
		sa->steamid = g_strdup(json_object_get_string_member(obj, "steamid"));
	}
	sa->message = (guint) json_object_get_int_member(obj, "message");
	
	purple_connection_set_state(sa->pc, PURPLE_CONNECTED);
	
	steam_get_friend_list(sa);
	steam_poll(sa, FALSE, 0);
	
	steam_fetch_new_sessionid(sa);
}

static void
steam_login_with_access_token(SteamAccount *sa)
{
	gchar *postdata;
	
	postdata = g_strdup_printf("access_token=%s", purple_url_encode(purple_account_get_string(sa->account, "access_token", "")));
	steam_post_or_get(sa, STEAM_METHOD_POST | STEAM_METHOD_SSL, NULL, "/ISteamWebUserPresenceOAuth/Logon/v0001", postdata, steam_login_access_token_cb, NULL, TRUE);
	g_free(postdata);
}

static void 
steam_set_steam_guard_token_cb(gpointer data, const gchar *steam_guard_token)
{
	PurpleAccount *account = data;
	
	purple_account_set_string(account, "steam_guard_code", steam_guard_token);
	purple_account_connect(account);
}

static void
steam_login_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
{
	if(json_object_has_member(obj, "access_token"))
	{
		purple_account_set_string(sa->account, "access_token", json_object_get_string_member(obj, "access_token"));
		steam_login_with_access_token(sa);
	} else 
	{
		const gchar *x_errorcode = json_object_get_string_member(obj, "x_errorcode");
		const gchar *error_description = json_object_get_string_member(obj, "error_description");
		if (g_str_equal(x_errorcode, "steamguard_code_required"))
		{
			purple_request_input(NULL, NULL, _("Set your Steam Guard Code"),
						_("Copy the Steam Guard Code you will have received in your email"), NULL,
						FALSE, FALSE, "Steam Guard Code", _("OK"),
						G_CALLBACK(steam_set_steam_guard_token_cb), _("Cancel"),
						NULL, sa->account, NULL, NULL, sa->account);
			purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, error_description);
		} else if (g_str_equal(x_errorcode, "incorrect_login"))
		{
			purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, error_description);
		} else {
			purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_description);
		}
	}
}

static void
steam_login(PurpleAccount *account)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	SteamAccount *sa = g_new0(SteamAccount, 1);
	
	pc->proto_data = sa;
	
	if (!purple_ssl_is_supported()) {
		purple_connection_error (pc,
								PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
								_("Server requires TLS/SSL for login.  No TLS/SSL support found."));
		return;
	}
	
	sa->account = account;
	sa->pc = pc;
	sa->cookie_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	sa->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	sa->sent_messages_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	sa->waiting_conns = g_queue_new();

	
	if (purple_account_get_string(account, "access_token", NULL))
	{
		steam_login_with_access_token(sa);
	} else
	{
		GString *post = g_string_new(NULL);
		g_string_append(post, "client_id=3638BFB1&");
		g_string_append(post, "grant_type=password&");
		g_string_append_printf(post, "username=%s&", purple_url_encode(account->username));
		g_string_append_printf(post, "password=%s&", purple_url_encode(account->password));
		g_string_append_printf(post, "x_emailauthcode=%s&", purple_url_encode(purple_account_get_string(account, "steam_guard_code", "")));
		g_string_append(post, "x_webcookie=&");
		g_string_append(post, "scope=read_profile write_profile read_client write_client");
		steam_post_or_get(sa, STEAM_METHOD_POST | STEAM_METHOD_SSL, NULL, "/ISteamOAuth2/GetTokenWithCredentials/v0001", post->str, steam_login_cb, NULL, TRUE);
		g_string_free(post, TRUE);
	}
	
	purple_connection_set_state(pc, PURPLE_CONNECTING);
	purple_connection_update_progress(pc, _("Connecting"), 1, 3);
}

static void steam_close(PurpleConnection *pc)
{
	SteamAccount *sa;
	
	g_return_if_fail(pc != NULL);
	g_return_if_fail(pc->proto_data != NULL);
	
	sa = pc->proto_data;
	
	purple_timeout_remove(sa->poll_timeout);
	
	purple_debug_info("steam", "destroying %d waiting connections\n",
					  g_queue_get_length(sa->waiting_conns));
	
	while (!g_queue_is_empty(sa->waiting_conns))
		steam_connection_destroy(g_queue_pop_tail(sa->waiting_conns));
	g_queue_free(sa->waiting_conns);
	
	purple_debug_info("steam", "destroying %d incomplete connections\n",
			g_slist_length(sa->conns));

	while (sa->conns != NULL)
		steam_connection_destroy(sa->conns->data);

	while (sa->dns_queries != NULL) {
		PurpleDnsQueryData *dns_query = sa->dns_queries->data;
		purple_debug_info("steam", "canceling dns query for %s\n",
					purple_dnsquery_get_host(dns_query));
		sa->dns_queries = g_slist_remove(sa->dns_queries, dns_query);
		purple_dnsquery_destroy(dns_query);
	}
	
	g_hash_table_destroy(sa->sent_messages_hash);
	g_hash_table_destroy(sa->cookie_table);
	g_hash_table_destroy(sa->hostname_ip_cache);
	
	g_free(sa->umqid);
	g_free(sa);
}

static unsigned int
steam_send_typing(PurpleConnection *pc, const gchar *name, PurpleTypingState state)
{
	SteamAccount *sa = pc->proto_data;
	PurpleAccount *account = sa->account;
	if (state == PURPLE_TYPING)
	{
		GString *post = g_string_new(NULL);
		
		g_string_append_printf(post, "access_token=%s&", purple_url_encode(purple_account_get_string(account, "access_token", "")));
		g_string_append_printf(post, "umqid=%s&", purple_url_encode(sa->umqid));
		g_string_append(post, "type=typing&");
		g_string_append_printf(post, "steamid_dst=%s", name);
		
		steam_post_or_get(sa, STEAM_METHOD_POST | STEAM_METHOD_SSL, NULL, "/ISteamWebUserPresenceOAuth/Message/v0001", post->str, NULL, NULL, TRUE);
		
		g_string_free(post, TRUE);
	}
	
	return 20;
}

static void
steam_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	SteamAccount *sa = pc->proto_data;
	PurpleStatusPrimitive prim = purple_status_type_get_primitive(purple_status_get_type(status));
	guint state_id;
	GString *post = NULL;
	
	switch(prim)
	{
		default:
		case PURPLE_STATUS_OFFLINE:
			state_id = 0;
			break;
		case PURPLE_STATUS_AVAILABLE:
			state_id = 1;
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			state_id = 2;
			break;
		case PURPLE_STATUS_AWAY:
			state_id = 3;
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			state_id = 4;
			break;
	}
	
	post = g_string_new(NULL);
	
	g_string_append_printf(post, "access_token=%s&", purple_url_encode(purple_account_get_string(account, "access_token", "")));
	g_string_append_printf(post, "umqid=%s&", purple_url_encode(sa->umqid));
	g_string_append(post, "type=personastate&");
	g_string_append_printf(post, "persona_state=%u", state_id);
	
	steam_post_or_get(sa, STEAM_METHOD_POST | STEAM_METHOD_SSL, NULL, "/ISteamWebUserPresenceOAuth/Message/v0001", post->str, NULL, NULL, TRUE);
	
	g_string_free(post, TRUE);
}

static void
steam_set_idle(PurpleConnection *pc, int time)
{
	SteamAccount *sa = pc->proto_data;
	sa->idletime = time;
}

static gint steam_send_im(PurpleConnection *pc, const gchar *who, const gchar *msg,
		PurpleMessageFlags flags)
{
	SteamAccount *sa = pc->proto_data;
	PurpleAccount *account = sa->account;
	GString *post = g_string_new(NULL);
	gchar *stripped;
	
	g_string_append_printf(post, "access_token=%s&", purple_url_encode(purple_account_get_string(account, "access_token", "")));
	g_string_append_printf(post, "umqid=%s&", purple_url_encode(sa->umqid));
	
	stripped = purple_markup_strip_html(msg);
	if (purple_message_meify(stripped, -1))
	{
		g_string_append(post, "type=emote&");
	} else {
		g_string_append(post, "type=saytext&");
	}
	g_string_append_printf(post, "text=%s&", purple_url_encode(stripped));
	g_string_append_printf(post, "steamid_dst=%s", who);
	
	steam_post_or_get(sa, STEAM_METHOD_POST | STEAM_METHOD_SSL, NULL, "/ISteamWebUserPresenceOAuth/Message/v0001", post->str, NULL, NULL, TRUE);
	
	g_string_free(post, TRUE);
	g_free(stripped);
	
	return 1;
}

static void steam_buddy_free(PurpleBuddy *buddy)
{
	SteamBuddy *sbuddy = buddy->proto_data;
	if (sbuddy != NULL)
	{
		buddy->proto_data = NULL;

		g_free(sbuddy->steamid);
		g_free(sbuddy->personaname);
		g_free(sbuddy->realname);
		g_free(sbuddy->profileurl);
		g_free(sbuddy->avatar);
		g_free(sbuddy->gameid);
		g_free(sbuddy->gameextrainfo);
		g_free(sbuddy->gameserversteamid);
		g_free(sbuddy->lobbysteamid);
		
		g_free(sbuddy);
	}
}

void
steam_fake_group_buddy(PurpleConnection *pc, const char *who, const char *old_group, const char *new_group)
{
	// Do nothing to stop the remove+add behaviour
}

void
#if PURPLE_VERSION_CHECK(3, 0, 0)
steam_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char* message)
#else
steam_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
#endif
{
	SteamAccount *sa = pc->proto_data;
	
	if (g_ascii_strtoull(buddy->name, NULL, 10))
	{
		steam_friend_action(sa, buddy->name, "add");
	} else {
		purple_blist_remove_buddy(buddy);
		purple_notify_warning(pc, "Invalid friend id", "Invalid friend id", "Friend ID's must be numeric.\nTry searching from the account menu.");
	}
}

void
steam_buddy_remove(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	SteamAccount *sa = pc->proto_data;
	
	steam_friend_action(sa, buddy->name, "remove");
}

/******************************************************************************/
/* Plugin functions */
/******************************************************************************/

static gboolean plugin_load(PurplePlugin *plugin)
{
	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static GList *steam_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Search for friends..."),
			steam_search_users);
	m = g_list_append(m, act);

	return m;
}

void
steam_blist_view_profile(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	SteamBuddy *sbuddy;
	PurplePlugin *handle = purple_find_prpl(STEAM_PLUGIN_ID);
	
	if(!PURPLE_BLIST_NODE_IS_BUDDY(node))
		return;
	buddy = (PurpleBuddy *) node;
	if (!buddy)
		return;
	sbuddy = buddy->proto_data;
	if (!sbuddy || !sbuddy->profileurl)
		return;
	
	purple_notify_uri(handle, sbuddy->profileurl);
}

static GList *
steam_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	PurpleBuddy *buddy;
	
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		buddy = (PurpleBuddy *)node;
		
		act = purple_menu_action_new("View online Profile",
				PURPLE_CALLBACK(steam_blist_view_profile),
				NULL, NULL);
		m = g_list_append(m, act);
	}
	return m;
}

static void plugin_init(PurplePlugin *plugin)
{
	PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;

	option = purple_account_option_string_new(
		_("Steam Guard Code"),
		"steam_guard_code", "");
	prpl_info->protocol_options = g_list_append(
		prpl_info->protocol_options, option);
}

static PurplePluginProtocolInfo prpl_info = {
#if PURPLE_VERSION_CHECK(3, 0, 0)
	sizeof(PurplePluginProtocolInfo),	/* struct_size */
#endif

	/* options */
	OPT_PROTO_MAIL_CHECK,

	NULL,                   /* user_splits */
	NULL,                   /* protocol_options */
	/* NO_BUDDY_ICONS */    /* icon_spec */
	{"png,jpeg", 0, 0, 64, 64, 0, PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
	steam_list_icon,           /* list_icon */
	steam_list_emblem,         /* list_emblems */
	steam_status_text,         /* status_text */
	steam_tooltip_text,        /* tooltip_text */
	steam_status_types,        /* status_types */
	steam_node_menu,           /* blist_node_menu */
	NULL,//steam_chat_info,           /* chat_info */
	NULL,//steam_chat_info_defaults,  /* chat_info_defaults */
	steam_login,               /* login */
	steam_close,               /* close */
	steam_send_im,             /* send_im */
	NULL,                      /* set_info */
	steam_send_typing,         /* send_typing */
	NULL,//steam_get_info,            /* get_info */
	steam_set_status,          /* set_status */
	steam_set_idle,            /* set_idle */
	NULL,                   /* change_passwd */
	steam_add_buddy,           /* add_buddy */
	NULL,                   /* add_buddies */
	steam_buddy_remove,        /* remove_buddy */
	NULL,                   /* remove_buddies */
	NULL,                   /* add_permit */
	NULL,                   /* add_deny */
	NULL,                   /* rem_permit */
	NULL,                   /* rem_deny */
	NULL,                   /* set_permit_deny */
	NULL,//steam_fake_join_chat,      /* join_chat */
	NULL,                   /* reject chat invite */
	NULL,//steam_get_chat_name,       /* get_chat_name */
	NULL,                   /* chat_invite */
	NULL,//steam_chat_fake_leave,     /* chat_leave */
	NULL,                   /* chat_whisper */
	NULL,//steam_chat_send,           /* chat_send */
	NULL,                   /* keepalive */
	NULL,                   /* register_user */
	NULL,                   /* get_cb_info */
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	NULL,                   /* get_cb_away */
#endif
	NULL,                   /* alias_buddy */
	steam_fake_group_buddy,    /* group_buddy */
	NULL,//steam_group_rename,        /* rename_group */
	steam_buddy_free,          /* buddy_free */
	NULL,//steam_conversation_closed, /* convo_closed */
	purple_normalize_nocase,/* normalize */
	NULL,                   /* set_buddy_icon */
	NULL,//steam_group_remove,        /* remove_group */
	NULL,                   /* get_cb_real_name */
	NULL,                   /* set_chat_topic */
	NULL,                   /* find_blist_chat */
	NULL,                   /* roomlist_get_list */
	NULL,                   /* roomlist_cancel */
	NULL,                   /* roomlist_expand_category */
	NULL,                   /* can_receive_file */
	NULL,                   /* send_file */
	NULL,                   /* new_xfer */
	NULL,                   /* offline_message */
	NULL,                   /* whiteboard_prpl_ops */
	NULL,                   /* send_raw */
	NULL,                   /* roomlist_room_serialize */
	NULL,                   /* unregister_user */
	NULL,                   /* send_attention */
	NULL,                   /* attention_types */
#if (PURPLE_MAJOR_VERSION == 2 && PURPLE_MINOR_VERSION >= 5) || PURPLE_MAJOR_VERSION > 2
#if PURPLE_MAJOR_VERSION == 2 && PURPLE_MINOR_VERSION >= 5
	sizeof(PurplePluginProtocolInfo), /* struct_size */
#endif
	NULL, /* steam_get_account_text_table, /* get_account_text_table */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
#else
	(gpointer) sizeof(PurplePluginProtocolInfo)
#endif
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,				/* major_version */
	PURPLE_MINOR_VERSION, 				/* minor version */
	PURPLE_PLUGIN_PROTOCOL, 			/* type */
	NULL, 						/* ui_requirement */
	0, 						/* flags */
	NULL, 						/* dependencies */
	PURPLE_PRIORITY_DEFAULT, 			/* priority */
	STEAM_PLUGIN_ID,				/* id */
	"Steam", 					/* name */
	STEAM_PLUGIN_VERSION, 			/* version */
	N_("Steam Protocol Plugin"), 		/* summary */
	N_("Steam Protocol Plugin"), 		/* description */
	"Eion Robb <eionrobb@gmail.com>", 		/* author */
	"http://pidgin-opensteamworks.googlecode.com/",	/* homepage */
	plugin_load, 					/* load */
	plugin_unload, 					/* unload */
	NULL, 						/* destroy */
	NULL, 						/* ui_info */
	&prpl_info, 					/* extra_info */
	NULL, 						/* prefs_info */
	steam_actions, 					/* actions */

							/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(steam, plugin_init, info);
