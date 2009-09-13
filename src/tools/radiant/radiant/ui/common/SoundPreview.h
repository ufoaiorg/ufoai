#ifndef SOUNDSHADERPREVIEW_H_
#define SOUNDSHADERPREVIEW_H_

#include <string>

// Forward declaration
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkListStore GtkListStore;
typedef struct _GtkTreeSelection GtkTreeSelection;
typedef struct _GtkButton GtkButton;

namespace ui
{
	/** greebo: This class provides the UI elements to inspect a given
	 * 			soundfile with playback option.
	 *
	 * 			Use the GtkWidget* cast operator to pack this into a
	 * 			parent container.
	 */
	class SoundPreview
	{
			// The main container widget of this preview
			GtkWidget* _widget;

			GtkWidget* _playButton;
			GtkWidget* _stopButton;
			GtkWidget* _statusLabel;

			// The currently "previewed" soundfile
			std::string _soundFile;

		public:
			SoundPreview ();

			/** greebo: Sets the soundfile to preview.
			 */
			void setSound (const std::string& soundFile);

			/** greebo: Operator cast to GtkWidget to pack this into a
			 * 			parent container widget.
			 */
			operator GtkWidget* ();

		private:
			/** greebo: Returns the currently selected sound file (file list)
			 *
			 * @returns: the filename or "" if nothing selected.
			 */
			std::string getSelectedSoundFile ();

			/** greebo: Creates the control widgets (play button) and such.
			 */
			GtkWidget* createControlPanel ();

			// GTK Callbacks
			static void onPlay (GtkButton* button, SoundPreview* self);
			static void onStop (GtkButton* button, SoundPreview* self);
	};

} // namespace ui

#endif /*SOUNDSHADERPREVIEW_H_*/
