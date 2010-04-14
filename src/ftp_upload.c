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
read_callback (void *ptr, size_t size, size_t nmemb, void *stream)
{
  PurplePlugin *plugin;
  size_t ret;

  plugin = purple_plugins_find_with_id (PLUGIN_ID);

  ret = fread (ptr, size, nmemb, stream);

  PLUGIN (read_size) += ret * size;	/* progress bar */
  return ret;
}

static gpointer
ftp_upload (PurplePlugin * plugin)
{
  CURL *curl;
  CURLcode res;
  FILE *hd_src;
  struct stat file_info;
  struct curl_slist *headerlist = NULL;
  gchar *local_file = NULL;
  gchar *remote_url = NULL;
  gchar *basename = NULL;

  G_LOCK (unload);
  /* get the file size of the local file */
  if (g_stat (PLUGIN (capture_path_filename), &file_info))
    {
      NotifyError ("Couldnt open '%s'\n", PLUGIN (capture_path_filename));
      return NULL;
    }
  PLUGIN (total_size) = file_info.st_size;

  basename = g_path_get_basename (PLUGIN (capture_path_filename));
  local_file = g_strdup_printf ("RNFR %s", basename);
  remote_url =
    g_strdup_printf ("%s/%s",
		     purple_prefs_get_string (PREF_FTP_REMOTE_URL), basename);

  g_free (basename);
  basename = NULL;

  /* get a FILE * of the same file */
  hd_src = fopen (PLUGIN (capture_path_filename), "rb");

  /* get a curl handle */
  curl = curl_easy_init ();
  if (curl != NULL)
    {
      static char curl_error[CURL_ERROR_SIZE];

      /* build a list of commands to pass to libcurl */
      headerlist = curl_slist_append (headerlist, local_file);

      plugin_curl_set_common_opts (curl, plugin);

      /* we want to use our own read function */
      curl_easy_setopt (curl, CURLOPT_READFUNCTION, read_callback);

      /* enable uploading */
      curl_easy_setopt (curl, CURLOPT_UPLOAD, 1L);

      /* specify target */
      curl_easy_setopt (curl, CURLOPT_URL, remote_url);

      /* specify username and password */
      curl_easy_setopt (curl, CURLOPT_USERNAME,
			purple_prefs_get_string (PREF_FTP_USERNAME));

      curl_easy_setopt (curl, CURLOPT_PASSWORD,
			purple_prefs_get_string (PREF_FTP_PASSWORD));


      /* pass in that last of FTP commands to run after the transfer */
      curl_easy_setopt (curl, CURLOPT_POSTQUOTE, headerlist);

      /* now specify which file to upload */
      curl_easy_setopt (curl, CURLOPT_READDATA, hd_src);

      /* Set the size of the file to upload (optional).  If you give a *_LARGE
         option you MUST make sure that the type of the passed-in argument is a
         curl_off_t. If you use CURLOPT_INFILESIZE (without _LARGE) you must
         make sure that to pass in a type 'long' argument. */
      curl_easy_setopt (curl, CURLOPT_INFILESIZE_LARGE,
			(curl_off_t) PLUGIN (total_size));

      curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);

      /* Now run off and do what you've been told! */
      res = curl_easy_perform (curl);

      PLUGIN (read_size) = 0;

      /* clean up the FTP commands list */
      curl_slist_free_all (headerlist);

      /* always cleanup */
      curl_easy_cleanup (curl);

      if (res != 0)
	PLUGIN (host_data)->htmlcode = g_strdup_printf ("%s", curl_error);
    }
  fclose (hd_src);		/* close the local file */
  g_free (local_file);
  g_free (remote_url);

  PLUGIN (libcurl_thread) = NULL;
  G_UNLOCK (unload);
  return NULL;
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

      if (PLUGIN (host_data)->htmlcode != NULL)
	{
	  NotifyError ("%s\n\n%s\n",
		       PLUGIN_FTP_UPLOAD_ERROR, PLUGIN (host_data)->htmlcode);
	  g_free (PLUGIN (host_data)->htmlcode);
	  PLUGIN (host_data)->htmlcode = NULL;
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
    g_timeout_add (500, (GSourceFunc) insert_ftp_link_cb, plugin);
}

/* end of ftp_upload.c */
