
#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "Steamworks.h"

#include <glib.h>

#include "prpl.h"
#include "plugin.h"
#include "account.h"
#include "blist.h"
#include "buddyicon.h"
#include "debug.h"
#include "accountopt.h"


extern "C" {
	const gchar *steamworks_list_icon(PurpleAccount *account, PurpleBuddy *buddy);
	gchar *steamworks_status_text(PurpleBuddy *buddy);
	GList *steamworks_status_types(PurpleAccount *account);
	void steamworks_login(PurpleAccount *account);
	void steamworks_close(PurpleConnection *pc);
	gint steamworks_send_im(PurpleConnection *pc, const char *who, const char *message, PurpleMessageFlags flags);
	guint steamworks_send_typing(PurpleConnection *pc, const char *who, PurpleTypingState state);
	void steamworks_set_status(PurpleAccount *account, PurpleStatus *status);

	static PurplePluginProtocolInfo prpl_info = {
		(PurpleProtocolOptions) (0),// options
		NULL,                     // user_splits
		NULL,                     // protocol_options
		{"jpeg",0,0,64,64,0,PURPLE_ICON_SCALE_DISPLAY},// icon_spec
		steamworks_list_icon,     // list_icon
		NULL,                     // list_emblem
		steamworks_status_text,   // status_text
		NULL,                     // tooltip_text
		steamworks_status_types,  // status_types
		NULL,                     // blist_node_menu
		NULL,                     // chat_info
		NULL,                     // chat_info_defaults
		steamworks_login,         // login
		steamworks_close,         // close
		steamworks_send_im,       // send_im
		NULL,                     // set_info
		steamworks_send_typing,   // send_typing
		NULL,                     // get_info
		steamworks_set_status,    // set_status
		NULL,                     // set_idle
		NULL,                     // change_passwd
		NULL,                     // add_buddy
		NULL,                     // add_buddies
		NULL,                     // remove_buddy
		NULL,                     // remove_buddies
		NULL,                     // add_permit
		NULL,                     // add_deny
		NULL,                     // rem_permit
		NULL,                     // rem_deny
		NULL,                     // set_permit_deny
		NULL,                     // join_chat
		NULL,                     // reject_chat
		NULL,                     // get_chat_name
		NULL,                     // chat_invite
		NULL,                     // chat_leave
		NULL,                     // chat_whisper
		NULL,                     // chat_send
		NULL,                     // keepalive
		NULL,                     // register_user
		NULL,                     // get_cb_info
		NULL,                     // get_cb_away
		NULL,                     // alias_buddy
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
		sizeof(PurplePluginProtocolInfo) // struct_size
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
