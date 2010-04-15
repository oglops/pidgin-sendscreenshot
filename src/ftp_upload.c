 /*
  * Pidgin SendScreenshot plugin - ftp upload  -
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

#include "ftp_upload.h"
#include "prefs.h"
#include "upload_utils.h"

G_LOCK_DEFINE (unload);

static size_t
read_callback (void *buf, size_t size, size_t nmemb, void *stream)
{
  PurplePlugin *plugin;
  GIOChannel *io_chan;
  GError *error = NULL;
  size_t ret;

  plugin = purple_plugins_find_with_id (PLUGIN_ID);
  io_chan = (GIOChannel*) stream;

  g_io_channel_read_chars (io_chan, buf, 
			   size*nmemb,
			   &ret,
			   &error);
   
  if (error != NULL) {
    PLUGIN (error_message) = g_strdup_printf (PLUGIN_UNEXPECTED_ERROR);
    purple_debug_error (PLUGIN_ID, "g_io_channel_read_chars : %s\n", error->message);
    g_error_free (error);
    return CURL_READFUNC_ABORT;
  }
  
  PLUGIN (read_size) += ret;	/* progress bar */
  return ret;
}

#define THREAD_QUIT\
  PLUGIN (libcurl_thread) = NULL;\
  G_UNLOCK (unload);\
  return NULL

/* inspired from http://curl.haxx.se/libcurl/c/ftpupload.html */
static gpointer
ftp_upload (PurplePlugin * plugin)
{
  CURL *curl;
  CURLcode res;
  GIOChannel *io_chan;
  GError *error = NULL;

  struct stat file_info;
  gchar *remote_url = NULL;
  gchar *basename = NULL;

  G_LOCK (unload);
  /* get the file size of the local file */
  if (g_stat (PLUGIN (capture_path_filename), &file_info) == -1)
    {
      PLUGIN (error_message) = g_strdup_printf (PLUGIN_UNEXPECTED_ERROR);
      purple_debug_error (PLUGIN_ID, "Couldnt open '%s'\n", /* FIXME */
			  PLUGIN (capture_path_filename));
      THREAD_QUIT;
    }
  PLUGIN (total_size) = file_info.st_size;

  basename = g_path_get_basename (PLUGIN (capture_path_filename));
  remote_url =
    g_strdup_printf ("%s/%s",
		     purple_prefs_get_string (PREF_FTP_REMOTE_URL), basename);

  g_free (basename);
  basename = NULL;

  io_chan = 
    g_io_channel_new_file (PLUGIN (capture_path_filename), "r", &error);
  if (error != NULL) {
    PLUGIN (error_message) = g_strdup_printf (PLUGIN_UNEXPECTED_ERROR);
    purple_debug_error (PLUGIN_ID, "g_io_channel_new_file (%s) : %s\n",
			PLUGIN (capture_path_filename),
			error->message);
    g_error_free (error);
    THREAD_QUIT;
  }
  /* binary data, this should never fail */
  g_io_channel_set_encoding (io_chan, NULL, NULL); 

  /* get a curl handle */
  curl = curl_easy_init ();
  if (curl != NULL)
    {
      static char curl_error[CURL_ERROR_SIZE];
 
      plugin_curl_set_common_opts (curl, plugin);

      /* we want to use our own read function */
      curl_easy_setopt (curl, CURLOPT_READFUNCTION, read_callback);
      curl_easy_setopt (curl, CURLOPT_READDATA, io_chan);

      /* enable uploading */
      curl_easy_setopt (curl, CURLOPT_UPLOAD, 1L);

      /* specify target */
      curl_easy_setopt (curl, CURLOPT_URL, remote_url);

      /* specify username and password */
      curl_easy_setopt (curl, CURLOPT_USERNAME,
			purple_prefs_get_string (PREF_FTP_USERNAME));

      curl_easy_setopt (curl, CURLOPT_PASSWORD,
			purple_prefs_get_string (PREF_FTP_PASSWORD));

      curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);

      /* Now run off and do what you've been told! */
      res = curl_easy_perform (curl);

      PLUGIN (read_size) = 0;

      /* always cleanup */
      curl_easy_cleanup (curl);

      if (res != 0) {
	g_assert (PLUGIN (error_message) == NULL);
	PLUGIN (error_message) = g_strdup_printf ("%s", curl_error);
      }
    }
 
  g_io_channel_shutdown (io_chan, TRUE, &error);
  if (error != NULL) {
    /* Don't set PLUGIN (error_message) */
    purple_debug_error (PLUGIN_ID, "g_io_channel_shutdown : %s\n", error->message);
    g_error_free (error);
    THREAD_QUIT;
  }
  g_free (remote_url);

  THREAD_QUIT;
}

static gboolean
insert_ftp_link_cb (PurplePlugin * plugin)
{
  /* still uploading... */
  if (PLUGIN (libcurl_thread) != NULL)
    {
      GtkProgressBar *progress_bar =
	g_object_get_data (G_OBJECT (PLUGIN (uploading_dialog)),
			   "progress-bar");
      if (PLUGIN (read_size) == 0)
	gtk_progress_bar_pulse (progress_bar);
      else
	gtk_progress_bar_set_fraction (progress_bar,
				       PLUGIN (read_size) /
				       (gdouble) PLUGIN (total_size));
      return TRUE;
    }
  else
    {
      PLUGIN (timeout_cb_handle) = 0;
      gtk_widget_destroy (PLUGIN (uploading_dialog));
      PLUGIN (uploading_dialog) = NULL;

      if (PLUGIN (error_message) != NULL)
	{
	  NotifyError ("%s\n\n%s\n",
		       PLUGIN_FTP_UPLOAD_ERROR, PLUGIN (error_message));
	  g_free (PLUGIN (error_message));
	  PLUGIN (error_message) = NULL;
	}
      else
	{
	  gchar *remote_url;
	  gchar *basename;

	  basename = g_path_get_basename (PLUGIN (capture_path_filename));

	  if (strcmp (purple_prefs_get_string (PREF_FTP_WEB_ADDR), ""))
	    /* not only a ftp server, but also a html server */
	    remote_url = g_strdup_printf ("%s/%s",
					  purple_prefs_get_string
					  (PREF_FTP_WEB_ADDR), basename);
	  else
	    remote_url = g_strdup_printf ("%s/%s",
					  purple_prefs_get_string
					  (PREF_FTP_REMOTE_URL), basename);

	  real_insert_link (plugin, remote_url);
	  g_free (remote_url);
	  g_free (basename);
	}
    }
  plugin_stop (plugin);
  return FALSE;
}

void
ftp_upload_prepare (PurplePlugin * plugin)
{
  struct host_param_data *host_data;

  host_data = PLUGIN (host_data);

  g_assert (PLUGIN (uploading_dialog) == NULL);
  g_assert (PLUGIN (libcurl_thread) == NULL);

  PLUGIN (read_size) = 0;

  PLUGIN (uploading_dialog) =
    show_uploading_dialog (plugin,
			   purple_prefs_get_string (PREF_FTP_REMOTE_URL));
  PLUGIN (libcurl_thread) =
    g_thread_create ((GThreadFunc) ftp_upload, plugin, FALSE, NULL);

  PLUGIN (timeout_cb_handle) =
    g_timeout_add (PLUGIN_UPLOAD_PROGRESS_INTERVAL,
		   (GSourceFunc) insert_ftp_link_cb, plugin);
}

/* end of ftp_upload.c */
