/**
 * @file LightDialog.cpp
 * @brief Creates the light dialog to set the intensity
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "LightDialog.h"
#include "iradiant.h"
#include "gtkutil/RightAlignment.h"
#include "gtkutil/FramedWidget.h"
#include "string/string.h"

namespace ui
{
	namespace
	{
		static const std::string _defaultIntensity = "100";
		static const int _maxIntensity = 400;
		static const int _minIntensity = 1;
	}

	LightDialog::LightDialog () :
		_widget(gtk_window_new(GTK_WINDOW_TOPLEVEL)), _aborted(false)
	{
		// Set up the window
		gtk_window_set_transient_for(GTK_WINDOW(_widget), GlobalRadiant().getMainWindow());
		gtk_window_set_modal(GTK_WINDOW(_widget), TRUE);
		gtk_window_set_title(GTK_WINDOW(_widget), _("Create light entity"));
		gtk_window_set_position(GTK_WINDOW(_widget), GTK_WIN_POS_CENTER_ON_PARENT);
		gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_DIALOG);

		// Set the default size of the window
		gtk_window_set_default_size(GTK_WINDOW(_widget), 300, 300);

		// Delete event
		g_signal_connect(
				G_OBJECT(_widget), "delete-event", G_CALLBACK(_onDelete), this
		);

		// Main vbox
		GtkWidget* vbx = gtk_vbox_new(FALSE, 12);
		gtk_box_pack_start(GTK_BOX(vbx), gtkutil::FramedWidget(createColorSelector(), _("Light Color")), TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbx), gtkutil::FramedWidget(createIntensity(), _("Light Intensity")), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbx), createButtons(), FALSE, FALSE, 0);

		gtk_container_set_border_width(GTK_CONTAINER(_widget), 12);
		gtk_container_add(GTK_CONTAINER(_widget), vbx);

		_intensityEntry.setValue(_defaultIntensity);
	}

	LightDialog::~LightDialog ()
	{
	}

	GtkWidget* LightDialog::createColorSelector ()
	{
		_colorSelector = GTK_COLOR_SELECTION(gtk_color_selection_new());
		gtk_color_selection_set_has_opacity_control(_colorSelector, FALSE);
		gtk_color_selection_set_has_palette(_colorSelector, FALSE);
		return GTK_WIDGET(_colorSelector);
	}

	// Create intensity panel
	GtkWidget* LightDialog::createIntensity ()
	{
		return _intensityEntry.getWidget();
	}

	// Create buttons panel
	GtkWidget* LightDialog::createButtons ()
	{
		GtkWidget* hbx = gtk_hbox_new(TRUE, 6);

		GtkWidget* okButton = gtk_button_new_from_stock(GTK_STOCK_OK);
		GtkWidget* cancelButton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

		g_signal_connect(G_OBJECT(okButton), "clicked",
				G_CALLBACK(_onOK), this);
		g_signal_connect(G_OBJECT(cancelButton), "clicked",
				G_CALLBACK(_onCancel), this);

		gtk_box_pack_end(GTK_BOX(hbx), okButton, TRUE, TRUE, 0);
		gtk_box_pack_end(GTK_BOX(hbx), cancelButton, TRUE, TRUE, 0);

		return gtkutil::RightAlignment(hbx);
	}

	std::string LightDialog::getIntensity ()
	{
		int intensity = string::toInt(_intensity);
		// boundary check
		if (intensity < _minIntensity)
			intensity = _minIntensity;
		else if (intensity > _maxIntensity)
			intensity = _maxIntensity;

		return string::toString(intensity);
	}

	Vector3 LightDialog::getColor ()
	{
		return _color;
	}

	Vector3 LightDialog::getNormalizedColor ()
	{
		GdkColor gdkcolor;
		Vector3 color;

		gtk_color_selection_get_current_color(_colorSelector, &gdkcolor);
		color[0] = (float) gdkcolor.red / 65535.0;
		color[1] = (float) gdkcolor.green / 65535.0;
		color[2] = (float) gdkcolor.blue / 65535.0;

		float largest = 0.0F;

		if (color[0] > largest)
			largest = color[0];
		if (color[1] > largest)
			largest = color[1];
		if (color[2] > largest)
			largest = color[2];

		if (largest == 0.0F) {
			color[0] = 1.0F;
			color[1] = 1.0F;
			color[2] = 1.0F;
		} else {
			const float scaler = 1.0F / largest;

			color[0] *= scaler;
			color[1] *= scaler;
			color[2] *= scaler;
		}

		return color;
	}

	/* GTK CALLBACKS */

	// Delete dialog
	gboolean LightDialog::_onDelete (GtkWidget* w, GdkEvent* e, LightDialog* self)
	{
		self->_aborted = true;
		gtk_main_quit();
		return false; // propagate event
	}

	// OK button
	void LightDialog::_onOK (GtkWidget* w, LightDialog* self)
	{
		self->_color = self->getNormalizedColor();
		self->_intensity = self->_intensityEntry.getValue();
		gtk_widget_destroy(self->_widget);
		gtk_main_quit();
	}

	// Cancel button
	void LightDialog::_onCancel (GtkWidget* w, LightDialog* self)
	{
		self->_aborted = true;
		gtk_widget_destroy(self->_widget);
		gtk_main_quit();
	}

	// Show and block
	void LightDialog::show ()
	{
		gtk_widget_show_all(_widget);
		gtk_main();
	}

	bool LightDialog::isAborted ()
	{
		return _aborted;
	}
}
