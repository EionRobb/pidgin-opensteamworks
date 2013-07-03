

#include "steam_connection.h"

#if !PURPLE_VERSION_CHECK(3, 0, 0)
	#define purple_connection_error purple_connection_error_reason
#endif

#if !GLIB_CHECK_VERSION (2, 22, 0)
#define g_hostname_is_ip_address(hostname) (g_ascii_isdigit(hostname[0]) && g_strstr_len(hostname, 4, "."))
#endif

static void steam_attempt_connection(SteamConnection *);
static void steam_next_connection(SteamAccount *sa);

#include <zlib.h>

static gchar *steam_gunzip(const guchar *gzip_data, gssize *len_ptr)
{
	gsize gzip_data_len	= *len_ptr;
	z_stream zstr;
	int gzip_err = 0;
	gchar *data_buffer;
	gulong gzip_len = G_MAXUINT16;
	GString *output_string = NULL;

	data_buffer = g_new0(gchar, gzip_len);

	zstr.next_in = NULL;
	zstr.avail_in = 0;
	zstr.zalloc = Z_NULL;
	zstr.zfree = Z_NULL;
	zstr.opaque = 0;
	gzip_err = inflateInit2(&zstr, MAX_WBITS+32);
	if (gzip_err != Z_OK)
	{
		g_free(data_buffer);
		purple_debug_error("steam", "no built-in gzip support in zlib\n");
		return NULL;
	}
	
	zstr.next_in = (Bytef *)gzip_data;
	zstr.avail_in = gzip_data_len;
	
	zstr.next_out = (Bytef *)data_buffer;
	zstr.avail_out = gzip_len;
	
	gzip_err = inflate(&zstr, Z_SYNC_FLUSH);

	if (gzip_err == Z_DATA_ERROR)
	{
		inflateEnd(&zstr);
		inflateInit2(&zstr, -MAX_WBITS);
		if (gzip_err != Z_OK)
		{
			g_free(data_buffer);
			purple_debug_error("steam", "Cannot decode gzip header\n");
			return NULL;
		}
		zstr.next_in = (Bytef *)gzip_data;
		zstr.avail_in = gzip_data_len;
		zstr.next_out = (Bytef *)data_buffer;
		zstr.avail_out = gzip_len;
		gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
	}
	output_string = g_string_new("");
	while (gzip_err == Z_OK)
	{
		//append data to buffer
		output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
		//reset buffer pointer
		zstr.next_out = (Bytef *)data_buffer;
		zstr.avail_out = gzip_len;
		gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
	}
	if (gzip_err == Z_STREAM_END)
	{
		output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
	} else {
		purple_debug_error("steam", "gzip inflate error\n");
	}
	inflateEnd(&zstr);

	g_free(data_buffer);	

	if (len_ptr)
		*len_ptr = output_string->len;

	return g_string_free(output_string, FALSE);
}

void
steam_connection_close(SteamConnection *steamcon)
{
	steamcon->sa->conns = g_slist_remove(steamcon->sa->conns, steamcon);
	
	if (steamcon->connect_data != NULL) {
		purple_proxy_connect_cancel(steamcon->connect_data);
		steamcon->connect_data = NULL;
	}

	if (steamcon->ssl_conn != NULL) {
		purple_ssl_close(steamcon->ssl_conn);
		steamcon->ssl_conn = NULL;
	}

	if (steamcon->fd >= 0) {
		close(steamcon->fd);
		steamcon->fd = -1;
	}

	if (steamcon->input_watcher > 0) {
		purple_input_remove(steamcon->input_watcher);
		steamcon->input_watcher = 0;
	}
	
	purple_timeout_remove(steamcon->timeout_watcher);
	
	g_free(steamcon->rx_buf);
	steamcon->rx_buf = NULL;
	steamcon->rx_len = 0;
}

void steam_connection_destroy(SteamConnection *steamcon)
{
	steam_connection_close(steamcon);
	
	if (steamcon->request != NULL)
		g_string_free(steamcon->request, TRUE);
	
	g_free(steamcon->url);
	g_free(steamcon->hostname);
	g_free(steamcon);
}

static void steam_update_cookies(SteamAccount *sa, const gchar *headers)
{
	const gchar *cookie_start;
	const gchar *cookie_end;
	gchar *cookie_name;
	gchar *cookie_value;
	int header_len;

	g_return_if_fail(headers != NULL);

	header_len = strlen(headers);

	/* look for the next "Set-Cookie: " */
	/* grab the data up until ';' */
	cookie_start = headers;
	while ((cookie_start = strstr(cookie_start, "\r\nSet-Cookie: ")) &&
			(cookie_start - headers) < header_len)
	{
		cookie_start += 14;
		cookie_end = strchr(cookie_start, '=');
		cookie_name = g_strndup(cookie_start, cookie_end-cookie_start);
		cookie_start = cookie_end + 1;
		cookie_end = strchr(cookie_start, ';');
		cookie_value= g_strndup(cookie_start, cookie_end-cookie_start);
		cookie_start = cookie_end;

		g_hash_table_replace(sa->cookie_table, cookie_name,
				cookie_value);
	}
}

static void steam_connection_process_data(SteamConnection *steamcon)
{
	gssize len;
	gchar *tmp;

	len = steamcon->rx_len;
	tmp = g_strstr_len(steamcon->rx_buf, len, "\r\n\r\n");
	if (tmp == NULL) {
		/* This is a corner case that occurs when the connection is
		 * prematurely closed either on the client or the server.
		 * This can either be no data at all or a partial set of
		 * headers.  We pass along the data to be good, but don't
		 * do any fancy massaging.  In all likelihood the result will
		 * be tossed by the connection callback func anyways
		 */
		tmp = g_strndup(steamcon->rx_buf, len);
	} else {
		tmp += 4;
		len -= g_strstr_len(steamcon->rx_buf, len, "\r\n\r\n") -
				steamcon->rx_buf + 4;
		tmp = g_memdup(tmp, len + 1);
		tmp[len] = '\0';
		steamcon->rx_buf[steamcon->rx_len - len] = '\0';
		steam_update_cookies(steamcon->sa, steamcon->rx_buf);

		if (strstr(steamcon->rx_buf, "Content-Encoding: gzip"))
		{
			/* we've received compressed gzip data, decompress */
			gchar *gunzipped;
			gunzipped = steam_gunzip((const guchar *)tmp, &len);
			g_free(tmp);
			tmp = gunzipped;
		}
	}

	g_free(steamcon->rx_buf);
	steamcon->rx_buf = NULL;

	if (steamcon->callback != NULL) {
		if (!len)
		{
			purple_debug_error("steam", "No data in response\n");
		} else {
			JsonParser *parser = json_parser_new();
			if (!json_parser_load_from_data(parser, tmp, len, NULL))
			{
				purple_debug_error("steam", "Error parsing response: %s\n", tmp);
			} else {
				JsonNode *root = json_parser_get_root(parser);
				JsonObject *jsonobj = json_node_get_object(root);
				
				//purple_debug_info("steam", "Got response: %s\n", tmp);
				purple_debug_info("steam", "executing callback for %s\n", steamcon->url);
				steamcon->callback(steamcon->sa, jsonobj, steamcon->user_data);
			}
			g_object_unref(parser);
		}
	}

	g_free(tmp);
}

static void steam_fatal_connection_cb(SteamConnection *steamcon)
{
	PurpleConnection *pc = steamcon->sa->pc;

	purple_debug_error("steam", "fatal connection error\n");

	steam_connection_destroy(steamcon);

	/* We died.  Do not pass Go.  Do not collect $200 */
	/* In all seriousness, don't attempt to call the normal callback here.
	 * That may lead to the wrong error message being displayed */
	purple_connection_error(pc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Server closed the connection."));

}

static void steam_post_or_get_readdata_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	SteamConnection *steamcon;
	SteamAccount *sa;
	gchar buf[4096];
	gssize len;

	steamcon = data;
	sa = steamcon->sa;

	if (steamcon->method & STEAM_METHOD_SSL) {
		len = purple_ssl_read(steamcon->ssl_conn,
				buf, sizeof(buf) - 1);
	} else {
		len = recv(steamcon->fd, buf, sizeof(buf) - 1, 0);
	}

	if (len < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			/* Try again later */
			return;
		}

		if (steamcon->method & STEAM_METHOD_SSL && steamcon->rx_len > 0) {
			/*
			 * This is a slightly hacky workaround for a bug in either
			 * GNU TLS or in the SSL implementation on steam's web
			 * servers.  The sequence of events is:
			 * 1. We attempt to read the first time and successfully read
			 *    the server's response.
			 * 2. We attempt to read a second time and libpurple's call
			 *    to gnutls_record_recv() returns the error
			 *    GNUTLS_E_UNEXPECTED_PACKET_LENGTH, or
			 *    "A TLS packet with unexpected length was received."
			 *
			 * Normally the server would have closed the connection
			 * cleanly and this second read() request would have returned
			 * 0.  Or maybe it's normal for SSL connections to be severed
			 * in this manner?  In any case, this differs from the behavior
			 * of the standard recv() system call.
			 */
			purple_debug_warning("steam",
				"ssl error, but data received.  attempting to continue\n");
		} else {
			/* Try resend the request */
			steamcon->retry_count++;
			if (steamcon->retry_count < 3) {
				steam_connection_close(steamcon);
				steamcon->request_time = time(NULL);
				
				g_queue_push_head(sa->waiting_conns, steamcon);
				steam_next_connection(sa);
			} else {
				steam_fatal_connection_cb(steamcon);
			}
			return;
		}
	}

	if (len > 0)
	{
		buf[len] = '\0';

		steamcon->rx_buf = g_realloc(steamcon->rx_buf,
				steamcon->rx_len + len + 1);
		memcpy(steamcon->rx_buf + steamcon->rx_len, buf, len + 1);
		steamcon->rx_len += len;

		/* Wait for more data before processing */
		return;
	}

	/* The server closed the connection, let's parse the data */
	steam_connection_process_data(steamcon);

	steam_connection_destroy(steamcon);
	
	steam_next_connection(sa);
}

static void steam_post_or_get_ssl_readdata_cb (gpointer data,
		PurpleSslConnection *ssl, PurpleInputCondition cond)
{
	steam_post_or_get_readdata_cb(data, -1, cond);
}

static void steam_post_or_get_connect_cb(gpointer data, gint source,
		const gchar *error_message)
{
	SteamConnection *steamcon;
	gssize len;

	steamcon = data;
	steamcon->connect_data = NULL;

	if (error_message)
	{
		purple_debug_error("steam", "post_or_get_connect failure to %s\n", steamcon->url);
		purple_debug_error("steam", "post_or_get_connect_cb %s\n",
				error_message);
		steam_fatal_connection_cb(steamcon);
		return;
	}

	steamcon->fd = source;

	/* TODO: Check the return value of write() */
	len = write(steamcon->fd, steamcon->request->str,
			steamcon->request->len);
	steamcon->input_watcher = purple_input_add(steamcon->fd,
			PURPLE_INPUT_READ,
			steam_post_or_get_readdata_cb, steamcon);
}

static void steam_post_or_get_ssl_connect_cb(gpointer data,
		PurpleSslConnection *ssl, PurpleInputCondition cond)
{
	SteamConnection *steamcon;
	gssize len;

	steamcon = data;

	purple_debug_info("steam", "post_or_get_ssl_connect_cb\n");

	/* TODO: Check the return value of write() */
	len = purple_ssl_write(steamcon->ssl_conn,
			steamcon->request->str, steamcon->request->len);
	purple_ssl_input_add(steamcon->ssl_conn,
			steam_post_or_get_ssl_readdata_cb, steamcon);
}

static void steam_host_lookup_cb(GSList *hosts, gpointer data,
		const char *error_message)
{
	GSList *host_lookup_list;
	struct sockaddr_in *addr;
	gchar *hostname;
	gchar *ip_address;
	SteamAccount *sa;
	PurpleDnsQueryData *query;

	/* Extract variables */
	host_lookup_list = data;

	sa = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);
	hostname = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);
	query = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);

	/* The callback has executed, so we no longer need to keep track of
	 * the original query.  This always needs to run when the cb is 
	 * executed. */
	sa->dns_queries = g_slist_remove(sa->dns_queries, query);

	/* Any problems, capt'n? */
	if (error_message != NULL)
	{
		purple_debug_warning("steam",
				"Error doing host lookup: %s\n", error_message);
		return;
	}

	if (hosts == NULL)
	{
		purple_debug_warning("steam",
				"Could not resolve host name\n");
		return;
	}

	/* Discard the length... */
	hosts = g_slist_delete_link(hosts, hosts);
	/* Copy the address then free it... */
	addr = hosts->data;
	ip_address = g_strdup(inet_ntoa(addr->sin_addr));
	g_free(addr);
	hosts = g_slist_delete_link(hosts, hosts);

	/*
	 * DNS lookups can return a list of IP addresses, but we only cache
	 * the first one.  So free the rest.
	 */
	while (hosts != NULL)
	{
		/* Discard the length... */
		hosts = g_slist_delete_link(hosts, hosts);
		/* Free the address... */
		g_free(hosts->data);
		hosts = g_slist_delete_link(hosts, hosts);
	}

	g_hash_table_insert(sa->hostname_ip_cache, hostname, ip_address);
}

static void steam_cookie_foreach_cb(gchar *cookie_name,
		gchar *cookie_value, GString *str)
{
	/* TODO: Need to escape name and value? */
	g_string_append_printf(str, "%s=%s;", cookie_name, cookie_value);
}

/**
 * Serialize the sa->cookie_table hash table to a string.
 */
gchar *steam_cookies_to_string(SteamAccount *sa)
{
	GString *str;

	str = g_string_new(NULL);

	g_hash_table_foreach(sa->cookie_table,
			(GHFunc)steam_cookie_foreach_cb, str);

	return g_string_free(str, FALSE);
}

static void steam_ssl_connection_error(PurpleSslConnection *ssl,
		PurpleSslErrorType errortype, gpointer data)
{
	SteamConnection *steamcon = data;
	SteamAccount *sa = steamcon->sa;
	PurpleConnection *pc = sa->pc;
	
	steamcon->ssl_conn = NULL;
	
	/* Try resend the request */
	steamcon->retry_count++;
	if (steamcon->retry_count < 3) {
		steam_connection_close(steamcon);
		steamcon->request_time = time(NULL);
		
		g_queue_push_head(sa->waiting_conns, steamcon);
		steam_next_connection(sa);
	} else {
		steam_connection_destroy(steamcon);
		purple_connection_ssl_error(pc, errortype);
	}
}

void steam_post_or_get(SteamAccount *sa, SteamMethod method,
		const gchar *host, const gchar *url, const gchar *postdata,
		SteamProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive)
{
	GString *request;
	gchar *cookies;
	SteamConnection *steamcon;
	gchar *real_url;
	gboolean is_proxy = FALSE;
	const gchar *user_agent;
	const gchar* const *languages;
	gchar *language_names;
	PurpleProxyInfo *proxy_info = NULL;
	gchar *proxy_auth;
	gchar *proxy_auth_base64;

	/* TODO: Fix keepalive and use it as much as possible */
	keepalive = FALSE;

	if (host == NULL)
		host = "api.steampowered.com";

	if (sa && sa->account)
	{
		if (purple_account_get_bool(sa->account, "use-https", FALSE))
			method |= STEAM_METHOD_SSL;
	}

	if (sa && sa->account && !(method & STEAM_METHOD_SSL))
	{
		proxy_info = purple_proxy_get_setup(sa->account);
		if (purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_USE_GLOBAL)
			proxy_info = purple_global_proxy_get_info();
		if (purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_HTTP)
		{
			is_proxy = TRUE;
		}	
	}
	if (is_proxy == TRUE)
	{
		real_url = g_strdup_printf("http://%s%s", host, url);
	} else {
		real_url = g_strdup(url);
	}

	cookies = steam_cookies_to_string(sa);
	user_agent = purple_account_get_string(sa->account, "user-agent", "Steam 1.2.0 / iPhone");
	
	if (method & STEAM_METHOD_POST && !postdata)
		postdata = "";

	/* Build the request */
	request = g_string_new(NULL);
	g_string_append_printf(request, "%s %s HTTP/1.0\r\n",
			(method & STEAM_METHOD_POST) ? "POST" : "GET",
			real_url);
	if (is_proxy == FALSE)
		g_string_append_printf(request, "Host: %s\r\n", host);
	g_string_append_printf(request, "Connection: %s\r\n",
			(keepalive ? "Keep-Alive" : "close"));
	g_string_append_printf(request, "User-Agent: %s\r\n", user_agent);
	if (method & STEAM_METHOD_POST) {
		g_string_append_printf(request,
				"Content-Type: application/x-www-form-urlencoded\r\n");
		g_string_append_printf(request,
				"Content-length: %zu\r\n", strlen(postdata));
	}
	g_string_append_printf(request, "Accept: */*\r\n");
	//Only use cookies for steamcommunity.com
	if (g_str_equal(host, "steamcommunity.com"))
		g_string_append_printf(request, "Cookie: %s\r\n", cookies);
	g_string_append_printf(request, "Accept-Encoding: gzip\r\n");
	if (is_proxy == TRUE)
	{
		if (purple_proxy_info_get_username(proxy_info) &&
			purple_proxy_info_get_password(proxy_info))
		{
			proxy_auth = g_strdup_printf("%s:%s", purple_proxy_info_get_username(proxy_info), purple_proxy_info_get_password(proxy_info));
			proxy_auth_base64 = purple_base64_encode((guchar *)proxy_auth, strlen(proxy_auth));
			g_string_append_printf(request, "Proxy-Authorization: Basic %s\r\n", proxy_auth_base64);
			g_free(proxy_auth_base64);
			g_free(proxy_auth);
		}
	}

	/* Tell the server what language we accept, so that we get error messages in our language (rather than our IP's) */
	languages = g_get_language_names();
	language_names = g_strjoinv(", ", (gchar **)languages);
	purple_util_chrreplace(language_names, '_', '-');
	g_string_append_printf(request, "Accept-Language: %s\r\n", language_names);
	g_free(language_names);

	purple_debug_info("steam", "getting url %s\n", url);

	g_string_append_printf(request, "\r\n");
	if (method & STEAM_METHOD_POST)
		g_string_append_printf(request, "%s", postdata);

	/* If it needs to go over a SSL connection, we probably shouldn't print
	 * it in the debug log.  Without this condition a user's password is
	 * printed in the debug log */
	if (method == STEAM_METHOD_POST)
		purple_debug_info("steam", "sending request data:\n%s\n",
			postdata);

	g_free(cookies);

	steamcon = g_new0(SteamConnection, 1);
	steamcon->sa = sa;
	steamcon->url = real_url;
	steamcon->method = method;
	steamcon->hostname = g_strdup(host);
	steamcon->request = request;
	steamcon->callback = callback_func;
	steamcon->user_data = user_data;
	steamcon->fd = -1;
	steamcon->connection_keepalive = keepalive;
	steamcon->request_time = time(NULL);
	
	g_queue_push_head(sa->waiting_conns, steamcon);
	steam_next_connection(sa);
}

static void steam_next_connection(SteamAccount *sa)
{
	SteamConnection *steamcon;
	
	g_return_if_fail(sa != NULL);	
	
	if (!g_queue_is_empty(sa->waiting_conns))
	{
		if(g_slist_length(sa->conns) < STEAM_MAX_CONNECTIONS)
		{
			steamcon = g_queue_pop_tail(sa->waiting_conns);
			steam_attempt_connection(steamcon);
		}
	}
}


static gboolean
steam_connection_timedout(gpointer userdata)
{
	SteamConnection *steamcon = userdata;
	SteamAccount *sa = steamcon->sa;
	
	/* Try resend the request */
	steamcon->retry_count++;
	if (steamcon->retry_count < 3) {
		steam_connection_close(steamcon);
		steamcon->request_time = time(NULL);
		
		g_queue_push_head(sa->waiting_conns, steamcon);
		steam_next_connection(sa);
	} else {
		steam_fatal_connection_cb(steamcon);
	}
	
	return FALSE;
}

static void steam_attempt_connection(SteamConnection *steamcon)
{
	gboolean is_proxy = FALSE;
	SteamAccount *sa = steamcon->sa;
	PurpleProxyInfo *proxy_info = NULL;

	if (sa && sa->account && !(steamcon->method & STEAM_METHOD_SSL))
	{
		proxy_info = purple_proxy_get_setup(sa->account);
		if (purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_USE_GLOBAL)
			proxy_info = purple_global_proxy_get_info();
		if (purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_HTTP)
		{
			is_proxy = TRUE;
		}	
	}

#if 0
	/* Connection to attempt retries.  This code doesn't work perfectly, but
	 * remains here for future reference if needed */
	if (time(NULL) - steamcon->request_time > 5) {
		/* We've continuously tried to remake this connection for a 
		 * bit now.  It isn't happening, sadly.  Time to die. */
		purple_debug_error("steam", "could not connect after retries\n");
		steam_fatal_connection_cb(steamcon);
		return;
	}

	purple_debug_info("steam", "making connection attempt\n");

	/* TODO: If we're retrying the connection, consider clearing the cached
	 * DNS value.  This will require some juggling with the hostname param */
	/* TODO/FIXME: This retries almost instantenously, which in some cases
	 * runs at blinding speed.  Slow it down. */
	/* TODO/FIXME: this doesn't retry properly on non-ssl connections */
#endif
	
	sa->conns = g_slist_prepend(sa->conns, steamcon);

	/*
	 * Do a separate DNS lookup for the given host name and cache it
	 * for next time.
	 *
	 * TODO: It would be better if we did this before we call
	 *       purple_proxy_connect(), so we could re-use the result.
	 *       Or even better: Use persistent HTTP connections for servers
	 *       that we access continually.
	 *
	 * TODO: This cache of the hostname<-->IP address does not respect
	 *       the TTL returned by the DNS server.  We should expire things
	 *       from the cache after some amount of time.
	 */
	if (!is_proxy && !(steamcon->method & STEAM_METHOD_SSL) && !g_hostname_is_ip_address(steamcon->hostname))
	{
		/* Don't do this for proxy connections, since proxies do the DNS lookup */
		gchar *host_ip;

		host_ip = g_hash_table_lookup(sa->hostname_ip_cache, steamcon->hostname);
		if (host_ip != NULL) {
			g_free(steamcon->hostname);
			steamcon->hostname = g_strdup(host_ip);
		} else if (sa->account && !sa->account->disconnecting) {
			GSList *host_lookup_list = NULL;
			PurpleDnsQueryData *query;

			host_lookup_list = g_slist_prepend(
					host_lookup_list, g_strdup(steamcon->hostname));
			host_lookup_list = g_slist_prepend(
					host_lookup_list, sa);

			query = purple_dnsquery_a(
#if PURPLE_VERSION_CHECK(3, 0, 0)
					steamcon->sa->account,
#endif
					steamcon->hostname, 80,
					steam_host_lookup_cb, host_lookup_list);
			sa->dns_queries = g_slist_prepend(sa->dns_queries, query);
			host_lookup_list = g_slist_append(host_lookup_list, query);
		}
	}

	if (steamcon->method & STEAM_METHOD_SSL) {
		steamcon->ssl_conn = purple_ssl_connect(sa->account, steamcon->hostname,
				443, steam_post_or_get_ssl_connect_cb,
				steam_ssl_connection_error, steamcon);
	} else {
		steamcon->connect_data = purple_proxy_connect(NULL, sa->account,
				steamcon->hostname, 80, steam_post_or_get_connect_cb, steamcon);
	}
	
	steamcon->timeout_watcher = purple_timeout_add_seconds(120, steam_connection_timedout, steamcon);

	return;
}

