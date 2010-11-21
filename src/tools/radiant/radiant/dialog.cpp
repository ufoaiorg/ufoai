/**
 * @file dialog.cpp
 * @brief Base dialog class, provides a way to run modal dialogs and
 * set/get the widget values in member variables.
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

#include "dialog.h"

#include "debugging/debugging.h"

#include "mainframe.h"

#include <stdlib.h>

#include <gtk/gtk.h>

#include "stream/stringstream.h"
#include "convert.h"
#include "gtkutil/dialog.h"
#include "gtkutil/button.h"
#include "gtkutil/entry.h"
#include "gtkutil/image.h"

#include "gtkmisc.h"

GtkEntry* DialogEntry_new ()
{
	GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
	gtk_widget_show(GTK_WIDGET(entry));
	widget_set_size(GTK_WIDGET(entry), 64, 0);
	return entry;
}

class DialogEntryRow
{
	public:
		DialogEntryRow (GtkWidget* row, GtkEntry* entry) :
			m_row(row), m_entry(entry)
		{
		}
		GtkWidget* m_row;
		GtkEntry* m_entry;
};

DialogEntryRow DialogEntryRow_new (const std::string& name)
{
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);

	GtkEntry* entry = DialogEntry_new();
	gtk_container_add(GTK_CONTAINER(alignment), GTK_WIDGET(entry));

	return DialogEntryRow(GTK_WIDGET(DialogRow_new(name, alignment)), entry);
}

GtkSpinButton* DialogSpinner_new (double value, double lower, double upper, int fraction)
{
	double step = 1.0 / double(fraction);
	unsigned int digits = 0;
	for (; fraction > 1; fraction /= 10) {
		++digits;
	}
	GtkSpinButton
			* spin =
					GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(value, lower, upper, step, 10, 0)), step, digits));
	gtk_widget_show(GTK_WIDGET(spin));
	widget_set_size(GTK_WIDGET(spin), 64, 0);
	return spin;
}

class DialogSpinnerRow
{
	public:
		DialogSpinnerRow (GtkWidget* row, GtkSpinButton* spin) :
			m_row(row), m_spin(spin)
		{
		}
		GtkWidget* m_row;
		GtkSpinButton* m_spin;
};

DialogSpinnerRow DialogSpinnerRow_new (const char* name, double value, double lower, double upper, int fraction)
{
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);

	GtkSpinButton* spin = DialogSpinner_new(value, lower, upper, fraction);
	gtk_container_add(GTK_CONTAINER(alignment), GTK_WIDGET(spin));

	return DialogSpinnerRow(GTK_WIDGET(DialogRow_new(name, alignment)), spin);
}

template<typename Type_, typename Other_, void(*Import) (Type_&, Other_), void(*Export) (Type_&,
		const Callback1<Other_>&)>
class ImportExport
{
	public:
		typedef Type_ Type;
		typedef Other_ Other;

		typedef ReferenceCaller1<Type, Other, Import> ImportCaller;
		typedef ReferenceCaller1<Type, const Callback1<Other>&, Export> ExportCaller;
};

void BoolToggleImport (GtkToggleButton& widget, bool value)
{
	gtk_toggle_button_set_active(&widget, value);
}
void BoolToggleExport (GtkToggleButton& widget, const BoolImportCallback& importCallback)
{
	importCallback(gtk_toggle_button_get_active(&widget) != FALSE);
}
typedef ImportExport<GtkToggleButton, bool, BoolToggleImport, BoolToggleExport> BoolToggleImportExport;

void IntRadioImport (GtkRadioButton& widget, int index)
{
	radio_button_set_active(&widget, index);
}
void IntRadioExport (GtkRadioButton& widget, const IntImportCallback& importCallback)
{
	importCallback(radio_button_get_active(&widget));
}
typedef ImportExport<GtkRadioButton, int, IntRadioImport, IntRadioExport> IntRadioImportExport;

template<typename Type, typename Formatter>
class StringFromType
{
		StringOutputStream value;
	public:
		StringFromType (const Type& type)
		{
			value << Formatter(type);
		}
		operator const char* () const
		{
			return value.c_str();
		}
};

typedef StringFromType<const char*, ConvertLocaleToUTF8> LocaleToUTF8;
typedef StringFromType<const char*, ConvertUTF8ToLocale> UTF8ToLocale;

void TextEntryImport (GtkEntry& widget, const char* text)
{
	gtk_entry_set_text(&widget, LocaleToUTF8(text));
}
void TextEntryExport (GtkEntry& widget, const StringImportCallback& importCallback)
{
	importCallback(UTF8ToLocale(gtk_entry_get_text(&widget)));
}
typedef ImportExport<GtkEntry, const char*, TextEntryImport, TextEntryExport> TextEntryImportExport;

void FloatSpinnerImport (GtkSpinButton& widget, float value)
{
	gtk_spin_button_set_value(&widget, value);
}
void FloatSpinnerExport (GtkSpinButton& widget, const FloatImportCallback& importCallback)
{
	importCallback(float(gtk_spin_button_get_value_as_float(&widget)));
}
typedef ImportExport<GtkSpinButton, float, FloatSpinnerImport, FloatSpinnerExport> FloatSpinnerImportExport;

void IntSpinnerImport (GtkSpinButton& widget, int value)
{
	gtk_spin_button_set_value(&widget, value);
}
void IntSpinnerExport (GtkSpinButton& widget, const IntImportCallback& importCallback)
{
	importCallback(gtk_spin_button_get_value_as_int(&widget));
}
typedef ImportExport<GtkSpinButton, int, IntSpinnerImport, IntSpinnerExport> IntSpinnerImportExport;

void IntAdjustmentImport (GtkAdjustment& widget, int value)
{
	gtk_adjustment_set_value(&widget, value);
}
void IntAdjustmentExport (GtkAdjustment& widget, const IntImportCallback& importCallback)
{
	importCallback((int) gtk_adjustment_get_value(&widget));
}
typedef ImportExport<GtkAdjustment, int, IntAdjustmentImport, IntAdjustmentExport> IntAdjustmentImportExport;

void IntComboImport (GtkComboBox& widget, int value)
{
	gtk_combo_box_set_active(&widget, value);
}
void IntComboExport (GtkComboBox& widget, const IntImportCallback& importCallback)
{
	importCallback(gtk_combo_box_get_active(&widget));
}
typedef ImportExport<GtkComboBox, int, IntComboImport, IntComboExport> IntComboImportExport;

template<typename FirstArgument>
class CallbackDialogData: public DLG_DATA
{
	public:
		typedef Callback1<FirstArgument> ImportCallback;
		typedef Callback1<const ImportCallback&> ExportCallback;

	private:
		ImportCallback m_importWidget;
		ExportCallback m_exportWidget;
		ImportCallback m_importViewer;
		ExportCallback m_exportViewer;

	public:
		CallbackDialogData (const ImportCallback& importWidget, const ExportCallback& exportWidget,
				const ImportCallback& importViewer, const ExportCallback& exportViewer) :
			m_importWidget(importWidget), m_exportWidget(exportWidget), m_importViewer(importViewer), m_exportViewer(
					exportViewer)
		{
		}
		void importData () const
		{
			m_exportViewer(m_importWidget);
		}
		void exportData () const
		{
			m_exportWidget(m_importViewer);
		}
};

template<typename Widget, typename Viewer>
class AddData
{
		DialogDataList& m_data;
	public:
		AddData (DialogDataList& data) :
			m_data(data)
		{
		}
		void apply (typename Widget::Type& widget, typename Viewer::Type& viewer) const
		{
			m_data.push_back(new CallbackDialogData<typename Widget::Other> (typename Widget::ImportCaller(widget),
					typename Widget::ExportCaller(widget), typename Viewer::ImportCaller(viewer),
					typename Viewer::ExportCaller(viewer)));
		}
};

template<typename Widget>
class AddCustomData
{
		DialogDataList& m_data;
	public:
		AddCustomData (DialogDataList& data) :
			m_data(data)
		{
		}
		void apply (typename Widget::Type& widget, const Callback1<typename Widget::Other>& importViewer,
				const Callback1<const Callback1<typename Widget::Other>&>& exportViewer) const
		{
			m_data.push_back(new CallbackDialogData<typename Widget::Other> (typename Widget::ImportCaller(widget),
					typename Widget::ExportCaller(widget), importViewer, exportViewer));
		}
};

// =============================================================================
// Dialog class

Dialog::Dialog () :
	m_window(0), m_parent(0)
{
}

Dialog::~Dialog ()
{
	for (DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i) {
		delete *i;
	}

	ASSERT_MESSAGE(m_window == 0, "dialog window not destroyed");
}

void Dialog::ShowDlg ()
{
	ASSERT_MESSAGE(m_window != 0, "dialog was not constructed");
	importData();
	gtk_widget_show(GTK_WIDGET(m_window));
}

void Dialog::HideDlg ()
{
	ASSERT_MESSAGE(m_window != 0, "dialog was not constructed");
	exportData();
	gtk_widget_hide(GTK_WIDGET(m_window));
}

static gint delete_event_callback (GtkWidget *widget, GdkEvent* event, gpointer data)
{
	reinterpret_cast<Dialog*> (data)->HideDlg();
	reinterpret_cast<Dialog*> (data)->EndModal(eIDCANCEL);
	return TRUE;
}

void Dialog::Create ()
{
	ASSERT_MESSAGE(m_window == 0, "dialog cannot be constructed");

	m_window = BuildDialog();
	g_signal_connect(G_OBJECT(m_window), "delete_event", G_CALLBACK(delete_event_callback), this);
}

void Dialog::Destroy ()
{
	ASSERT_MESSAGE(m_window != 0, "dialog cannot be destroyed");

	gtk_widget_destroy(GTK_WIDGET(m_window));
	m_window = 0;
}

void Dialog::AddBoolToggleData (GtkToggleButton& widget, const BoolImportCallback& importViewer,
		const BoolExportCallback& exportViewer)
{
	AddCustomData<BoolToggleImportExport> (m_data).apply(widget, importViewer, exportViewer);
}

void Dialog::AddIntRadioData (GtkRadioButton& widget, const IntImportCallback& importViewer,
		const IntExportCallback& exportViewer)
{
	AddCustomData<IntRadioImportExport> (m_data).apply(widget, importViewer, exportViewer);
}

void Dialog::AddTextEntryData (GtkEntry& widget, const StringImportCallback& importViewer,
		const StringExportCallback& exportViewer)
{
	AddCustomData<TextEntryImportExport> (m_data).apply(widget, importViewer, exportViewer);
}

void Dialog::AddFloatSpinnerData (GtkSpinButton& widget, const FloatImportCallback& importViewer,
		const FloatExportCallback& exportViewer)
{
	AddCustomData<FloatSpinnerImportExport> (m_data).apply(widget, importViewer, exportViewer);
}

void Dialog::AddIntAdjustmentData (GtkAdjustment& widget, const IntImportCallback& importViewer,
		const IntExportCallback& exportViewer)
{
	AddCustomData<IntAdjustmentImportExport> (m_data).apply(widget, importViewer, exportViewer);
}

void Dialog::AddIntComboData (GtkComboBox& widget, const IntImportCallback& importViewer,
		const IntExportCallback& exportViewer)
{
	AddCustomData<IntComboImportExport> (m_data).apply(widget, importViewer, exportViewer);
}

void Dialog::exportData ()
{
	for (DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i) {
		(*i)->exportData();
	}
}

void Dialog::importData ()
{
	for (DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i) {
		(*i)->importData();
	}
}

void Dialog::EndModal (EMessageBoxReturn code)
{
	m_modal.loop = 0;
	m_modal.ret = code;
}

EMessageBoxReturn Dialog::DoModal ()
{
	importData();

	// Import the values from the registry before showing the dialog
	_registryConnector.importValues();

	PreModal();

	EMessageBoxReturn ret = modal_dialog_show(m_window, m_modal);
	ASSERT_NOTNULL(m_window);
	if (ret == eIDOK) {
		exportData();
	}

	gtk_widget_hide(GTK_WIDGET(m_window));

	PostModal(m_modal.ret);

	return m_modal.ret;
}

GtkWidget* Dialog::addCheckBox (GtkWidget* vbox, const char* name, const char* flag,
		const BoolImportCallback& importViewer, const BoolExportCallback& exportViewer)
{
	GtkWidget* check = gtk_check_button_new_with_label(flag);
	gtk_widget_show(check);
	AddBoolToggleData(*GTK_TOGGLE_BUTTON(check), importViewer, exportViewer);

	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(DialogRow_new(name, check)));
	return check;
}

GtkWidget* Dialog::addCheckBox (GtkWidget* vbox, const char* name, const char* flag, bool& data)
{
	return addCheckBox(vbox, name, flag, BoolImportCaller(data), BoolExportCaller(data));
}

/* greebo: This adds a checkbox to the given VBox and connects an XMLRegistry value to it.
 * The actual data is "imported" and "exported" via those templates.
 */
GtkWidget* Dialog::addCheckBox(GtkWidget* vbox, const std::string& name, const std::string& flag, const std::string& registryKey)
{
	// Create a new checkbox with the given caption and display it
	GtkWidget* check = gtk_check_button_new_with_label(flag.c_str());
	gtk_widget_show(check);

	// Connect the registry key to this toggle button
	_registryConnector.connectGtkObject(GTK_OBJECT(check), registryKey);

	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(DialogRow_new(name, check)));
	return check;
}

/* greebo: This adds a horizontal slider and connects it to the value of the given registryKey
 */
void Dialog::addSlider (GtkWidget* vbox, const std::string& name, const std::string& registryKey,
		gboolean draw_value, double value, double lower, double upper,
		double step_increment, double page_increment, double page_size)
{
	// Create a new adjustment with the boundaries <lower> and <upper> and all the increments
	GtkObject* adj = gtk_adjustment_new(value, lower, upper, step_increment, page_increment, page_size);

	// Connect the registry key to this adjustment
	_registryConnector.connectGtkObject(adj, registryKey);

	// scale
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);
	gtk_widget_show(alignment);

	GtkWidget* scale = gtk_hscale_new(GTK_ADJUSTMENT(adj));
	gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_LEFT);
	gtk_widget_show(scale);
	gtk_container_add(GTK_CONTAINER(alignment), scale);

	gtk_scale_set_draw_value(GTK_SCALE (scale), draw_value);
	int digits = (step_increment < 1.0f) ? 2 : 0;
	gtk_scale_set_digits(GTK_SCALE (scale), digits);

	GtkTable* row = DialogRow_new(name, alignment);
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));
}

// greebo: Adds a PathEntry to choose files or directories (depending on the given boolean)
GtkWidget* Dialog::addPathEntry (GtkWidget* vbox, const std::string& name, const std::string& registryKey,
		bool browseDirectories)
{
	PathEntry pathEntry = PathEntry_new();
	g_signal_connect(G_OBJECT(pathEntry.m_button), "clicked", G_CALLBACK(browseDirectories ? button_clicked_entry_browse_directory : button_clicked_entry_browse_file), pathEntry.m_entry);

	// Connect the registry key to the newly created input field
	_registryConnector.connectGtkObject(GTK_OBJECT(pathEntry.m_entry), registryKey);

	GtkTable* row = DialogRow_new(name, GTK_WIDGET(pathEntry.m_frame));
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));

	return GTK_WIDGET(row);
}

void Dialog::addCombo (GtkWidget* vbox, const char* name, StringArrayRange values,
		const IntImportCallback& importViewer, const IntExportCallback& exportViewer)
{
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);
	{
		GtkWidget* combo = gtk_combo_box_new_text();

		for (StringArrayRange::Iterator i = values.first; i != values.last; ++i) {
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo), *i);
		}

		AddIntComboData(*GTK_COMBO_BOX(combo), importViewer, exportViewer);

		gtk_widget_show(combo);
		gtk_container_add(GTK_CONTAINER(alignment), combo);
	}

	GtkTable* row = DialogRow_new(name, alignment);
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));
}

void Dialog::addCombo (GtkWidget* vbox, const char* name, int& data, StringArrayRange values)
{
	addCombo(vbox, name, values, IntImportCaller(data), IntExportCaller(data));
}

void Dialog::addCombo (GtkWidget* vbox, const std::string& name, const std::string& registryKey,
		const ComboBoxValueList& valueList)
{
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);

	{
		// Create a new combo box
		GtkWidget* combo = gtk_combo_box_new_text();

		// Add all the string values to the combo box
		for (ComboBoxValueList::const_iterator i = valueList.begin(); i != valueList.end(); i++) {
			// Add the current string value to the combo box
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo), i->c_str());
		}

		// Connect the registry key to the newly created combo box
		_registryConnector.connectGtkObject(GTK_OBJECT(combo), registryKey);

		// Add it to the container and make it visible
		gtk_widget_show(combo);
		gtk_container_add(GTK_CONTAINER(alignment), combo);
	}

	// Add the widget to the dialog row
	GtkTable* row = DialogRow_new(name.c_str(), alignment);
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));
}

void Dialog::addSlider (GtkWidget* vbox, const char* name, int& data, gboolean draw_value, const char* low,
		const char* high, double value, double lower, double upper, double step_increment, double page_increment,
		double page_size)
{
	// adjustment
	GtkObject* adj = gtk_adjustment_new(value, lower, upper, step_increment, page_increment, page_size);
	AddIntAdjustmentData(*GTK_ADJUSTMENT(adj), IntImportCaller(data), IntExportCaller(data));

	// scale
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);
	gtk_widget_show(alignment);

	GtkWidget* scale = gtk_hscale_new(GTK_ADJUSTMENT(adj));
	gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_LEFT);
	gtk_widget_show(scale);
	gtk_container_add(GTK_CONTAINER(alignment), scale);

	gtk_scale_set_draw_value(GTK_SCALE(scale), draw_value);
	gtk_scale_set_digits(GTK_SCALE(scale), 0);

	GtkTable* row = DialogRow_new(name, alignment);
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));
}

void Dialog::addRadio (GtkWidget* vbox, const char* name, StringArrayRange names,
		const IntImportCallback& importViewer, const IntExportCallback& exportViewer)
{
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);
	{
		RadioHBox radioBox = RadioHBox_new(names);
		gtk_container_add(GTK_CONTAINER(alignment), GTK_WIDGET(radioBox.m_hbox));
		AddIntRadioData(*GTK_RADIO_BUTTON(radioBox.m_radio), importViewer, exportViewer);
	}

	GtkTable* row = DialogRow_new(name, alignment);
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));
}

void Dialog::addRadio (GtkWidget* vbox, const char* name, int& data, StringArrayRange names)
{
	addRadio(vbox, name, names, IntImportCaller(data), IntExportCaller(data));
}

void Dialog::addRadioIcons (GtkWidget* vbox, const char* name, StringArrayRange icons,
		const IntImportCallback& importViewer, const IntExportCallback& exportViewer)
{
	GtkWidget* table = gtk_table_new(2, static_cast<guint> (icons.last - icons.first), FALSE);
	gtk_widget_show(table);

	gtk_table_set_row_spacings(GTK_TABLE (table), 5);
	gtk_table_set_col_spacings(GTK_TABLE (table), 5);

	GSList* group = 0;
	GtkWidget* radio = 0;
	for (StringArrayRange::Iterator icon = icons.first; icon != icons.last; ++icon) {
		guint pos = static_cast<guint> (icon - icons.first);
		GtkWidget* image = gtkutil::getImage(*icon);
		gtk_widget_show(image);
		gtk_table_attach(GTK_TABLE(table), image, pos, pos + 1, 0, 1, (GtkAttachOptions) (0), (GtkAttachOptions) (0),
				0, 0);

		radio = gtk_radio_button_new(group);
		gtk_widget_show(radio);
		gtk_table_attach(GTK_TABLE (table), radio, pos, pos + 1, 1, 2, (GtkAttachOptions) (0), (GtkAttachOptions) (0),
				0, 0);

		group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
	}

	AddIntRadioData(*GTK_RADIO_BUTTON(radio), importViewer, exportViewer);

	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(DialogRow_new(name, table)));
}

void Dialog::addRadioIcons (GtkWidget* vbox, const char* name, int& data, StringArrayRange icons)
{
	addRadioIcons(vbox, name, icons, IntImportCaller(data), IntExportCaller(data));
}

// greebo: add an entry box connected to the given registryKey
GtkWidget* Dialog::addEntry (GtkWidget* vbox, const std::string& name, const std::string& registryKey)
{
	// Create a new row containing an input field
	DialogEntryRow row(DialogEntryRow_new(name));

	// Connect the registry key to the newly created input field
	_registryConnector.connectGtkObject(GTK_OBJECT(row.m_entry), registryKey);

	DialogVBox_packRow(GTK_VBOX(vbox), row.m_row);
	return row.m_row;
}

GtkWidget* Dialog::addPathEntry (GtkWidget* vbox, const char* name, bool browse_directory,
		const StringImportCallback& importViewer, const StringExportCallback& exportViewer)
{
	PathEntry pathEntry = PathEntry_new();
	g_signal_connect(G_OBJECT(pathEntry.m_button), "clicked", G_CALLBACK(browse_directory ? button_clicked_entry_browse_directory : button_clicked_entry_browse_file), pathEntry.m_entry);

	AddTextEntryData(*GTK_ENTRY(pathEntry.m_entry), importViewer, exportViewer);

	GtkTable* row = DialogRow_new(name, GTK_WIDGET(pathEntry.m_frame));
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));

	return GTK_WIDGET(row);
}

GtkWidget* Dialog::addPathEntry (GtkWidget* vbox, const char* name, std::string& data, bool browse_directory)
{
	return addPathEntry(vbox, name, browse_directory, StringImportCallback(StringImportCaller(data)),
			StringExportCallback(StringExportCaller(data)));
}

GtkWidget* Dialog::addSpinner (GtkWidget* vbox, const std::string& name, const std::string& registryKey, double lower,
		double upper, int fraction)
{
	// Load the initial value (maybe unnecessary, as the value is loaded upon dialog show)
	float value = GlobalRegistry().getFloat(registryKey);

	// Create a new row containing an input field
	DialogSpinnerRow row(DialogSpinnerRow_new(name.c_str(), value, lower, upper, fraction));

	// Connect the registry key to the newly created input field
	_registryConnector.connectGtkObject(GTK_OBJECT(row.m_spin), registryKey);

	DialogVBox_packRow(GTK_VBOX(vbox), row.m_row);
	return row.m_row;
}

GtkWidget* Dialog::addSpinner (GtkWidget* vbox, const char* name, double value, double lower, double upper,
		const FloatImportCallback& importViewer, const FloatExportCallback& exportViewer)
{
	DialogSpinnerRow row(DialogSpinnerRow_new(name, value, lower, upper, 10));
	AddFloatSpinnerData(*row.m_spin, importViewer, exportViewer);
	DialogVBox_packRow(GTK_VBOX(vbox), row.m_row);
	return row.m_row;
}
