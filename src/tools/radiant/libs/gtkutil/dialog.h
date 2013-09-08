/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#if !defined(INCLUDED_GTKUTIL_DIALOG_H)
#define INCLUDED_GTKUTIL_DIALOG_H

#include "generic/callback.h"
#include "generic/arrayrange.h"
#include "iradiant.h"
#include <gtk/gtkenums.h>
#include <string>

typedef int gint;
typedef gint gboolean;
typedef void (*GCallback) (void);
typedef void* gpointer;
typedef struct _GdkEventAny GdkEventAny;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkEntry GtkEntry;
typedef struct _GtkButton GtkButton;
typedef struct _GtkLabel GtkLabel;
typedef struct _GtkTable GtkTable;
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkVBox GtkVBox;
typedef struct _GtkHBox GtkHBox;
typedef struct _GtkFrame GtkFrame;

struct ModalDialog
{
		ModalDialog () :
			loop(true), ret(eIDCANCEL)
		{
		}
		bool loop;
		EMessageBoxReturn ret;
};

struct ModalDialogButton
{
		ModalDialogButton (ModalDialog& dialog, EMessageBoxReturn value) :
			m_dialog(dialog), m_value(value)
		{
		}
		ModalDialog& m_dialog;
		EMessageBoxReturn m_value;
};

GtkButton* create_dialog_button (const std::string& label, GCallback func, gpointer data);
GtkVBox* create_dialog_vbox (int spacing, int border = 0);
GtkHBox* create_dialog_hbox (int spacing, int border = 0);
GtkFrame* create_dialog_frame (const std::string& frameHeadline, GtkShadowType shadow = GTK_SHADOW_ETCHED_IN);

GtkButton* create_modal_dialog_button (const std::string& label, ModalDialogButton& button);
GtkWindow* create_fixedsize_modal_dialog_window (GtkWindow* parent, const std::string& title, ModalDialog& dialog, int width =
		-1, int height = -1);
EMessageBoxReturn modal_dialog_show (GtkWindow* window, ModalDialog& dialog);

gboolean dialog_button_ok (GtkWidget *widget, ModalDialog* data);
gboolean dialog_button_cancel (GtkWidget *widget, ModalDialog* data);
gboolean dialog_delete_callback (GtkWidget *widget, GdkEventAny* event, ModalDialog* data);

GtkWindow* create_simple_modal_dialog_window (const std::string& title, ModalDialog& dialog, GtkWidget* contents);

class PathEntry
{
	public:
		GtkFrame* m_frame;
		GtkEntry* m_entry;
		GtkButton* m_button;
		PathEntry (GtkFrame* frame, GtkEntry* entry, GtkButton* button) :
			m_frame(frame), m_entry(entry), m_button(button)
		{
		}
};

PathEntry PathEntry_new ();

GtkLabel* DialogLabel_new (const std::string& name);
GtkTable* DialogRow_new (const std::string& name, GtkWidget* widget);
typedef struct _GtkVBox GtkVBox;
void DialogVBox_packRow (GtkVBox* vbox, GtkWidget* row);

namespace gtkutil
{
	// Display a modal error dialog
	void errorDialog (const std::string&);
	// Display a modal info dialog
	void infoDialog (const std::string&);
	// Display a modal error dialog and quit immediately
	void fatalErrorDialog (const std::string& errorText);
}

#endif
