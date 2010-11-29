#include "SoundPreview.h"

#include "isound.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/TextColumn.h"
#include "gtkutil/LeftAlignedLabel.h"
#include "gtkutil/TreeModel.h"
#include <gtk/gtk.h>
#include <iostream>
#include "radiant_i18n.h"

namespace ui
{
	namespace
	{
		enum FileListCols
		{
			DISPLAYNAME_COL, FILENAME_COL, // The filename (VFS path)
			NUM_COLS
		};
	}

	SoundPreview::SoundPreview () :
		_soundFile("")
	{
		_widget = gtk_hbox_new(FALSE, 12);

		gtk_box_pack_start(GTK_BOX(_widget), createControlPanel(), FALSE, FALSE, 0);
	}

	GtkWidget* SoundPreview::createControlPanel ()
	{
		GtkWidget* vbox = gtk_vbox_new(FALSE, 6);
		gtk_widget_set_size_request(vbox, 200, -1);

		// Create the playback button
		_playButton = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
		_stopButton = gtk_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
		g_signal_connect(G_OBJECT(_playButton), "clicked", G_CALLBACK(onPlay), this);
		g_signal_connect(G_OBJECT(_stopButton), "clicked", G_CALLBACK(onStop), this);

		GtkWidget* btnHBox = gtk_hbox_new(TRUE, 6);
		gtk_box_pack_start(GTK_BOX(btnHBox), _playButton, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(btnHBox), _stopButton, TRUE, TRUE, 0);

		gtk_box_pack_end(GTK_BOX(vbox), btnHBox, FALSE, FALSE, 0);

		_statusLabel = gtkutil::LeftAlignedLabel("");
		gtk_box_pack_end(GTK_BOX(vbox), _statusLabel, FALSE, FALSE, 0);

		return vbox;
	}

	void SoundPreview::setSound (const std::string& soundFile)
	{
		_soundFile = soundFile;
		gtk_widget_set_sensitive(_playButton, !soundFile.empty());
	}

	SoundPreview::operator GtkWidget* ()
	{
		return _widget;
	}

	std::string SoundPreview::getSelectedSoundFile ()
	{
		return _soundFile;
	}

	void SoundPreview::onPlay (GtkButton* button, SoundPreview* self)
	{
		gtk_label_set_markup(GTK_LABEL(self->_statusLabel), "");
		std::string selectedFile = self->getSelectedSoundFile();

		if (!GlobalSoundManager().isPlaybackEnabled()) {
			gtk_label_set_markup(GTK_LABEL(self->_statusLabel), _("<b>Error:</b> Sound is deactivated."));
		} else {
			if (!selectedFile.empty()) {
				// Pass the call to the sound manager
				if (!GlobalSoundManager().playSound(selectedFile)) {
					gtk_label_set_markup(GTK_LABEL(self->_statusLabel), _("<b>Error:</b> File not found."));
				}
			}
		}
	}

	void SoundPreview::onStop (GtkButton* button, SoundPreview* self)
	{
		// Pass the call to the sound manager
		GlobalSoundManager().stopSound();
		gtk_label_set_markup(GTK_LABEL(self->_statusLabel), "");
	}

} // namespace ui
