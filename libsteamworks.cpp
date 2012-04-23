#define STEAMWORKS_CLIENT_INTERFACES

#include "libsteamworks.h"

//#pragma comment(lib, "opensteamworks/steamclient")
typedef bool (*Steam_BGetCallbackFn)( HSteamPipe hSteamPipe, CallbackMsg_t *pCallbackMsg );
typedef void (*Steam_FreeLastCallbackFn)( HSteamPipe hSteamPipe );
typedef int (*SteamEnumerateAppFn)( unsigned int uAppId, TSteamApp *pApp, TSteamError *pError );
static Steam_BGetCallbackFn BGetCallback = NULL;
static Steam_FreeLastCallbackFn FreeLastCallback = NULL;
static SteamEnumerateAppFn SteamEnumApp = NULL;

#undef g_new0
#define g_new0(struct_type, n_structs)		\
    ((struct_type *) g_malloc0 (((gsize) sizeof (struct_type)) * ((gsize) (n_structs))))

typedef struct {
	CSteamAPILoader *loader;
	CreateInterfaceFn factory;

	HSteamPipe pipe;
	HSteamUser user;

	ISteamClient008 *client;
	ISteamFriends002 *friends;
	ISteamUtils002 *utils;
	ISteamUser005 *suser;
	IClientFriends *clientfriends;
	ISteamApps001 *apps;
	ISteamMatchmaking008 *matchmaking;

	guint event_timer;
	GHashTable *sent_messages_table;
} SteamInfo;

/*const CSteamID 
steamworks_name_to_sid(const gchar *name)
{
	static CSteamID *sid;
	guint64 accountkey = g_ascii_strtoull(name, NULL, 10);
	
	sid = new CSteamID(accountkey);
	
	return *sid;
}

const gchar *
steamworks_sid_to_name(CSteamID steamID)
{
	static gchar *id = NULL;

	g_free(id);
	id = g_strdup_printf("%" G_GUINT64_FORMAT, steamID.ConvertToUint64());
	
	return id;
}*/

const CSteamID 
steamworks_name_to_sid(const gchar *name)
{       
	guint32 accountkey = atol(name);

	return CSteamID(accountkey, 1, k_EUniversePublic, k_EAccountTypeIndividual);
}

const gchar *
steamworks_sid_to_name(CSteamID steamID)
{
	//purple_debug_info("steam", "sid_to_name\n");
	guint32 accountkey;
	static gchar *id = NULL;
	
	g_free(id);
	accountkey = steamID.GetAccountID();
	id = g_strdup_printf("%" G_GUINT32_FORMAT, accountkey);
	
	return id;
}

const guint64
steamworks_get_friend_gameid(SteamInfo *steam, CSteamID user)
{
	FriendGameInfo_t gameinfo;
	
	if (steam && steam->clientfriends && steam->clientfriends->GetFriendGamePlayed(user, &gameinfo))
		return gameinfo.m_gameID.ToUint64();
	
	return 0;
}

gboolean
steamworks_load_avatar_for_user(PurpleAccount *account, CSteamID steamID)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	SteamInfo *steam = (SteamInfo *) pc->proto_data;
	gboolean success = FALSE;
	uint32 avatar_width, avatar_height;

	int avatar_handle;
	
	if (steam->clientfriends)
		avatar_handle = steam->clientfriends->GetLargeFriendAvatar(steamID);
	else
	{
		//Need at least ISteamFriends004 for avatars to work
		ISteamFriends007 *friends = (ISteamFriends007 *)steam->client->GetISteamFriends(steam->user, steam->pipe, STEAMFRIENDS_INTERFACE_VERSION_007);
		//avatar_handle = friends->GetFriendAvatar(steamID, 1);
		avatar_handle = friends->GetLargeFriendAvatar(steamID);
	}

	if (steam->utils->GetImageSize(avatar_handle, &avatar_width, &avatar_height))
	{
		//Set a TGA file header for a 32bpp BGRA image
		guchar tgaheader[18] = {0,0,2, 0,0,0,0,0, 0,0,0,0,
			avatar_width&0xFF,(avatar_width&0xFF00)/256,
			avatar_height&0xFF,(avatar_height&0xFF00)/256,
			32, 32};

		int buffer_size = (4 * avatar_width * avatar_height);
		purple_debug_info("steam", "avatar_size %d x %d\n", avatar_width, avatar_height);
		guchar *image = g_new0(guchar, buffer_size);
		if (steam->utils->GetImageRGBA(avatar_handle, image, buffer_size))
		{
			//User has image, add into Pidgin
			guchar temp;
			for(int i=0; i<buffer_size; i+=4)
			{
				// Swap RGBA -> BGRA
				temp = image[i+0]; image[i+0] = image[i+2]; image[i+2] = temp;
			}
			guchar *tgaimage = g_new0(guchar, buffer_size+18);
			memcpy(tgaimage, tgaheader, 18);
			memcpy(&tgaimage[18], image, buffer_size);
			purple_buddy_icons_set_for_user(account, steamworks_sid_to_name(steamID), tgaimage, buffer_size+18, NULL);
			success = TRUE;
		}
		g_free(image);
	}

	return success;
}

const gchar *
steamworks_personastate_to_statustype(const EPersonaState state)
{
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
	return status_id;
}

#ifndef memmem
#ifndef _LIBC
# define __builtin_expect(expr, val)   (expr)
#endif
/* Return the first occurrence of NEEDLE in HAYSTACK.  */
void *
memmem (const void *haystack, size_t haystack_len, const void *needle, size_t needle_len)
{
  const char *begin;
  const char *const last_possible
    = (const char *) haystack + haystack_len - needle_len;

  if (needle_len == 0)
    /* The first occurrence of the empty string is deemed to occur at
       the beginning of the string.  */
    return (void *) haystack;

  /* Sanity check, otherwise the loop might search through the whole
     memory.  */
  if (__builtin_expect (haystack_len < needle_len, 0))
    return NULL;

  for (begin = (const char *) haystack; begin <= last_possible; ++begin)
    if (begin[0] == ((const char *) needle)[0] &&
	!memcmp ((const void *) &begin[1],
		 (const void *) ((const char *) needle + 1),
		 needle_len - 1))
      return (void *) begin;

  return NULL;
}
#endif

gchar *
steamworks_gameid_to_gamename(SteamInfo *steam, guint64 gameid)
{
	gchar *name = NULL;
	static gsize appinfo_len = 0;
	static gchar *appinfo = NULL;
	gchar *appinfo_filename;
	gboolean success;
	
	purple_debug_info("steam", "Finding game name for %" G_GUINT64_FORMAT "\n", gameid);

	if (steam->apps)
	{
		name = g_new0(gchar, 256);
		if (steam->apps->GetAppData((AppId_t) gameid, "name", name, 256))
			return name;
		g_free(name);
		name = NULL;
	}

	if (!appinfo)
	{
		purple_debug_info("steam", "One-off loading appinfo.vdf\n");
		// steamdir/appcache/appinfo.vdf
		appinfo_filename = g_strconcat(steam->loader->GetSteamDir(),
			G_DIR_SEPARATOR_S "appcache" G_DIR_SEPARATOR_S "appinfo.vdf", NULL);
		success = g_file_get_contents(appinfo_filename, &appinfo, &appinfo_len, NULL);
		g_free(appinfo_filename);
		if (!success)
			return NULL;
	}
	
	purple_debug_info("steam", "Searching appinfo.vdf\n");

	// Find:
	// \1gameid\0(id)\0
	gchar *gameid_str = g_strdup_printf("%" G_GUINT64_FORMAT, gameid);
	gchar *gameid_search = g_strdup_printf("\1gameid\1%s\0", gameid_str);
	gameid_search[7] = '\0';
	gchar *search_temp = (gchar *)memmem(appinfo, appinfo_len, gameid_search, strlen(gameid_str)+9);
	g_free(gameid_search);
	g_free(gameid_str);

	if (search_temp)
	{
		// Go to the start of the line
		while(search_temp != appinfo && *search_temp != '\2')
			search_temp--;

		// Find \1name\0(gamename)\0
		search_temp = (gchar *)memmem(search_temp, appinfo_len-(search_temp-appinfo), "\1name\0", 6);
		if (search_temp)
		{
			name = g_strdup(&search_temp[6]);
		}
	}

	return name;
}

gboolean
steamworks_eventloop(gpointer userdata)
{
	PurpleConnection *pc = (PurpleConnection *) userdata;
	SteamInfo *steam = (SteamInfo *) pc->proto_data;
	CallbackMsg_t CallbackMsg;

	// Download all waiting events
	while (BGetCallback(steam->pipe, &CallbackMsg))
	{
		purple_debug_info("steam", "Recieved event type %d\n", CallbackMsg.m_iCallback);
		switch(CallbackMsg.m_iCallback)
		{
		case FriendChatMsg_t::k_iCallback: // event type 306
		//case 805: // k_iClientFriendsCallbacks + 5
		{
			PurpleMessageFlags msgflags = PURPLE_MESSAGE_RECV;
			
			purple_debug_info("steam", "received message event\n");
			//message or typing notification
			FriendChatMsg_t *chatMsg = (FriendChatMsg_t *)CallbackMsg.m_pubParam;

			EChatEntryType msgType;
			gint msgLen;
			gint messageSize = 255;
			gchar *message = g_new0(gchar, messageSize + 1);
			CSteamID chatFriend = chatMsg->m_ulSenderID;
			if (chatMsg->m_ulSenderID == steam->suser->GetSteamID())
			{
				chatFriend = chatMsg->m_ulFriendID;
				msgflags = PURPLE_MESSAGE_SEND;
			}
			
			while((msgLen = steam->friends->GetChatMessage(chatFriend, chatMsg->m_iChatID, message, messageSize, &msgType)) >= messageSize)
			{
				//purple_debug_info("steam", "message has new length of %d\n", msgLen);
				if (msgLen > messageSize)
					messageSize = msgLen + 1;
				else
					messageSize *= 2;
				g_free(message);
				message = g_new0(gchar, messageSize + 1);
			}
			
			//Check that the message didn't come from ourselves
			if (chatMsg->m_ulSenderID == steam->suser->GetSteamID() && g_hash_table_remove(steam->sent_messages_table, message))
			{
				break;
			}

			if (msgType & k_EChatEntryTypeTyping)
			{
				serv_got_typing(pc, steamworks_sid_to_name(chatFriend), 20, PURPLE_TYPING);
			} else if ((msgType & k_EChatEntryTypeChatMsg) || (!msgType && message && *message)) {
				gchar *html = purple_strdup_withhtml(message);
				serv_got_im(pc, steamworks_sid_to_name(chatFriend), html, msgflags, time(NULL));
				g_free(html);
			} else if (msgType & k_EChatEntryTypeEmote) {
				gchar *emote = g_strconcat("/me ", message, NULL);
				gchar *html = purple_strdup_withhtml(emote);
				serv_got_im(pc, steamworks_sid_to_name(chatFriend), html, msgflags, time(NULL));
				g_free(html);
				g_free(emote);
			} else if (msgType & k_EChatEntryTypeInviteGame) {
				purple_debug_warning("steam", "invite to game message\n");
				purple_debug_info("steam", "invite message '%s'\n", message);
			} else {
				purple_debug_info("steam", "unknown msgType %d\n", msgType);
			}
			g_free(message);
		}	break;
		case PersonaStateChange_t::k_iCallback:
		{
			purple_debug_info("steam", "received statechange event\n");
			//state changed
			PersonaStateChange_t *state = (PersonaStateChange_t *)CallbackMsg.m_pubParam;
			if (state->m_nChangeFlags & k_EPersonaChangeName)
			{
				const gchar *alias = steam->friends->GetFriendPersonaName( state->m_ulSteamID );
				serv_got_alias(pc, steamworks_sid_to_name(state->m_ulSteamID), alias);
			}
			if (state->m_nChangeFlags & (k_EPersonaChangeStatus | k_EPersonaChangeGamePlayed))
			{
				EPersonaState pstate = steam->friends->GetFriendPersonaState(state->m_ulSteamID);
				const gchar *status_id = steamworks_personastate_to_statustype(pstate);
				purple_prpl_got_user_status(pc->account, steamworks_sid_to_name(state->m_ulSteamID), status_id, NULL);
			}
			if (state->m_nChangeFlags & k_EPersonaChangeAvatar)
			{
				steamworks_load_avatar_for_user(pc->account, state->m_ulSteamID);
			}
			purple_debug_info("steam", "user %d changed state %d\n", state->m_ulSteamID, state->m_nChangeFlags);
		}	break;
		case UserRequestingFriendship_t::k_iCallback: // 302
		//case 804: //k_iClientFriendsCallbacks + 4
		{
			purple_debug_info("steam", "received friendrequest event\n");
			//requesting friendship
			UserRequestingFriendship_t *request = (UserRequestingFriendship_t *)CallbackMsg.m_pubParam;
			purple_debug_info("steam", "user %d requested auth\n", request->k_iCallback);
			const gchar *username = steamworks_sid_to_name(request->m_ulSteamID);
			const gchar *alias = steam->friends->GetFriendPersonaName(request->m_ulSteamID);
			purple_account_request_add(pc->account, username, NULL, alias, NULL);
		}	break;
		case FriendAdded_t::k_iCallback: // event type 301
		//case 803: //k_iClientFriendsCallbacks + 3
		{
			purple_debug_info("steam", "received friendadded event\n");
			//friend added to buddylist
			FriendAdded_t *friendadd = (FriendAdded_t *)CallbackMsg.m_pubParam;
			if(friendadd->m_eResult == k_EResultOK)
			{
				const gchar *username = steamworks_sid_to_name(friendadd->m_ulSteamID);
				PurpleBuddy *buddy = purple_find_buddy(pc->account, username);
				const gchar *alias = steam->friends->GetFriendPersonaName(friendadd->m_ulSteamID);
				if (!buddy)
				{
					buddy = purple_buddy_new(pc->account, username, NULL);
					purple_blist_add_buddy(buddy, NULL, purple_find_group("Steam"), NULL);
				}
				serv_got_alias(pc, username, alias);
				buddy->proto_data = &(friendadd->m_ulSteamID);
				EPersonaState state = steam->friends->GetFriendPersonaState(friendadd->m_ulSteamID);
				purple_debug_info("steam", "Friend state %d ", state);
				const gchar *status_id = steamworks_personastate_to_statustype(state);
				purple_debug_info("steam", "status_id %s\n", status_id);
				purple_prpl_got_user_status(pc->account, username, status_id, NULL);
			}
		}	break;
		case LobbyInvite_t::k_iCallback:
		{
			purple_debug_info("steam", "received gameinvite event\n");
			//steam://joinlobby/630/109775240975659434/76561198011361273
			//steam://joinlobby/gameid/lobbyid/friendid
			LobbyInvite_t *invite = (LobbyInvite_t *)CallbackMsg.m_pubParam;
			CSteamID user = invite->m_ulSteamIDUser;
			CSteamID lobby = invite->m_ulSteamIDLobby;
			guint64 gameid = steamworks_get_friend_gameid(steam, user);
			gchar *message = g_strdup_printf("/me has invited you to <a href=\"steam://joinlobby/%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT "\">join their game</a>",
				gameid, lobby, user);
			serv_got_im(pc, steamworks_sid_to_name(user), message, (PurpleMessageFlags)(PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM), time(NULL));
			g_free(message);
		}	break;
		case ChatRoomInvite_t::k_iCallback: // event type 308
		//case 807: //k_iClientFriendsCallbacks + 7
		{
			purple_debug_info("steam", "received chatroominvite event\n");
			// invited to chat room
			ChatRoomInvite_t *invite = (ChatRoomInvite_t *)CallbackMsg.m_pubParam;
			if(invite->m_EChatRoomType == k_EChatRoomTypeMUC)
			{
				purple_debug_info("steam", "%s\n", invite->m_ulSteamIDChat.SteamRender());
				gchar *chatid = g_strdup(steamworks_sid_to_name(invite->m_ulSteamIDChat));
				serv_got_chat_invite(pc, chatid, NULL, NULL, steamworks_chat_defaults(pc, chatid));
				g_free(chatid);
			}
		}	break;
		case ChatRoomMsg_t::k_iCallback: // event type 311
		//case 810: //k_iClientFriendsCallbacks + 10
		{
			purple_debug_info("steam", "received chatroommsg event\n");
			// chat room message
			ChatRoomMsg_t *msg = (ChatRoomMsg_t *)CallbackMsg.m_pubParam;
			EChatEntryType msgType;
			CSteamID steamUser = msg->m_ulSteamIDUser;
			
			gint msgLen;
			gint messageSize = 255;
			gchar *message = g_new0(gchar, messageSize + 1);
			
			purple_debug_info("steam", "test1\n");
			while((msgLen = steam->clientfriends->GetChatRoomEntry(msg->m_ulSteamIDChat, msg->m_iChatID, NULL, message, messageSize, &msgType)) >= messageSize)
			{
				purple_debug_info("steam", "message has new length of %d\n", msgLen);
				if (msgLen > messageSize)
					messageSize = msgLen + 1;
				else
					messageSize *= 2;
				g_free(message);
				message = g_new0(gchar, messageSize + 1);
			}
			purple_debug_info("steam", "test2 %s\n", message);
			
			gint chatid = msg->m_ulSteamIDChat.GetAccountID();
			gchar *friendid = g_strdup(steamworks_sid_to_name(steamUser));
			purple_debug_info("steam", "test3 %d\n", msg->m_eChatEntryType);
			if (msg->m_eChatEntryType & k_EChatEntryTypeChatMsg) {
			purple_debug_info("steam", "test4\n");
				gchar *html = purple_strdup_withhtml(message);
				serv_got_chat_in(pc, chatid, friendid, PURPLE_MESSAGE_RECV, html, time(NULL));
				g_free(html);
			} else if (msg->m_eChatEntryType & k_EChatEntryTypeEmote) {
			purple_debug_info("steam", "test5\n");
				gchar *emote = g_strconcat("/me ", message, NULL);
				gchar *html = purple_strdup_withhtml(emote);
				serv_got_chat_in(pc, chatid, friendid, PURPLE_MESSAGE_RECV, html, time(NULL));
				g_free(html);
				g_free(emote);
			}
			g_free(friendid);
			g_free(message);
		}	break;
		case ChatRoomEnter_t::k_iCallback: // event type 309
		//case 808: //k_iClientFriendsCallbacks + 8
		{
			purple_debug_info("steam", "received chatenter event\n");
			// chat room message
			ChatRoomEnter_t *room = (ChatRoomEnter_t *)CallbackMsg.m_pubParam;
			const gchar *friendid = steamworks_sid_to_name(room->m_ulSteamIDFriendChat);
			serv_got_joined_chat(pc, room->m_ulSteamIDChat.GetAccountID(), friendid);
		} break;
		case AvatarImageLoaded_t::k_iCallback:
		{
			AvatarImageLoaded_t *avatar = (AvatarImageLoaded_t *)CallbackMsg.m_pubParam;
			steamworks_load_avatar_for_user(pc->account, avatar->m_steamID);
		} break;
		default:
			purple_debug_warning("steam", "unhandled event!\n");
			break;
		}
		FreeLastCallback(steam->pipe);
	}

	return TRUE;
}

gchar *
steamworks_status_text(PurpleBuddy *buddy)
{
	//purple_debug_info("steam", "status_text\n");
	PurpleConnection *pc = purple_account_get_connection(buddy->account);
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	CSteamID steamID = steamworks_name_to_sid(buddy->name);

	if (!steam || !steam->friends)
		return NULL;

	//Get game info to use for status message
	guint64 gameid = steamworks_get_friend_gameid(steam, steamID);
	gchar *appname = NULL;
	if (gameid)
	{
		gchar *tempname;
		tempname = steamworks_gameid_to_gamename(steam, gameid);
		if (tempname == NULL)
		{
			tempname = g_strdup_printf("%" G_GUINT64_FORMAT, gameid);
		}
		appname = g_markup_printf_escaped("In game %s", tempname);
		g_free(tempname);
	}
	
	return appname;
}

const gchar *
steamworks_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	//purple_debug_info("steam", "list_icon\n");
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
	
	if (state == PURPLE_TYPING)
	{
		purple_debug_info("steam", "send_typing\n");
		steam->friends->SendMsgToFriend(steamID, k_EChatEntryTypeTyping, "", 0);
	}
	//purple_debug_info("steam", "sent typing\n");
	
	return 20;
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
	
	purple_debug_info("steam", "Setting status to %d\n", state);
	steam->clientfriends->SetPersonaState(state);
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
	
	if (purple_message_meify(stripped, -1))
	{
		purple_debug_info("steam", "sending emote...\n");
		success = steam->friends->SendMsgToFriend(steamID, k_EChatEntryTypeEmote, stripped, (int)strlen(stripped)+1);
	} else {
		purple_debug_info("steam", "sending message...\n");
		success = steam->friends->SendMsgToFriend(steamID, k_EChatEntryTypeChatMsg, stripped, (int)strlen(stripped)+1);
	}
	purple_debug_info("steam", "sent.\n");
	
	g_hash_table_insert(steam->sent_messages_table, g_strdup(stripped), NULL);
	
	g_free(stripped);
	return success;
}


void steamworks_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;

	if (g_ascii_strtoull(buddy->name, NULL, 10))
	{
		//Looks like a valid steam uid
		//Probably from adding buddy when they added us first
		steam->clientfriends->AddFriend(steamworks_name_to_sid(buddy->name));
	} else {
		//Looks like an email address or name
		//Probably from adding buddy through buddy list
		steam->clientfriends->AddFriendByName(buddy->name);
		purple_blist_remove_buddy(buddy);
	}
}

void steamworks_remove_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	
	if (g_ascii_strtoull(buddy->name, NULL, 10))
		steam->clientfriends->RemoveFriend(steamworks_name_to_sid(buddy->name));
}

void
steamworks_alias_buddy(PurpleConnection *pc, const char *who, const char *alias)
{
	CSteamID sid;
	PurpleBuddy *buddy;
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	
	if (!alias)
		alias = "";
	
	buddy = purple_find_buddy(pc->account, who);
	if (buddy && buddy->proto_data)
		sid = *((CSteamID *)buddy->proto_data);
	else
		sid = steamworks_name_to_sid(who);
		
	steam->clientfriends->SetFriendAlias(sid, alias);
}

void
steamworks_ignore_buddy(PurpleConnection *pc, const char *name)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	if (steam && steam->clientfriends)
	{
		steam->clientfriends->SetIgnoreFriend(steamworks_name_to_sid(name), TRUE);
	}
}

void
steamworks_unignore_buddy(PurpleConnection *pc, const char *name)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	if (steam && steam->clientfriends)
	{
		steam->clientfriends->SetIgnoreFriend(steamworks_name_to_sid(name), FALSE);
	}
}

const CSteamID
steamworks_chatid_to_sid(int chatid)
{
	return CSteamID(chatid, 1, k_EUniversePublic, k_EAccountTypeChat);
}

const CSteamID
steamworks_chatname_to_sid(const gchar *chatname)
{
	guint32 accountkey;
	accountkey = atol(chatname);
	return steamworks_chatid_to_sid(accountkey);
}

int
steamworks_chat_send(PurpleConnection *pc, int id, const char *message, PurpleMessageFlags flags)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	gchar *stripped;
	gboolean success;
	CSteamID steamID = steamworks_chatid_to_sid(id);
	
	if (steam && steam->clientfriends)
	{
		purple_debug_info("steam", "chat_send\n");
		stripped = purple_markup_strip_html(message);
		
		if (purple_message_meify(stripped, -1))
		{
			purple_debug_info("steam", "sending emote...\n");
			success = steam->clientfriends->SendChatMsg(steamID, k_EChatEntryTypeEmote, stripped, (int)strlen(stripped)+1);
		} else {
			purple_debug_info("steam", "sending message...\n");
			success = steam->clientfriends->SendChatMsg(steamID, k_EChatEntryTypeChatMsg, stripped, (int)strlen(stripped)+1);
		}

		g_free(stripped);

		return 1;
	}

	return -57;
}

GList *
steamworks_chat_info(PurpleConnection *pc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = "SteamID";
	pce->identifier = "steamID";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	return m;
}

GHashTable *
steamworks_chat_defaults(PurpleConnection *pc, const char *chat_name)
{
	GHashTable *defaults;
	purple_debug_info("steam", "chat_defaults %s\n", chat_name);

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name && *chat_name)
	{
		g_hash_table_insert(defaults, "steamID", g_strdup(chat_name));
	}

	return defaults;
}

void
steamworks_reject_chat(PurpleConnection *pc, GHashTable *components)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	purple_debug_info("steam", "reject_chat\n");

	if (steam && steam->clientfriends && components)
	{
		CSteamID chatId = steamworks_chatname_to_sid((const gchar *)g_hash_table_lookup(components, "steamID"));
		
		steam->clientfriends->ReportChatDeclined(chatId);
	}
}

gchar *
steamworks_get_chat_name(GHashTable *components)
{
	return g_strdup((const gchar *)g_hash_table_lookup(components, "steamID"));
}

void
steamworks_chat_invite(PurpleConnection *pc, int id, const char *message, const char *who)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;

	if (steam && steam->clientfriends)
	{
		CSteamID chatId = steamworks_chatid_to_sid(id);
		CSteamID friendId = steamworks_name_to_sid(who);

		steam->clientfriends->InviteUserToChatRoom(friendId, chatId);
	}
}

void
steamworks_chat_leave(PurpleConnection *pc, int id)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;

	if (steam && steam->clientfriends)
	{
		CSteamID chatId = steamworks_chatid_to_sid(id);

		steam->clientfriends->LeaveChatRoom(chatId);
	}
}

void
steamworks_join_chat(PurpleConnection *pc, GHashTable *components)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;

	if (steam && steam->clientfriends && components)
	{
		CSteamID chatId = steamworks_chatname_to_sid((const gchar *)g_hash_table_lookup(components, "steamID"));
		
		steam->clientfriends->JoinChatRoom(chatId);
	}
}

void
steamworks_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	PurpleConnection *pc = purple_account_get_connection(buddy->account);
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	CSteamID steamID = steamworks_name_to_sid(buddy->name);

	if (steam && steam->friends)
	{
		const gchar *name = steam->friends->GetFriendPersonaName(steamID);
		if (*name)
			purple_notify_user_info_add_pair(user_info, "Name", name);
	}

	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PurpleStatus *status = purple_presence_get_active_status(presence);
	purple_notify_user_info_add_pair(user_info, "Status", purple_status_get_name(status));
	
	if (steam)
	{
		guint64 gameid = steamworks_get_friend_gameid(steam, steamID);
		if (gameid)
		{
			gchar *gamename = NULL;
			gamename = steamworks_gameid_to_gamename(steam, gameid);
			if (gamename)
				purple_notify_user_info_add_pair(user_info, "In game", gamename);
			g_free(gamename);
		}
	}
}

const gchar *
steamworks_list_emblem(PurpleBuddy *buddy)
{
	PurpleConnection *pc = purple_account_get_connection(buddy->account);
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	CSteamID steamID = steamworks_name_to_sid(buddy->name);
	
	if (steam && steamworks_get_friend_gameid(steam, steamID))
	{
		return "game";
	}
	
	return NULL;
}

void
steamworks_close(PurpleConnection *pc)
{
	SteamInfo *steam = (SteamInfo *)pc->proto_data;
	
	purple_debug_info("steam", "close\n");

	if (!steam)
		return;
		
	purple_timeout_remove(steam->event_timer);

	g_hash_table_destroy(steam->sent_messages_table);
	
	if (steam->clientfriends)
		steam->clientfriends->SetPersonaState(k_EPersonaStateOffline);
	
	//if (steam->client && steam->pipe)
	//{
	//	if (steam->user)
	//		steam->client->ReleaseUser(steam->pipe, steam->user);
	//	steam->client->ReleaseSteamPipe(steam->pipe);
	//}
	
	steam->client = NULL;
	steam->factory = NULL;
	steam->friends = NULL;
	steam->loader = NULL;
	steam->pipe = NULL;
	steam->suser = NULL;
	steam->utils = NULL;
	steam->clientfriends = NULL;
	steam->apps = NULL;
	steam->matchmaking = NULL;
	
	g_free(steam);

	pc->proto_data = NULL;
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
	
	steam->sent_messages_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	steam->loader = new CSteamAPILoader();
	if (!steam->loader)
	{
		purple_connection_error(pc, "Could not load loader\n");
		return;
	}
	steam->factory = steam->loader->GetSteam3Factory();
	if (!steam->factory)
	{
		purple_connection_error(pc, "Could not load factory\n");
		return;
	}
	
	BGetCallback = (Steam_BGetCallbackFn)steam->loader->GetSteamClientModule()->GetSymbol("Steam_BGetCallback");
	FreeLastCallback = (Steam_FreeLastCallbackFn)steam->loader->GetSteamClientModule()->GetSymbol("Steam_FreeLastCallback");
	SteamEnumApp = (SteamEnumerateAppFn)steam->loader->GetSteamModule()->GetSymbol("SteamEnumerateApp");

	steam->client = (ISteamClient008 *)steam->factory( STEAMCLIENT_INTERFACE_VERSION_008, NULL );
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
	steam->apps = (ISteamApps001 *)steam->client->GetISteamApps(steam->user, steam->pipe, STEAMAPPS_INTERFACE_VERSION_001);
	steam->matchmaking = (ISteamMatchmaking008 *)steam->client->GetISteamMatchmaking(steam->user, steam->pipe, STEAMMATCHMAKING_INTERFACE_VERSION_008);

	IClientEngine *clientEngine = (IClientEngine *)steam->factory( CLIENTENGINE_INTERFACE_VERSION_002, NULL );
	if (!clientEngine)
	{
		clientEngine = (IClientEngine *)steam->factory( "CLIENTENGINE_INTERFACE_VERSION001", NULL );
		if (!clientEngine)
		{
			purple_connection_error(pc, "Could not load client engine\n");
			return;
		}
	}
	//steam->clientfriends = clientEngine->GetIClientFriends( steam->user, steam->pipe, CLIENTFRIENDS_INTERFACE_VERSION );
	HSteamPipe clientPipe = clientEngine->CreateSteamPipe();
	HSteamUser clientUser = clientEngine->ConnectToGlobalUser(clientPipe);
	steam->clientfriends = clientEngine->GetIClientFriends( clientUser, clientPipe, CLIENTFRIENDS_INTERFACE_VERSION );
	if (!steam->clientfriends)
	{
		purple_connection_error(pc, "Could not load client friends\n");
		return;
	}

	purple_connection_set_state(pc, PURPLE_CONNECTED);
	steamworks_set_status(account, purple_account_get_active_status(account));

	purple_account_set_alias(account, steam->friends->GetPersonaName());

	steam->suser = (ISteamUser005 *)steam->client->GetISteamUser(steam->user, steam->pipe, STEAMUSER_INTERFACE_VERSION_005);
	if (steam->suser)
	{
		if (!steam->suser->BLoggedOn())
		{
			purple_debug_info("steam", "Not logged in.  Logging in...\n");
			steam->suser->SetAccountName(account->username);
			steam->suser->SetPassword(account->password);
			steam->suser->LogOn(steam->suser->GetSteamID());
		}
		steam->suser->SetSelfAsPrimaryChatDestination();
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
		purple_debug_info("steam", "CSteam id %d\n", steamID);
		const gchar *id = steamworks_sid_to_name(steamID);
		purple_debug_info("steam", "Friend id %s\n", id);
		const gchar *alias = steam->friends->GetFriendPersonaName( steamID );
		purple_debug_info("steam", "Friend alias %s\n", alias);
		
		PurpleBuddy *buddy;
		buddy = purple_find_buddy(account, id);
		if (!buddy)
		{
			buddy = purple_buddy_new(account, id, NULL);
			purple_blist_add_buddy(buddy, NULL, group, NULL);
		}
		serv_got_alias(pc, id, alias);
		buddy->proto_data = &steamID;
		
		EPersonaState state = steam->friends->GetFriendPersonaState(steamID);
		purple_debug_info("steam", "Friend state %d\n", state);
		const gchar *status_id = steamworks_personastate_to_statustype(state);
		purple_debug_info("steam", "status_id %s\n", status_id);
		purple_prpl_got_user_status(account, id, status_id, NULL);
		
		steamworks_load_avatar_for_user(account, steamID);
	}

	//Finally, start the event loop
	steam->event_timer = purple_timeout_add_seconds(1, (GSourceFunc)steamworks_eventloop, pc);
}
