/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The Sushi project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Sushi. This
 * permission is above and beyond the permissions granted by the GPL license
 * Sushi is covered by.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "sushi-cover-art.h"

#include <musicbrainz3/mb_c.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_DEFINE_TYPE (SushiCoverArtFetcher, sushi_cover_art_fetcher, G_TYPE_OBJECT);

#define SUSHI_COVER_ART_FETCHER_GET_PRIVATE(obj)\
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SUSHI_TYPE_COVER_ART_FETCHER, SushiCoverArtFetcherPrivate))

enum {
  PROP_COVER = 1,
  PROP_TAGLIST,
};

struct _SushiCoverArtFetcherPrivate {
  GdkPixbuf *cover;
  GstTagList *taglist;
};

#define AMAZON_IMAGE_FORMAT "http://images.amazon.com/images/P/%s.01.LZZZZZZZ.jpg"

static void sushi_cover_art_fetcher_set_taglist (SushiCoverArtFetcher *self,
                                                 GstTagList *taglist);
static void sushi_cover_art_fetcher_get_uri_for_track_async (SushiCoverArtFetcher *self,
                                                             const gchar *artist,
                                                             const gchar *album,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

static void
sushi_cover_art_fetcher_dispose (GObject *object)
{
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (object);

  g_clear_object (&priv->cover);

  if (priv->taglist != NULL) {
    gst_tag_list_free (priv->taglist);
    priv->taglist = NULL;
  }

  G_OBJECT_CLASS (sushi_cover_art_fetcher_parent_class)->dispose (object);
}

static void
sushi_cover_art_fetcher_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  SushiCoverArtFetcher *self = SUSHI_COVER_ART_FETCHER (gobject);
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);

  switch (prop_id) {
  case PROP_COVER:
    g_value_set_object (value, priv->cover);
    break;
  case PROP_TAGLIST:
    g_value_set_boxed (value, priv->taglist);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    break;
  }
}

static void
sushi_cover_art_fetcher_set_property (GObject    *gobject,
                                      guint       prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
  SushiCoverArtFetcher *self = SUSHI_COVER_ART_FETCHER (gobject);
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);

  switch (prop_id) {
  case PROP_TAGLIST:
    sushi_cover_art_fetcher_set_taglist (self, g_value_get_boxed (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    break;
  }
}

static void
sushi_cover_art_fetcher_init (SushiCoverArtFetcher *self)
{
  self->priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);
}

static void
sushi_cover_art_fetcher_class_init (SushiCoverArtFetcherClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = sushi_cover_art_fetcher_get_property;
  gobject_class->set_property = sushi_cover_art_fetcher_set_property;
  gobject_class->dispose = sushi_cover_art_fetcher_dispose;

  g_object_class_install_property
    (gobject_class,
     PROP_COVER,
     g_param_spec_object ("cover",
                          "Cover art",
                          "Cover art for the current attrs",
                          GDK_TYPE_PIXBUF,
                          G_PARAM_READABLE));

  g_object_class_install_property
    (gobject_class,
     PROP_TAGLIST,
     g_param_spec_boxed ("taglist",
                         "Taglist",
                         "Current file tags",
                          GST_TYPE_TAG_LIST,
                          G_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (SushiCoverArtFetcherPrivate));
}

typedef struct {
  SushiCoverArtFetcher *self;
  GSimpleAsyncResult *result;
  gchar *artist;
  gchar *album;
} FetchUriJob;

static void
fetch_uri_job_free (gpointer user_data)
{
  FetchUriJob *data = user_data;

  g_clear_object (&data->self);
  g_clear_object (&data->result);
  g_free (data->artist);
  g_free (data->album);

  g_slice_free (FetchUriJob, data);
}

static FetchUriJob *
fetch_uri_job_new (SushiCoverArtFetcher *self,
                   const gchar *artist,
                   const gchar *album,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
  FetchUriJob *retval;

  retval = g_slice_new0 (FetchUriJob);
  retval->artist = g_strdup (artist);
  retval->album = g_strdup (album);
  retval->self = g_object_ref (self);
  retval->result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
                                              sushi_cover_art_fetcher_get_uri_for_track_async);

  return retval;
}

static gboolean
fetch_uri_job_callback (gpointer user_data)
{
  FetchUriJob *job = user_data;

  g_simple_async_result_complete (job->result);
  fetch_uri_job_free (job);

  return FALSE;
}

static gboolean
fetch_uri_job (GIOSchedulerJob *sched_job,
               GCancellable *cancellable,
               gpointer user_data)
{
  FetchUriJob *job = user_data;
  MbQuery query;
  MbReleaseFilter filter;
  MbRelease release;
  MbResultList results;
  gint results_len, idx;
  gchar *retval = NULL;

  query = mb_query_new (NULL, NULL);

  filter = mb_release_filter_new ();
  filter = mb_release_filter_title (filter, job->album);
  filter = mb_release_filter_artist_name (filter, job->artist);

  results = mb_query_get_releases (query, filter);
  results_len = mb_result_list_get_size (results);

  for (idx = 0; idx < results_len; idx++) {
    gchar asin[255];

    release = mb_result_list_get_release (results, idx);
    mb_release_get_asin (release, asin, 255);

    if (asin != NULL &&
        asin[0] != '\0') {
      retval = g_strdup_printf (AMAZON_IMAGE_FORMAT, asin);
      break;
    }
  }

  if (retval == NULL) {
    /* FIXME: do we need a better error? */
    g_simple_async_result_set_error (job->result,
                                     G_IO_ERROR,
                                     0, "%s",
                                     "Error getting the ASIN from MusicBrainz");
  } else {
    g_simple_async_result_set_op_res_gpointer (job->result,
                                               retval, NULL);
  }

  g_io_scheduler_job_send_to_mainloop_async (sched_job,
                                             fetch_uri_job_callback,
                                             job, NULL);

  return FALSE;
}

static gchar *
sushi_cover_art_fetcher_get_uri_for_track_finish (SushiCoverArtFetcher *self,
                                                  GAsyncResult *result,
                                                  GError **error)
{
  gchar *retval;

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                             error))
    return NULL;

  retval = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));

  return retval;
}

static void
sushi_cover_art_fetcher_get_uri_for_track_async (SushiCoverArtFetcher *self,
                                                 const gchar *artist,
                                                 const gchar *album,
                                                 GAsyncReadyCallback callback,
                                                 gpointer user_data)
{
  GSimpleAsyncResult *result;
  FetchUriJob *job;
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);

  job = fetch_uri_job_new (self, artist, album, callback, user_data);
  g_io_scheduler_push_job (fetch_uri_job,
                           job, NULL,
                           G_PRIORITY_DEFAULT, NULL);
}

static void
pixbuf_from_stream_async_cb (GObject *source,
                             GAsyncResult *res,
                             gpointer user_data)
{
  SushiCoverArtFetcher *self = user_data;
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);
  GError *error = NULL;
  GdkPixbuf *pix;

  pix = gdk_pixbuf_new_from_stream_finish (res, &error);

  if (error != NULL) {
    g_print ("Unable to fetch Amazon cover art: %s\n", error->message);
    g_error_free (error);
    return;
  }

  priv->cover = pix;
  g_object_notify (G_OBJECT (self), "cover");
}

static void
asin_uri_read_cb (GObject *source,
                  GAsyncResult *res,
                  gpointer user_data)
{
  SushiCoverArtFetcher *self = user_data;
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);
  GFileInputStream *stream;
  GError *error = NULL;

  stream = g_file_read_finish (G_FILE (source),
                               res, &error);

  if (error != NULL) {
    g_print ("Unable to fetch Amazon cover art: %s\n", error->message);
    g_error_free (error);
    return;
  }

  gdk_pixbuf_new_from_stream_async (G_INPUT_STREAM (stream), NULL,
                                    pixbuf_from_stream_async_cb, self);

  g_object_unref (stream);
}

static void
amazon_cover_uri_async_ready_cb (GObject *source,
                                 GAsyncResult *res,
                                 gpointer user_data)
{
  SushiCoverArtFetcher *self = SUSHI_COVER_ART_FETCHER (source);
  GError *error = NULL;
  gchar *asin;
  GFile *file;

  asin = sushi_cover_art_fetcher_get_uri_for_track_finish
    (self, res, &error);

  if (error != NULL) {
    g_print ("Unable to fetch the Amazon cover art uri from MusicBrainz: %s\n",
             error->message);
    g_error_free (error);

    return;
  }

  file = g_file_new_for_uri (asin);
  g_file_read_async (file, G_PRIORITY_DEFAULT,
                     NULL, asin_uri_read_cb,
                     self);

  g_object_unref (file);
  g_free (asin);
}

static void
try_fetch_from_amazon (SushiCoverArtFetcher *self)
{
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);
  gchar *artist = NULL;
  gchar *album = NULL;
  GFile *file;

  gst_tag_list_get_string (priv->taglist,
                           GST_TAG_ARTIST, &artist);
  gst_tag_list_get_string (priv->taglist,
                           GST_TAG_ALBUM, &album);

  if (artist == NULL &&
      album == NULL) {
    /* don't even try */
    return;
  }

  sushi_cover_art_fetcher_get_uri_for_track_async
    (self, artist, album,
     amazon_cover_uri_async_ready_cb, NULL);

  g_free (artist);
  g_free (album);
}

/* code taken from Totem; totem-gst-helpers.c
 *
 * Copyright (C) 2003-2007 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2005-2008 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright © 2009 Christian Persch
 *
 * License: GPLv2+ with exception clause, see COPYING
 *
 */
static GdkPixbuf *
totem_gst_buffer_to_pixbuf (GstBuffer *buffer)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;

  loader = gdk_pixbuf_loader_new ();

  if (gdk_pixbuf_loader_write (loader, buffer->data, buffer->size, &err) &&
      gdk_pixbuf_loader_close (loader, &err)) {
    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
    if (pixbuf)
      g_object_ref (pixbuf);
  } else {
    g_warning ("could not convert tag image to pixbuf: %s", err->message);
    g_error_free (err);
  }

  g_object_unref (loader);
  return pixbuf;
}

static const GValue *
totem_gst_tag_list_get_cover_real (GstTagList *tag_list)
{
  const GValue *cover_value = NULL;
  guint i;

  for (i = 0; ; i++) {
    const GValue *value;
    GstBuffer *buffer;
    GstStructure *caps_struct;
    int type;

    value = gst_tag_list_get_value_index (tag_list,
					  GST_TAG_IMAGE,
					  i);
    if (value == NULL)
      break;

    buffer = gst_value_get_buffer (value);

    caps_struct = gst_caps_get_structure (buffer->caps, 0);
    gst_structure_get_enum (caps_struct,
			    "image-type",
			    GST_TYPE_TAG_IMAGE_TYPE,
			    &type);
    if (type == GST_TAG_IMAGE_TYPE_UNDEFINED) {
      if (cover_value == NULL)
        cover_value = value;
    } else if (type == GST_TAG_IMAGE_TYPE_FRONT_COVER) {
      cover_value = value;
      break;
    }
  }

  return cover_value;
}

GdkPixbuf *
totem_gst_tag_list_get_cover (GstTagList *tag_list)
{
  const GValue *cover_value;

  g_return_val_if_fail (tag_list != NULL, FALSE);

  cover_value = totem_gst_tag_list_get_cover_real (tag_list);
  /* Fallback to preview */
  if (!cover_value) {
    cover_value = gst_tag_list_get_value_index (tag_list,
						GST_TAG_PREVIEW_IMAGE,
						0);
  }

  if (cover_value) {
    GstBuffer *buffer;
    GdkPixbuf *pixbuf;

    buffer = gst_value_get_buffer (cover_value);
    pixbuf = totem_gst_buffer_to_pixbuf (buffer);
    return pixbuf;
  }

  return NULL;
}
/* */
static void
try_fetch_from_tags (SushiCoverArtFetcher *self)
{
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);

  if (priv->taglist == NULL)
    return;

  if (priv->cover != NULL)
    g_clear_object (&priv->cover);

  priv->cover = totem_gst_tag_list_get_cover (priv->taglist);

  if (priv->cover != NULL)
    g_object_notify (G_OBJECT (self), "cover");
  else
    try_fetch_from_amazon (self);
}

static void
sushi_cover_art_fetcher_set_taglist (SushiCoverArtFetcher *self,
                                     GstTagList *taglist)
{
  SushiCoverArtFetcherPrivate *priv = SUSHI_COVER_ART_FETCHER_GET_PRIVATE (self);

  g_clear_object (&priv->cover);

  if (priv->taglist != NULL) {
    gst_tag_list_free (priv->taglist);
    priv->taglist = NULL;
  }

  priv->taglist = gst_tag_list_copy (taglist);
  try_fetch_from_tags (self);
}

SushiCoverArtFetcher *
sushi_cover_art_fetcher_new (GstTagList *taglist)
{
  return g_object_new (SUSHI_TYPE_COVER_ART_FETCHER,
                       "taglist", taglist,
                       NULL);
}
