 /*
  *
  * Pidgin SendScreenshot plugin - http upload  -
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
  *
  *
  * --  Raoul Berger <contact@raoulito.info>
  *
  */

#include "upload_utils.h"
#include "http_upload.h"
#include "prefs.h"

static gpointer
http_upload (PurplePlugin * plugin)
{				/* this is a thread */
  guint i;
  CURL *curl;
  CURLcode res;

  struct curl_httppost *formpost = NULL;
  struct curl_httppost *lastptr = NULL;
  struct curl_slist *headerlist = NULL;
  static const char buf[] = "Expect:";
  gchar *img_ctype = NULL;

  img_ctype =
    g_strdup_printf ("image/%s", purple_prefs_get_string (PREF_IMAGE_TYPE));

  curl_global_init (CURL_GLOBAL_ALL);

  /* fill in extra fields */
  for (i = 0; i < PLUGIN (host_data)->extra_names->len; i++)
    {
      curl_formadd (&formpost,
		    &lastptr,
		    CURLFORM_COPYNAME,
		    g_array_index (PLUGIN (host_data)->extra_names, gchar *,
				   i), CURLFORM_COPYCONTENTS,
		    g_array_index (PLUGIN (host_data)->extra_values, gchar *,
				   i), CURLFORM_END);
    }
  /* fill in the file upload field */
  curl_formadd (&formpost,
		&lastptr,
		CURLFORM_COPYNAME,
		PLUGIN (host_data)->file_input_name,
		CURLFORM_FILE, PLUGIN (capture_path_filename),
		CURLFORM_CONTENTTYPE, img_ctype, CURLFORM_END);
  g_free (img_ctype);

  headerlist = curl_slist_append (headerlist, buf);
  if ((curl = curl_easy_init ()) != NULL)
    {
      char *path;
      FILE *store_url_data = purple_mkstemp (&path, FALSE);
      static char curl_error[CURL_ERROR_SIZE];

      curl_easy_setopt (curl, CURLOPT_URL, PLUGIN (host_data)->form_action);

      curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT,
			purple_prefs_get_int (PREF_UPLOAD_CONNECTTIMEOUT));
      curl_easy_setopt (curl, CURLOPT_TIMEOUT,
			purple_prefs_get_int (PREF_UPLOAD_TIMEOUT));
      curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);

      /* hmm... or InternetExplorer? */
      curl_easy_setopt (curl, CURLOPT_USERAGENT, "Mozilla/5.0");

      curl_easy_setopt (curl, CURLOPT_WRITEDATA, store_url_data);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, fwrite);
      curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headerlist);

      curl_easy_setopt (curl, CURLOPT_HTTPPOST, formpost);
      res = curl_easy_perform (curl);

      curl_formfree (formpost);
      curl_slist_free_all (headerlist);
      fclose (store_url_data);
      curl_easy_cleanup (curl);

      /* 
       * Make sure that the plugin is still _loaded_ when entering that block.
       * see plugin_unload ()
       */
      G_LOCK (unload);
      if (plugin->extra != NULL)
	{
	  gsize size;

	  if (res == 0)
	    {
	      GError *error = NULL;

	      if (g_file_get_contents (path,
				       &PLUGIN (host_data)->htmlcode, &size,
				       &error) == FALSE)
		{
		  if (PLUGIN (host_data)->htmlcode != NULL)
		    {
		      g_free (PLUGIN (host_data)->htmlcode);
		      PLUGIN (host_data)->htmlcode = NULL;

		    }

		  PLUGIN (host_data)->htmlcode =
		    g_strdup_printf ("ERR_FETCH: %s", error->message);
		  g_error_free (error);
		}
	    }
	  else
	    {
	      PLUGIN (host_data)->htmlcode =
		g_strdup_printf ("ERR_UPLOAD: %s", curl_error);
	    }
	}
      G_UNLOCK (unload);
      g_unlink (path);
      g_free (path);
    }

  G_LOCK (unload);
  if (plugin->extra != NULL)
    PLUGIN (libcurl_thread) = NULL;
  G_UNLOCK (unload);
  return NULL;
}
/* #endif */

/*
 * Retrieve informations to post to a server.
 */
static void
xml_get_host_data_start_element_handler (GMarkupParseContext * context,
					 const gchar * element_name,
					 const gchar ** attribute_names,
					 const gchar ** attribute_values,
					 gpointer user_data, GError ** error)
{
  struct host_param_data *host_data = (struct host_param_data *) user_data;
  gint line_number, char_number;

  if (host_data->quit_handlers == TRUE)	/* don't parse anymore */
    return;

  g_markup_parse_context_get_position (context, &line_number, &char_number);

  /* found host in xml */
  if (!strcmp (element_name, "upload_hosts"))
    {
      return;
    }
  else if (!strcmp (element_name, "host") && !host_data->is_inside)
    {
      if (attribute_names[0])
	{
	  if (!strcmp (attribute_values[0], host_data->selected_hostname))
	    host_data->is_inside = TRUE;
	}
      else
	{
	  g_set_error (error,
		       G_MARKUP_ERROR,
		       G_MARKUP_ERROR_MISSING_ATTRIBUTE,
		       PLUGIN_PARSE_XML_ERRMSG_MISSATTR,
		       line_number, char_number, element_name);
	}
    }
  else if (!strcmp (element_name, "param"))
    {
      if (host_data->is_inside)
	{
	  if (attribute_names[0])
	    {
	      if (!strcmp (attribute_names[0], "form_action")
		  && !host_data->form_action)
		{
		  host_data->form_action = g_strdup (attribute_values[0]);
		}
	      else if (!strcmp (attribute_names[0], "file_input_name")
		       && !host_data->file_input_name)
		{
		  host_data->file_input_name = g_strdup (attribute_values[0]);
		}
	      else if (!strcmp (attribute_names[0], "regexp")
		       && !host_data->regexp)
		{
		  host_data->regexp = g_strdup (attribute_values[0]);
		}
	      else if (!strcmp (attribute_names[0], "name"))
		{
		  if (attribute_names[1]
		      && !strcmp (attribute_names[1], "value"))
		    {
		      gchar *extra_name = g_strdup (attribute_values[0]);
		      gchar *extra_value = g_strdup (attribute_values[1]);

		      g_array_append_val (host_data->extra_names, extra_name);
		      g_array_append_val (host_data->extra_values,
					  extra_value);
		    }
		  else
		    {
		      g_set_error (error,
				   G_MARKUP_ERROR,
				   G_MARKUP_ERROR_MISSING_ATTRIBUTE,
				   PLUGIN_PARSE_XML_ERRMSG_MISSATTRVAL,
				   line_number, char_number);
		    }
		}
	      else if (!strcmp (attribute_names[0], "location"))
		{
		  /* ignored, fixme */
		}
	      else
		{
		  g_set_error (error,
			       G_MARKUP_ERROR,
			       G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
			       PLUGIN_PARSE_XML_ERRMSG_INVATTR,
			       line_number, char_number,
			       attribute_names[0], element_name);
		}
	    }
	  else
	    {
	      g_set_error (error,
			   G_MARKUP_ERROR,
			   G_MARKUP_ERROR_MISSING_ATTRIBUTE,
			   PLUGIN_PARSE_XML_ERRMSG_MISSATTR,
			   line_number, char_number, element_name);
	    }
	}
    }
  else
    {
      g_set_error (error,
		   G_MARKUP_ERROR,
		   G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		   PLUGIN_PARSE_XML_ERRMSG_INVELEM,
		   line_number, char_number, element_name);
    }
}

static void
xml_get_host_data_end_element_handler (GMarkupParseContext * context,
				       const gchar * element_name,
				       gpointer user_data, GError ** error)
{
  struct host_param_data *host_data = (struct host_param_data *) user_data;

  if (host_data->quit_handlers)
    return;

  if (!strcmp (element_name, "host") && host_data->is_inside)
    {
      host_data->quit_handlers = TRUE;	/* don't parse anymore */
    }
  (void) context;
  (void) error;
}

GMarkupParser xml_get_host_data_parser = {
  xml_get_host_data_start_element_handler,
  xml_get_host_data_end_element_handler,
  NULL,
  NULL,
  NULL
};

static gboolean
insert_html_link_cb (PurplePlugin * plugin)
{
  /* still uploading... */
  if (PLUGIN (libcurl_thread) != NULL)
    {
      GtkProgressBar *progress_bar =
	g_object_get_data (G_OBJECT (PLUGIN (uploading_dialog)),
			   "progress-bar");
      gtk_progress_bar_pulse (progress_bar);
      return TRUE;
    }
  else
    {
      GError *error = NULL;

      PLUGIN (timeout_cb_handle) = 0;
      gtk_widget_destroy (PLUGIN (uploading_dialog));
      PLUGIN (uploading_dialog) = NULL;

      if (g_str_has_prefix (PLUGIN (host_data)->htmlcode, "ERR_UPLOAD:"))
	{
	  gchar *errmsg_uploadto = g_strdup_printf (PLUGIN_UPLOAD_ERROR,
						    PLUGIN
						    (host_data)->
						    selected_hostname);

	  NotifyError ("%s\n\n%s\n\n%s\n%s",
		       errmsg_uploadto,
		       PLUGIN (host_data)->htmlcode,
		       PLUGIN_UPLOAD_DISCLOC_ERROR,
		       PLUGIN (capture_path_filename));
	  g_free (errmsg_uploadto);
	}
      else if (g_str_has_prefix (PLUGIN (host_data)->htmlcode, "ERR_FETCH:"))
	{
	  gchar *errmsg_uploadto =
	    g_strdup_printf (PLUGIN_UPLOAD_FETCHURL_ERROR,
			     PLUGIN (host_data)->selected_hostname,
			     PLUGIN (host_data)->htmlcode);

	  NotifyError ("%s\n\n%s\n%s",
		       errmsg_uploadto,
		       PLUGIN_UPLOAD_DISCLOC_ERROR,
		       PLUGIN (capture_path_filename));

	  g_free (errmsg_uploadto);
	}
      else
	{
	  GRegex *url_regex = NULL;
	  GRegex *space_regex = NULL;

	  if ((url_regex =
	       g_regex_new (PLUGIN (host_data)->regexp,
			    G_REGEX_MULTILINE, 0, &error)) == NULL)
	    {

	      gchar *errmsg_uploadto =
		g_strdup_printf (PLUGIN_UPLOAD_FETCHURL_ERROR,
				 PLUGIN (host_data)->selected_hostname,
				 error->message);

	      NotifyError ("%s\n\n%s\n%s",
			   errmsg_uploadto,
			   PLUGIN_UPLOAD_DISCLOC_ERROR,
			   PLUGIN (capture_path_filename));

	      g_free (errmsg_uploadto);
	      g_error_free (error);
	    }
	  else
	    {
	      GMatchInfo *url_match_info = NULL;
	      gchar *nospace = NULL;

	      /* remove every newlines first */
	      space_regex = g_regex_new ("\\s", G_REGEX_MULTILINE, 0, NULL);
	      nospace = g_regex_replace_literal (space_regex,
						 PLUGIN (host_data)->htmlcode,
						 -1, 0, "", 0, NULL);
	      g_free (PLUGIN (host_data)->htmlcode);
	      PLUGIN (host_data)->htmlcode = NULL;
	      g_regex_unref (space_regex);
	      if (g_regex_match (url_regex, nospace, 0, &url_match_info) ==
		  FALSE)
		{
		  gchar *errmsg_uploadto;
		  if (url_match_info != NULL)
		    g_match_info_free (url_match_info);
		  g_regex_unref (url_regex);
		  g_free (nospace);

		  errmsg_uploadto =
		    g_strdup_printf (PLUGIN_UPLOAD_FETCHURL_ERROR,
				     PLUGIN (host_data)->selected_hostname,
				     PLUGIN_UPLOAD_REGEXP_NOTMACH_ERROR);

		  NotifyError ("%s\n\n%s\n%s",
			       errmsg_uploadto,
			       PLUGIN_UPLOAD_DISCLOC_ERROR,
			       PLUGIN (capture_path_filename));
		  g_free (errmsg_uploadto);
		}
	      else
		{
		  gchar *url = g_match_info_fetch (url_match_info, 1);

		  g_regex_unref (url_regex);
		  g_free (nospace);
		  g_match_info_free (url_match_info);
		  real_insert_link (plugin, url);
		  g_free (url);
		}
	    }
	}
    }
  CLEAR_HOST_PARAM_DATA_FULL (PLUGIN (host_data));
  CLEAR_PLUGIN_EXTRA_GCHARS (plugin);
  CLEAR_SEND_INFO_TO_NULL (plugin);
  PLUGIN (running) = FALSE;
  return FALSE;
}

void
http_upload_prepare (const gchar * capture_path_filename,
		     const gchar * capture_name, PurplePlugin * plugin)
{
  struct host_param_data *host_data;
  gsize length;
  GError *error = NULL;
  GMarkupParseContext *context;
  gchar *xml_contents = NULL;

  host_data = PLUGIN (host_data);
  host_data->selected_hostname =
    g_strdup (purple_prefs_get_string (PREF_UPLOAD_TO));

  if (!g_file_get_contents
      (PLUGIN (xml_hosts_filename), &xml_contents, &length, &error))
    {
      NotifyError (PLUGIN_LOAD_XML_ERROR, error->message, PLUGIN_WEBSITE);

      g_error_free (error);
      g_free (host_data->selected_hostname);
      host_data->selected_hostname = NULL;

      PLUGIN (running) = FALSE;
      return;
    }
  context =
    g_markup_parse_context_new (&xml_get_host_data_parser, 0, host_data,
				NULL);

  host_data->extra_names = g_array_new (FALSE, FALSE, sizeof (gchar *));
  host_data->extra_values = g_array_new (FALSE, FALSE, sizeof (gchar *));

  if (!g_markup_parse_context_parse (context, xml_contents, length, &error)
      || !g_markup_parse_context_end_parse (context, &error)
      || !host_data->form_action || !host_data->file_input_name
      || !host_data->regexp)
    {

      if (error != NULL)
	{
	  gchar *errmsg_parse;
	  gchar *errmsg_referto;

	  errmsg_parse = g_strdup_printf (PLUGIN_PARSE_XML_ERROR,
					  PLUGIN (xml_hosts_filename));

	  errmsg_referto = g_strdup_printf (PLUGIN_PLEASE_REFER_TO,
					    PLUGIN_WEBSITE);

	  NotifyError ("%s\n%s\n%s", errmsg_parse, error->message,
		       errmsg_referto);

	  g_free (errmsg_referto);
	  g_free (errmsg_parse);
	  g_error_free (error);

	}
      else if (!host_data->is_inside)
	{
	  gchar *errmsg_uploadto;
	  gchar *errmsg_referto;

	  errmsg_uploadto = g_strdup_printf (PLUGIN_XML_STUFF_MISSING,
					     PLUGIN
					     (host_data)->selected_hostname);
	  errmsg_referto =
	    g_strdup_printf (PLUGIN_PLEASE_REFER_TO, PLUGIN_WEBSITE);

	  NotifyError ("%s\n\t%s\n\n%s",
		       errmsg_uploadto,
		       PLUGIN_PARSE_XML_MISSING_HOST, errmsg_referto);

	  g_free (errmsg_uploadto);
	  g_free (errmsg_referto);

	}
      else if (!host_data->form_action)
	{
	  gchar *errmsg_uploadto;
	  gchar *errmsg_referto;

	  errmsg_uploadto = g_strdup_printf (PLUGIN_XML_STUFF_MISSING,
					     PLUGIN
					     (host_data)->selected_hostname);
	  errmsg_referto =
	    g_strdup_printf (PLUGIN_PLEASE_REFER_TO, PLUGIN_WEBSITE);

	  NotifyError ("%s\n\t%s\n\n%s",
		       errmsg_uploadto,
		       PLUGIN_PARSE_XML_MISSING_ACTION, errmsg_referto);

	  g_free (errmsg_uploadto);
	  g_free (errmsg_referto);

	}
      else if (!host_data->file_input_name)
	{
	  gchar *errmsg_uploadto;
	  char *errmsg_referto;

	  errmsg_uploadto = g_strdup_printf (PLUGIN_XML_STUFF_MISSING,
					     PLUGIN
					     (host_data)->selected_hostname);
	  errmsg_referto =
	    g_strdup_printf (PLUGIN_PLEASE_REFER_TO, PLUGIN_WEBSITE);

	  NotifyError ("%s\n\t%s\n\n%s",
		       errmsg_uploadto,
		       PLUGIN_PARSE_XML_MISSING_INPUT, errmsg_referto);

	  g_free (errmsg_uploadto);
	  g_free (errmsg_referto);

	}
      else if (!host_data->regexp)
	{
	  gchar *errmsg_uploadto;
	  gchar *errmsg_referto;

	  errmsg_uploadto = g_strdup_printf (PLUGIN_XML_STUFF_MISSING,
					     PLUGIN
					     (host_data)->selected_hostname);
	  errmsg_referto =
	    g_strdup_printf (PLUGIN_PLEASE_REFER_TO, PLUGIN_WEBSITE);

	  NotifyError ("%s\n\t%s\n\n%s",
		       errmsg_uploadto,
		       PLUGIN_PARSE_XML_MISSING_REGEXP, errmsg_referto);

	  g_free (errmsg_uploadto);
	  g_free (errmsg_referto);
	}

      g_markup_parse_context_free (context);
      g_free (xml_contents);
      CLEAR_HOST_PARAM_DATA_FULL (host_data);
      PLUGIN (running) = FALSE;
      return;
    }
  g_markup_parse_context_free (context);
  g_free (xml_contents);

  PLUGIN (capture_name) = g_strdup (capture_name);
  PLUGIN (capture_path_filename) = g_strdup (capture_path_filename);

  /* upload to server */
  PLUGIN (uploading_dialog) =
    show_uploading_dialog (plugin, PLUGIN (host_data)->selected_hostname);
  PLUGIN (libcurl_thread) =
    g_thread_create ((GThreadFunc) http_upload, plugin, FALSE, NULL);
  PLUGIN (timeout_cb_handle) =
    g_timeout_add (50, (GSourceFunc) insert_html_link_cb, plugin);
}

void on_screenshot_insert_as_link_activate_cb (PidginConversation * gtkconv)
{
  PurplePlugin *plugin;

  plugin = purple_plugins_find_with_id (PLUGIN_ID);

  TRY_SET_RUNNING_ON (plugin);

  PLUGIN (send_as) = SEND_AS_HTTP_LINK;

  if (!strcmp (purple_prefs_get_string (PREF_UPLOAD_TO), HOST_DISABLED))
    {
      purple_notify_error (plugin, PLUGIN_NAME, PLUGIN_ERROR,
			   PLUGIN_HOST_DISABLED_ERROR);
      PLUGIN (running) = FALSE;
      return;
    }
  REMEMBER_ACCOUNT (gtkconv);

  PLUGIN (conv_features) = gtkconv->active_conv->features;
  FREEZE_DESKTOP ();
}

void
  on_screenshot_insert_as_link_show_cb (GtkWidget *
					as_link, PidginConversation * gtkconv)
{
  PurpleConversation *conv = gtkconv->active_conv;

  /*
   * Depending on which protocol the conv is associated with,
   * html is supported or not...
   */
#if GTK_CHECK_VERSION(2,16,0)
  if (purple_conversation_get_features (conv) & PURPLE_CONNECTION_NO_IMAGES)
    gtk_menu_item_set_label (GTK_MENU_ITEM (as_link), SEND_AS_HTML_LINK_TXT);
  else
    gtk_menu_item_set_label (GTK_MENU_ITEM (as_link),
			     SEND_AS_HTML_URL_TXT);
#else
  if (purple_conversation_get_features (conv) & PURPLE_CONNECTION_NO_IMAGES)
    gtk_label_set_label (GTK_LABEL (GTK_BIN (as_link)->child),
			 SEND_AS_HTML_LINK_TXT);
  else
    gtk_label_set_label (GTK_LABEL (GTK_BIN (as_link)->child),
			 SEND_AS_HTML_URL_TXT);
#endif
}

/* end of http_upload.c*/