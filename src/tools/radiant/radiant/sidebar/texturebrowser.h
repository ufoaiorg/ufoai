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

#if !defined(INCLUDED_TEXWINDOW_H)
#define INCLUDED_TEXWINDOW_H

#include <string>
#include <set>
#include <glib.h>
#include <gtk/gtk.h>
#include "generic/callbackfwd.h"
#include "signal/signalfwd.h"
#include "math/Vector3.h"
#include "ishader.h"
#include "signal/signal.h"

typedef std::set<std::string> TextureGroups;

/*
 ============================================================================

 TEXTURE LAYOUT

 TTimo: now based on a rundown through all the shaders
 NOTE: we expect the Active shaders count doesn't change during a Texture_StartPos .. Texture_NextPos cycle
 otherwise we may need to rely on a list instead of an array storage
 ============================================================================
 */

class TextureLayout {
	public:
		// texture layout functions
		// TTimo: now based on shaders
		int current_x, current_y, current_row;
		TextureLayout() :
			current_x(8), current_y(-8), current_row(0) {
		}
};

class DeferredAdjustment {
		gdouble m_value;
		guint m_handler;
		typedef void (*ValueChangedFunction)(void* data, gdouble value);
		ValueChangedFunction m_function;
		void* m_data;

		static gboolean deferred_value_changed(gpointer data) {
			reinterpret_cast<DeferredAdjustment*> (data)->m_function(
					reinterpret_cast<DeferredAdjustment*> (data)->m_data,
					reinterpret_cast<DeferredAdjustment*> (data)->m_value);
			reinterpret_cast<DeferredAdjustment*> (data)->m_handler = 0;
			reinterpret_cast<DeferredAdjustment*> (data)->m_value = 0;
			return FALSE;
		}
	public:
		DeferredAdjustment(ValueChangedFunction function, void* data) :
			m_value(0), m_handler(0), m_function(function), m_data(data) {
		}
		void flush() {
			if (m_handler != 0) {
				g_source_remove(m_handler);
				deferred_value_changed(this);
			}
		}
		void value_changed(gdouble value) {
			m_value = value;
			if (m_handler == 0) {
				m_handler = g_idle_add(deferred_value_changed, this);
			}
		}
		static void adjustment_value_changed(GtkAdjustment *adjustment, DeferredAdjustment* self) {
			self->value_changed(adjustment->value);
		}
};

#include "gtkutil/widget.h"
#include "gtkutil/cursor.h"
#include "gtkutil/glwidget.h"
#include "texturelib.h"
#include "math/Vector3.h"
#include "preferencesystem.h"
#include "iregistry.h"
#include "sidebar.h"

class TextureBrowser: public RegistryKeyObserver, public PreferenceConstructor, public ui::SidebarComponent
{
	private:
		TextureGroups groups;

		int width, height;
		int originy;
		int m_nTotalHeight;
		Signal0 g_activeShadersChangedCallbacks;

		std::string shader;
		GtkWindow* m_parent;
		gtkutil::GLWidget _glWidget;
		GtkWidget* _widget;
		GtkWidget* m_texture_scroll;
		GtkWidget* m_treeViewTree;
		GtkWidget* m_scr_win_tree;
		GtkWidget* m_scr_win_tags;

		bool m_heightChanged;
		bool m_originInvalid;

		DeferredAdjustment m_scrollAdjustment;
		FreezePointer m_freezePointer;

		// the increment step we use against the wheel mouse
		std::size_t m_mouseWheelScrollIncrement;
		std::size_t m_textureScale;
		// make the texture increments match the grid changes
		bool m_showShaders;
		// if true, the texture window will only display in-use shaders
		// if false, all the shaders in memory are displayed
		bool m_hideUnused;
		bool m_hideInvalid;
		bool m_rmbSelected;
		// If true, textures are resized to an uniform size when displayed in the texture browser.
		// If false, textures are displayed in proportion to their pixel size.
		bool m_resizeTextures;
		// The uniform size (in pixels) that textures are resized to when m_resizeTextures is true.
		int m_uniformTextureSize;

		std::string currentDirectory;
	public:

		TextureBrowser();

		const std::string& getSelectedShader() const;
		void setSelectedShader(const std::string& shader);
		void showStartupShaders();
		void showDirectory(const std::string& directory);
		GtkWidget* getWidget() const;
		const std::string getTitle() const;
		void activeShadersChanged();
		void addActiveShadersChangedCallback(const SignalHandler& handler);
		void constructPreferencePage(PreferenceGroup& group);

		void keyChanged(const std::string& changedKey, const std::string& newValue);

		void registerCommands();

		void setOriginY(int originy);
		void createWidget();
	private:

		void focus(const std::string& name);
		void selectTextureAt(int mx, int my);
		void queueDraw();
		bool isTextureShown(const IShader* shader) const;
		void updateScroll();
		int getFontHeight();
		void setStatusText(const std::string& name);

		// Return the display width of a texture in the texture browser
		int getTextureWidth(const GLTexture* tex);
		// Return the display height of a texture in the texture browser
		int getTextureHeight(const GLTexture* tex);

		void textureScaleImport (int value);

		void heightChanged();
		void evaluateHeight();
		int getTotalHeight();
		void clampOriginY();
		int getOriginY();
		void getNextTexturePosition(TextureLayout& layout, const GLTexture* q, int *x, int *y);
		const IShader* getTextureAt(int mx, int my);
		void draw();
		void onMouseWheel(bool bUp);

		static gboolean onMouseMotion(GtkWidget *widget, GdkEventMotion *event,
				TextureBrowser* textureBrowser);
		static gboolean onButtonRelease(GtkWidget* widget, GdkEventButton* event,
				TextureBrowser* textureBrowser);
		static gboolean onButtonPress(GtkWidget* widget, GdkEventButton* event,
				TextureBrowser* textureBrowser);
		static void onToggleResizeTextures(GtkToggleToolButton* button,
				TextureBrowser* textureBrowser);

		static gboolean onSizeAllocate(GtkWidget* widget, GtkAllocation* allocation,
				TextureBrowser* textureBrowser);

		static gboolean onExpose(GtkWidget* widget, GdkEventExpose* event,
				TextureBrowser* textureBrowser);
		static void onVerticalScroll(GtkAdjustment *adjustment, TextureBrowser* textureBrowser);
		static gboolean onMouseScroll(GtkWidget* widget, GdkEventScroll* event,
				TextureBrowser* textureBrowser);
		static void onActivateDirectoryChange(GtkMenuItem* item, TextureBrowser* textureBrowser);

		// commands
		void refreshShaders();
		void showAll(void);

		GtkWidget* createMenuBar();
		GtkWidget* createToolBar();
		GtkMenuItem* constructToolsMenu(GtkMenu* menu);
		GtkMenuItem* constructDirectoriesMenu(GtkMenu* menu);
		void constructTreeView();
};
TextureBrowser& GlobalTextureBrowser();

void TextureBrowser_Construct();
void TextureBrowser_Destroy();

typedef Callback1<const char*> StringImportCallback;
template<typename FirstArgument, void(*func)(FirstArgument)>
class FreeCaller1;

#endif
