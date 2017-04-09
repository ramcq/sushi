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

const Gettext = imports.gettext.domain('sushi');
const _ = Gettext.gettext;
const Gtk = imports.gi.Gtk;

const Lang = imports.lang;

let SPINNER_SIZE = 48;

const SpinnerBox = new Lang.Class({
    Name: 'SpinnerBox',

    _init : function(args) {
        this.canFullScreen = false;
        this.moveOnClick = true;

        this._spinner = new Gtk.Spinner();
        this._spinner.set_size_request(SPINNER_SIZE, SPINNER_SIZE);

        this._label = new Gtk.Label();
        this._label.set_text(_("Loading…"));

        this._spinnerBox = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL,
                                         valign: Gtk.Align.CENTER,
                                         spacing: 12 });
        this._spinnerBox.pack_start(this._spinner, true, true, 0);
        this._spinnerBox.pack_start(this._label, true, true, 0);

        this._spinnerBox.show_all();
    },

    render : function() {
        this._spinner.start();
        return this._spinnerBox;
    },

    getSizeForAllocation : function() {
        let spinnerSize = this._spinnerBox.get_preferred_size();
        return [ spinnerSize[0].width,
                 spinnerSize[0].height ];
    }
});
