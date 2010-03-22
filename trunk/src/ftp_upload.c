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

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef HAVE_LIBCURL

G_LOCK_DEFINE (unload);

/* read_callback() and ftp_upload() funcs (and comments) are stolen
   from curl/docs/examples/ftpupload.c 


   NOTE: if you want this example to work on Windows with libcurl as a
   DLL, you MUST also provide a read callback with CURLOPT_READFUNCTION.
   Failing to do so will give you a crash since a DLL may not use the
   variable's memory when passed in to it from an app like this. */
static size_t
read_callback (void *ptr, size_t size, size_t nmemb, void *stream)
{
  /* in real-world cases, this would probably get this data differently
     as this fread() stuff is exactly what the library already would do
     by default internally */
  size_t retcode = fread (ptr, size, nmemb, stream);
  return retcode;
}

static gpointer
ftp_upload (PurplePlugin * plugin)
{

  CURL *curl;
  CURLcode res;
  FILE *hd_src;
  struct stat file_info;
  curl_off_t fsize;
  struct curl_slist *headerlist = NULL;
  char *local_file, *remote_url;

  local_file = g_strdup_printf ("RNFR %s", PLUGIN (capture_name));
  remote_url =
    g_strdup_printf ("%s/%s",
		     purple_prefs_get_string (PREF_FTP_REMOTE_URL),
		     PLUGIN (capture_name));

  /* get the file size of the local file */
  if (stat (PLUGIN (capture_path_filename), &file_info))
    {
      printf ("Couldnt open '%s'\n", PLUGIN (capture_path_filename));
    }
  fsize = (curl_off_t) file_info.st_size;

  /* get a FILE * of the same file */
  hd_src = fopen (PLUGIN (capture_path_filename), "rb");

  /* get a curl handle */
  curl = curl_easy_init ();
  if (curl != NULL)
    {
      static char curl_error[CURL_ERROR_SIZE];

      /* build a list of commands to pass to libcurl */
      headerlist = curl_slist_append (headerlist, local_file);

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
      curl_easy_setopt (curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) fsize);

      curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);

      /* Now run off and do what you've been told! */
      res = curl_easy_perform (curl);

      /* clean up the FTP commands list */
      curl_slist_free_all (headerlist);

      /* always cleanup */
      curl_easy_cleanup (curl);

      if (res != 0)
	{
	  G_LOCK (unload);
	  PLUGIN (host_data)->htmlcode = g_strdup_printf ("%s", curl_error);
	  G_UNLOCK (unload);
	}
    }
  fclose (hd_src);		/* close the local file */
  g_free (local_file);
  g_free (remote_url);

  G_LOCK (unload);
  PLUGIN (libcurl_thread) = NULL;
  G_UNLOCK (unload);
  return NULL;
}


#endif /* HAVE_LIBCURL */

gboolean
insert_ftp_link_cb (PurplePlugin * plugin)
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

	  if (strcmp (purple_prefs_get_string (PREF_FTP_WEB_ADDR), ""))
	    /* not only a ftp server, but also a html server */
	    remote_url = g_strdup_printf ("%s/%s",
					  purple_prefs_get_string
					  (PREF_FTP_WEB_ADDR),
					  PLUGIN (capture_name));
	  else
	    remote_url = g_strdup_printf ("%s/%s",
					  purple_prefs_get_string
					  (PREF_FTP_REMOTE_URL),
					  PLUGIN (capture_name));

	  real_insert_link (plugin, remote_url);
	  g_free (remote_url);
	}
    }

  CLEAR_PLUGIN_EXTRA_GCHARS (plugin);
  CLEAR_SEND_INFO_TO_NULL (plugin);
  PLUGIN (running) = FALSE;
  return FALSE;
}

void
ftp_upload_prepare (PurplePlugin * plugin)
{
  struct host_param_data *host_data;

  host_data = PLUGIN (host_data);

#ifdef HAVE_LIBCURL
  PLUGIN (uploading_dialog) =
    show_uploading_dialog (plugin,
			   purple_prefs_get_string (PREF_FTP_REMOTE_URL));
  PLUGIN (libcurl_thread) =
    g_thread_create ((GThreadFunc) ftp_upload, plugin, FALSE, NULL);
  PLUGIN (timeout_cb_handle) =
    g_timeout_add (50, (GSourceFunc) insert_ftp_link_cb, plugin);

#endif
}

/* end of ftp_upload.c */
