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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * The Sushi project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Sushi. This
 * permission is above and beyond the permissions granted by the GPL license
 * Sushi is covered by.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */
imports.gi.versions.Gdk = '3.0';
imports.gi.versions.Gtk = '3.0';

const Gdk = imports.gi.Gdk;
const GdkPixbuf = imports.gi.GdkPixbuf;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;

const Gettext = imports.gettext.domain('sushi');
const _ = Gettext.gettext;
const Lang = imports.lang;

const MimeHandler = imports.ui.mimeHandler;
const Utils = imports.ui.utils;

const Image = new Lang.Class({
    Name: 'Image',
    Extends: Gtk.DrawingArea,
    Properties: {
        'pix': GObject.ParamSpec.object('pix', '', '',
                                        GObject.ParamFlags.READWRITE,
                                        GdkPixbuf.Pixbuf)
    },

    _init: function() {
        this._pix = null;
        this._scaledSurface = null;

        this.parent();
    },

    _ensureScaledPix: function() {
        let scaleFactor = this.get_scale_factor();
        let width = this.get_allocated_width() * scaleFactor;
        let height = this.get_allocated_height() * scaleFactor;

        // Downscale original to fit, if necessary
        let origWidth = this._pix.get_width();
        let origHeight = this._pix.get_height();

        let scaleX = origWidth / width;
        let scaleY = origHeight / height;
        let scale = Math.max(scaleX, scaleY);

        let newWidth;
        let newHeight;

        if (scale < 1) {
            newWidth = Math.min(width, origWidth * scaleFactor);
            newHeight = Math.min(height, origHeight * scaleFactor);
        } else {
            newWidth = Math.floor(origWidth / scale);
            newHeight = Math.floor(origHeight / scale);
        }

        let scaledWidth = this._scaledSurface ? this._scaledSurface.getWidth() : 0;
        let scaledHeight = this._scaledSurface ? this._scaledSurface.getHeight() : 0;

        if (newWidth != scaledWidth || newHeight != scaledHeight) {
            let scaledPixbuf = this._pix.scale_simple(newWidth, newHeight,
                                                      GdkPixbuf.InterpType.BILINEAR);
            this._scaledSurface = Gdk.cairo_surface_create_from_pixbuf(scaledPixbuf,
                                                                       scaleFactor,
                                                                       this.get_window());
        }
    },

    vfunc_get_preferred_width: function() {
        return [1, this._pix ? this.pix.get_width() : 1];
    },

    vfunc_get_preferred_height: function() {
        return [1, this._pix ? this.pix.get_height() : 1];
    },

    vfunc_size_allocate: function(allocation) {
        this.parent(allocation);
        this._ensureScaledPix();
    },

    vfunc_draw: function(context) {
        let width = this.get_allocated_width();
        let height = this.get_allocated_height();

        let scaleFactor = this.get_scale_factor();
        let offsetX = (width - this._scaledSurface.getWidth() / scaleFactor) / 2;
        let offsetY = (height - this._scaledSurface.getHeight() / scaleFactor) / 2;

        context.setSourceSurface(this._scaledSurface, offsetX, offsetY);
        context.paint();
        return false;
    },

    set pix(p) {
        this._pix = p;
        this._scaledSurface = null;
        this.queue_resize();
    },

    get pix() {
        return this._pix;
    }
});

const ImageRenderer = new Lang.Class({
    Name: 'ImageRenderer',

    _init : function(args) {
        this.moveOnClick = true;
        this.canFullScreen = true;
    },

    prepare : function(file, mainWindow, callback) {
        this._mainWindow = mainWindow;
        this._file = file;
        this._callback = callback;

        this._createImageTexture(file);
    },

    render : function() {
        return this._texture;
    },

    _createImageTexture : function(file) {
        this._texture = new Image();

        file.read_async
        (GLib.PRIORITY_DEFAULT, null,
         Lang.bind(this,
                   function(obj, res) {
                       try {
                           let stream = obj.read_finish(res);
                           this._textureFromStream(stream);
                       } catch (e) {
                       }
                   }));
    },

    _textureFromStream : function(stream) {
        GdkPixbuf.Pixbuf.new_from_stream_async
        (stream, null,
         Lang.bind(this, function(obj, res) {
             let pix = GdkPixbuf.Pixbuf.new_from_stream_finish(res);
             pix = pix.apply_embedded_orientation();
             this._texture.pix = pix;

             /* we're ready now */
             this._callback();

             stream.close_async(GLib.PRIORITY_DEFAULT,
                                null, function(object, res) {
                                    try {
                                        object.close_finish(res);
                                    } catch (e) {
                                        log('Unable to close the stream ' + e.toString());
                                    }
                                });
         }));
    },

    getSizeForAllocation : function(allocation) {
        let width = this._texture.pix.get_width();
        let height = this._texture.pix.get_height();
        return Utils.getScaledSize([width, height], allocation, false);
    },

    createToolbar : function() {
        this._mainToolbar = new Gtk.Toolbar({ icon_size: Gtk.IconSize.MENU,
                                              halign: Gtk.Align.CENTER });
        this._mainToolbar.get_style_context().add_class('osd');
        this._mainToolbar.set_show_arrow(false);
        this._mainToolbar.show();

        this._toolbarZoom = Utils.createFullScreenButton(this._mainWindow);
        this._mainToolbar.insert(this._toolbarZoom, 0);

        return this._mainToolbar;
    },
});

let handler = new MimeHandler.MimeHandler();
let renderer = new ImageRenderer();

let formats = GdkPixbuf.Pixbuf.get_formats();
for (let idx in formats) {
    let mimeTypes = formats[idx].get_mime_types();
    handler.registerMimeTypes(mimeTypes, renderer);
}
