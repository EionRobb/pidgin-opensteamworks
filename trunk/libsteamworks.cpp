#include "libsteamworks.h"

typedef struct {
	HSteamPipe pipe;
	HSteamUser user;
	ISteamClient008 *client;
	ISteamFriends002 *friends;
	ISteamUtils002 *utils;
} SteamInfo;

CSteamID
steamworks_name_to_sid(const gchar *name)
{
	purple_debug_info("steam", "name_to_sid %s\n", name);
	guint64 accountkey;
	CSteamID steamID;
	
	accountkey = g_ascii_strtoull(name, NULL, 10);
	
	steamID = CSteamID::CSteamID(accountkey);
	
	return steamID;
}

gchar *
steamworks_sid_to_name(CSteamID steamID)
{
	purple_debug_info("steam", "sid_to_name\n");
	guint64 accountkey;
	gchar *id;

	accountkey = steamID.GetStaticAccountKey();
	id = g_strdup_printf("%" G_GUINT64_FORMAT, accountkey);
	
	return id;
}

gchar *
steamworks_status_text(PurpleBuddy *buddy)
{
	purple_debug_info("steam", "status_text\n");
	PurpleConnection *pc = purple_account_get_connection(buddy->account);
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	CSteamID steamID = steamworks_name_to_sid(buddy->name);

	//Get game info to use for status message
	FriendGameInfo_t friendinfo;
	guint64 gameid;
	gchar *appname = NULL;
	if (steam->friends->GetFriendGamePlayed( steamID, &gameid, NULL, NULL, NULL ))
	{
		TSteamApp app;
		TSteamError error;
		//SteamEnumerateApp(gameid, &app, &error);
		//appname = g_strdup(app.szName);
	}
	
	return appname;
}

const gchar *
steamworks_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	purple_debug_info("steam", "list_icon\n");
	return "steam";
}

GList *
steamworks_status_types(PurpleAccount *account)
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

guint
steamworks_send_typing(PurpleConnection *pc, const char *who, PurpleTypingState state)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	CSteamID steamID = steamworks_name_to_sid(who);
	
	purple_debug_info("steam", "send_typing\n");
	if (state == PURPLE_TYPING)
	{
		steam->friends->SendMsgToFriend(steamID, k_EChatEntryTypeTyping, "", 0);
		return 999;
	}
	
	return 0;
}

void
steamworks_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	EPersonaState state;
	PurpleStatusPrimitive prim;
	
	purple_debug_info("steam", "set_status\n");
	if (!steam)
		return;
	
	prim = purple_status_type_get_primitive(purple_status_get_type(status));

	switch(prim)
	{
		default:
		case PURPLE_STATUS_OFFLINE:
			state = k_EPersonaStateOffline;
			break;
		case PURPLE_STATUS_AVAILABLE:
			state = k_EPersonaStateOnline;
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			state = k_EPersonaStateBusy;
			break;
		case PURPLE_STATUS_AWAY:
			state = k_EPersonaStateAway;
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			state = k_EPersonaStateSnooze;
			break;
	}
	
	steam->friends->SetPersonaState(state);
}

gint
steamworks_send_im(PurpleConnection *pc, const char *who, const char *message, PurpleMessageFlags flags)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	gchar *stripped;
	CSteamID steamID = steamworks_name_to_sid(who);
	gboolean success;
	
	purple_debug_info("steam", "send_im\n");
	stripped = purple_markup_strip_html(message);
	
	if (message[0] == '/' &&
		message[1] == 'm' &&
		message[2] == 'e' &&
		message[3] == ' ')
	{
		success = steam->friends->SendMsgToFriend(steamID, k_EChatEntryTypeEmote, &stripped[4], strlen(stripped)-4);
	} else {
		success = steam->friends->SendMsgToFriend(steamID, k_EChatEntryTypeChatMsg, stripped, strlen(stripped));
	}
	
	g_free(stripped);
	return success;
}

void
steamworks_close(PurpleConnection *pc)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	
	purple_debug_info("steam", "close\n");
	if (!steam)
		return;
	
	//if (steam->client && steam->pipe)
	//{
	//	if (steam->user)
	//		steam->client->ReleaseUser(steam->pipe, steam->user);
	//	steam->client->ReleaseSteamPipe(steam->pipe);
	//}
	
	g_free(steam);
}

void
steamworks_login(PurpleAccount *account)
{
	PurpleConnection *pc;
	SteamInfo *steam;
	
	purple_debug_info("steam", "login\n");
	
	pc = purple_account_get_connection(account);
	
	steam = g_new0(SteamInfo, 1);
	pc->proto_data = steam;

	CSteamAPILoader loader;
	CreateInterfaceFn factory = loader.Load();
	steam->client = (ISteamClient008 *)factory( STEAMCLIENT_INTERFACE_VERSION_008, NULL );
	if (!steam->client)
	{
		purple_connection_error(pc, "Could not load client\n");
		return;
	}
	steam->pipe = steam->client->CreateSteamPipe();
	if (!steam->pipe)
	{
		purple_connection_error(pc, "Could not create pipe\n");
		return;
	}
	steam->user = steam->client->ConnectToGlobalUser( steam->pipe );
	if (!steam->user)
	{
		purple_connection_error(pc, "Could not load user\n");
		return;
	}
	
	steam->friends = (ISteamFriends002 *)steam->client->GetISteamFriends(steam->user, steam->pipe, STEAMFRIENDS_INTERFACE_VERSION_002);
	if (!steam->friends)
	{
		purple_connection_error(pc, "Could not load friends\n");
		return;
	}
	steam->utils = (ISteamUtils002 *)steam->client->GetISteamUtils(steam->pipe, STEAMUTILS_INTERFACE_VERSION_002);
	if (!steam->utils)
	{
		purple_connection_error(pc, "Could not load utils\n");
		return;
	}

	PurpleGroup *group;
	group = purple_find_group("Steam");
	if (!group)
	{
		group = purple_group_new("Steam");
	}
	
	int count = steam->friends->GetFriendCount(k_EFriendFlagImmediate);
	for(int i=0; i<count; i++)
	{
		purple_debug_info("steam", "Friend number %d\n", i);
		
		CSteamID steamID = steam->friends->GetFriendByIndex(i, k_EFriendFlagImmediate);
		gchar *id = steamworks_sid_to_name(steamID);
		purple_debug_info("steam", "Friend id %s\n", id);
		const gchar *alias = steam->friends->GetFriendPersonaName( steamID );
		purple_debug_info("steam", "Friend alias %s\n", alias);
		
		PurpleBuddy *buddy;
		buddy = purple_find_buddy(account, id);
		if (!buddy)
		{
			buddy = purple_buddy_new(account, id, alias);
			purple_blist_add_buddy(buddy, NULL, group, NULL);
		}
		
		EPersonaState state = steam->friends->GetFriendPersonaState(steamID);
		purple_debug_info("steam", "Friend state %d\n", state);
		const char *status_id;
		PurpleStatusPrimitive prim;
		switch(state)
		{
			default:
			case k_EPersonaStateOffline:
				prim = PURPLE_STATUS_OFFLINE; 
				break;
			case k_EPersonaStateOnline:
				prim = PURPLE_STATUS_AVAILABLE;
				break;
			case k_EPersonaStateBusy:
				prim = PURPLE_STATUS_UNAVAILABLE;
				break;
			case k_EPersonaStateAway:
				prim = PURPLE_STATUS_AWAY;
				break;
			case k_EPersonaStateSnooze:
				prim = PURPLE_STATUS_EXTENDED_AWAY;
				break;
		}
		status_id = purple_primitive_get_id_from_type(prim);
		purple_prpl_got_user_status(account, id, status_id, NULL);
		
		/*
		//Need at least ISteamFriends003 for this to work
		int avatar_handle = steam->friends->GetFriendAvatar(steamID, 1);
		uint32 avatar_width, avatar_height;
		if (steam->utils->GetImageSize(avatar_handle, &avatar_width, &avatar_height))
		{
			int buffer_size = (4 * avatar_width * avatar_height);
			guchar *image = g_new(guchar, buffer_size);
			if (steam->utils->GetImageRGBA(avatar_handle, image, buffer_size))
			{
				//add image into pidgin
				purple_buddy_icons_set_for_user(account, id, image, buffer_size, NULL);
			} else {
				g_free(image);
			}
		}*/
		
		g_free(id);
	}
}
