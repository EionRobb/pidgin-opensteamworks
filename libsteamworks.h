
#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#define STEAMWORKS_CLIENT_INTERFACES
#define		STEAM_API_NON_VERSIONED_INTERFACES
#define		STEAMWORKS_CLIENT_INTERFACES
#define		WLB_WINSECURE
#include "windows.h"
#include "winbase.h"
#include "Steamworks.h"

#include <glib.h>

#include "prpl.h"
#include "plugin.h"
#include "account.h"
#include "blist.h"
#include "buddyicon.h"
#include "debug.h"
#include "accountopt.h"

#ifndef CLIENTENGINE_INTERFACE_VERSION_002
#	define CLIENTENGINE_INTERFACE_VERSION_002 "CLIENTENGINE_INTERFACE_VERSION002"
#endif

extern "C" {
	const gchar *steamworks_list_icon(PurpleAccount *account, PurpleBuddy *buddy);
	gchar *steamworks_status_text(PurpleBuddy *buddy);
	GList *steamworks_status_types(PurpleAccount *account);
	void steamworks_login(PurpleAccount *account);
	void steamworks_close(PurpleConnection *pc);
	gint steamworks_send_im(PurpleConnection *pc, const char *who, const char *message, PurpleMessageFlags flags);
	guint steamworks_send_typing(PurpleConnection *pc, const char *who, PurpleTypingState state);
	void steamworks_set_status(PurpleAccount *account, PurpleStatus *status);
	void steamworks_add_buddy(PurpleConnection *, PurpleBuddy *buddy, PurpleGroup *group);
	void steamworks_remove_buddy(PurpleConnection *, PurpleBuddy *buddy, PurpleGroup *group);
	void steamworks_alias_buddy(PurpleConnection *, const char *who, const char *alias);
	void steamworks_ignore_buddy(PurpleConnection *, const char *name);
	void steamworks_unignore_buddy(PurpleConnection *, const char *name);
	int steamworks_chat_send(PurpleConnection *, int id, const char *message, PurpleMessageFlags flags);
	GList *steamworks_chat_info(PurpleConnection *);
	GHashTable *steamworks_chat_defaults(PurpleConnection *, const char *chat_name);
	void steamworks_reject_chat(PurpleConnection *, GHashTable *components);
	gchar *steamworks_get_chat_name(GHashTable *components);
	void steamworks_chat_invite(PurpleConnection *, int id, const char *message, const char *who);
	void steamworks_chat_leave(PurpleConnection *, int id);
	void steamworks_join_chat(PurpleConnection *, GHashTable *components);
	void steamworks_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full);
	const gchar *steamworks_list_emblem(PurpleBuddy *buddy);

	static PurplePluginProtocolInfo prpl_info = {
		(PurpleProtocolOptions) (OPT_PROTO_CHAT_TOPIC),// options
		NULL,                     // user_splits
		NULL,                     // protocol_options
		{"png,jpeg",0,0,64,64,0,PURPLE_ICON_SCALE_DISPLAY},// icon_spec
		steamworks_list_icon,     // list_icon
		steamworks_list_emblem,   // list_emblem
		steamworks_status_text,   // status_text
		steamworks_tooltip_text,  // tooltip_text
		steamworks_status_types,  // status_types
		NULL,                     // blist_node_menu
		steamworks_chat_info,     // chat_info
		steamworks_chat_defaults, // chat_info_defaults
		steamworks_login,         // login
		steamworks_close,         // close
		steamworks_send_im,       // send_im
		NULL,                     // set_info
		steamworks_send_typing,   // send_typing
		NULL,                     // get_info
		steamworks_set_status,    // set_status
		NULL,                     // set_idle
		NULL,                     // change_passwd
		steamworks_add_buddy,     // add_buddy
		NULL,                     // add_buddies
		steamworks_remove_buddy,  // remove_buddy
		NULL,                     // remove_buddies
		NULL,                     // add_permit
		steamworks_ignore_buddy,  // add_deny
		NULL,                     // rem_permit
		steamworks_unignore_buddy,// rem_deny
		NULL,                     // set_permit_deny
		steamworks_join_chat,     // join_chat
		steamworks_reject_chat,   // reject_chat
		steamworks_get_chat_name, // get_chat_name
		steamworks_chat_invite,   // chat_invite
		steamworks_chat_leave,    // chat_leave
		NULL,                     // chat_whisper
		steamworks_chat_send,     // chat_send
		NULL,                     // keepalive
		NULL,                     // register_user
		NULL,                     // get_cb_info
		NULL,                     // get_cb_away
		steamworks_alias_buddy,   // alias_buddy
		NULL,                     // group_buddy
		NULL,                     // rename_group
		NULL,                     // buddy_free
		NULL,                     // convo_closed
		purple_normalize_nocase,  // normalize
		NULL,                     // set_buddy_icon
		NULL,                     // remove_group
		NULL,                     // get_cb_real_name
		NULL,                     // set_chat_topic
		NULL,                     // find_blist_chat
		NULL,                     // roomlist_get_list
		NULL,                     // roomlist_cancel
		NULL,                     // roomlist_expand_category
		NULL,                     // can_receive_file
		NULL,                     // send_file
		NULL,                     // new_xfer
		NULL,                     // offline_message
		NULL,                     // whiteboard_prpl_ops
		NULL,                     // send_raw
		NULL,                     // roomlist_room_serialize
		NULL,                     // unregister_user
		NULL,                     // send_attention
		NULL,                     // get_attention_types
		sizeof(PurplePluginProtocolInfo), // struct_size
		NULL,                     // account_text_table
		NULL,                     // initiate_media
		NULL,                     // media_caps
		NULL,                     // get_moods
		NULL,                     // set_public_alias
		NULL                      // get_public_alias
	};

	static gboolean
	plugin_load(PurplePlugin *plugin)
	{
		purple_debug_info("steam", "plugin_load\n");
		return TRUE;
	}

	static gboolean
	plugin_unload(PurplePlugin *plugin)
	{
		purple_debug_info("steam", "plugin_unload\n");
		return TRUE;
	}

	static PurplePluginInfo info = {
		PURPLE_PLUGIN_MAGIC,
		2, 2,
		PURPLE_PLUGIN_PROTOCOL,
		NULL,
		0,
		NULL,
		PURPLE_PRIORITY_DEFAULT,
		"prpl-bigbrownchunx-steamworks",
		"Steam",
		"0.1",
		"",
		"",
		"Eion Robb <eionrobb@gmail.com>",
		"",
		plugin_load,
		plugin_unload,
		NULL,
		NULL,
		&prpl_info,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};

	static void
	plugin_init(PurplePlugin *plugin)
	{
		PurpleAccountOption *option;
		option = purple_account_option_string_new("Server", "host", "127.0.0.1");
		prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
		
		purple_debug_info("steam", "plugin_init\n");
	}


	PURPLE_INIT_PLUGIN(steamworks, plugin_init, info);

};
