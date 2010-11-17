/**
 * @file texwindow.cpp
 * @brief Texture Window
 * @author Leonardo Zide (leo@lokigames.com)
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

#include "texturebrowser.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "ifilesystem.h"
#include "iundo.h"
#include "igl.h"
#include "iarchive.h"
#include "moduleobserver.h"
#include "ieventmanager.h"

#include <set>
#include <string>
#include <vector>

#include "math/Vector3.h"
#include "texturelib.h"
#include "string/string.h"
#include "shaderlib.h"
#include "os/file.h"
#include "os/path.h"
#include "stream/memstream.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "../texmanip.h"
#include "../textures.h"
#include "convert.h"
#include "imaterial.h"

#include "gtkutil/image.h"
#include "gtkutil/menu.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/cursor.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/TreeModel.h"
#include "gtkutil/ModalProgressDialog.h"

#include "../ui/common/ToolbarCreator.h"
#include "../map/map.h"
#include "../render/OpenGLRenderSystem.h"
#include "../select.h"
#include "../brush/TexDef.h"
#include "../brush/TextureProjection.h"
#include "../brush/brushmanip.h"
#include "../plugin.h"
#include "../gtkmisc.h"
#include "../mainframe.h"
#include "surfaceinspector/surfaceinspector.h"

namespace {
const std::string RKEY_TEXTURES_HIDE_UNUSED = "user/ui/textures/browser/hideUnused";
const std::string RKEY_TEXTURES_HIDE_INVALID = "user/ui/textures/browser/hideInvalid";
}

static void TextureBrowser_scrollChanged (void* data, gdouble value);

TextureBrowser::TextureBrowser () :
	_glWidget(false), m_texture_scroll(0), m_heightChanged(true), m_originInvalid(true), m_scrollAdjustment(
			TextureBrowser_scrollChanged, this), m_textureScale(50), m_hideUnused(GlobalRegistry().get(
			RKEY_TEXTURES_HIDE_UNUSED) == "1"), m_hideInvalid(GlobalRegistry().get(RKEY_TEXTURES_HIDE_INVALID) == "1"),
			m_rmbSelected(false), m_resizeTextures(true), m_uniformTextureSize(128)
{
	GlobalRegistry().addKeyObserver(this, RKEY_TEXTURES_HIDE_UNUSED);
	GlobalRegistry().addKeyObserver(this, RKEY_TEXTURES_HIDE_INVALID);

	Textures_setModeChangedNotify(MemberCaller<TextureBrowser, &TextureBrowser::queueDraw> (*this));

	GlobalShaderSystem().setActiveShadersChangedNotify(MemberCaller<TextureBrowser,
			&TextureBrowser::activeShadersChanged> (*this));

	setSelectedShader("");
}

void TextureBrowser::keyChanged (const std::string& changedKey, const std::string& newValue)
{
	m_hideUnused = (GlobalRegistry().get(RKEY_TEXTURES_HIDE_UNUSED) == "1");
	m_hideInvalid = (GlobalRegistry().get(RKEY_TEXTURES_HIDE_INVALID) == "1");

	heightChanged();
	m_originInvalid = true;
}

int TextureBrowser::getFontHeight ()
{
	return GlobalOpenGL().m_fontHeight;
}

const std::string& TextureBrowser::getSelectedShader () const
{
	return shader;
}

/**
 * @brief Updates statusbar with texture information
 */
void TextureBrowser::setStatusText (const std::string& name)
{
	if (g_pParentWnd == 0)
		return;
	IShader* shaderPtr = GlobalShaderSystem().getShaderForName(name);
	qtexture_t* q = shaderPtr->getTexture();
	StringOutputStream strTex(256);
	strTex << name << " W: " << string::toString(q->width) << " H: " << string::toString(q->height);
	shaderPtr->DecRef();
	g_pParentWnd->SetStatusText(g_pParentWnd->m_texture_status, strTex.toString().substr(
			GlobalTexturePrefix_get().length()));
}

void TextureBrowser::setSelectedShader (const std::string& _shader)
{
	shader = _shader;
	if (shader.empty())
		shader = "textures/tex_common/nodraw";
	setStatusText(shader);
	focus(shader);
}

void TextureBrowser::getNextTexturePosition (TextureLayout& layout, const qtexture_t* q, int *x, int *y)
{
	const int nWidth = getTextureWidth(q);
	const int nHeight = getTextureHeight(q);
	// go to the next row unless the texture is the first on the row
	if (layout.current_x + nWidth > width - 8 && layout.current_row) {
		layout.current_x = 8;
		layout.current_y -= layout.current_row + getFontHeight() + 4;
		layout.current_row = 0;
	}

	*x = layout.current_x;
	*y = layout.current_y;

	// Is our texture larger than the row? If so, grow the
	// row height to match it

	if (layout.current_row < nHeight)
		layout.current_row = nHeight;

	// never go less than 96, or the names get all crunched up
	layout.current_x += nWidth < 96 ? 96 : nWidth;
	layout.current_x += 8;
}

// if texture_showinuse jump over non in-use textures
bool TextureBrowser::isTextureShown (const IShader* shader) const
{
	if (!shader_equal_prefix(shader->getName(), GlobalTexturePrefix_get()))
		return false;

	if (m_hideUnused && !shader->IsInUse())
		return false;

	if (m_hideInvalid && !shader->IsValid())
		return false;

	if (!shader_equal_prefix(shader_get_textureName(shader->getName()), currentDirectory))
		return false;

	return true;
}

void TextureBrowser::heightChanged ()
{
	m_heightChanged = true;

	updateScroll();
	queueDraw();
}

void TextureBrowser::evaluateHeight ()
{
	if (!m_heightChanged)
		return;

	m_heightChanged = false;
	m_nTotalHeight = 0;

	TextureLayout layout;
	ShaderSystem &s = GlobalShaderSystem();
	for (s.beginActiveShadersIterator(); !s.endActiveShadersIterator(); s.incrementActiveShadersIterator()) {
		const IShader* shader = s.dereferenceActiveShadersIterator();

		if (!isTextureShown(shader))
			continue;

		int x, y;
		getNextTexturePosition(layout, shader->getTexture(), &x, &y);
		m_nTotalHeight = std::max(m_nTotalHeight, abs(layout.current_y) + getFontHeight() + getTextureHeight(
				shader->getTexture()) + 4);
	}
}

int TextureBrowser::getTotalHeight ()
{
	evaluateHeight();
	return m_nTotalHeight;
}

void TextureBrowser::clampOriginY ()
{
	if (originy > 0) {
		originy = 0;
	}
	const int lower = std::min(height - getTotalHeight(), 0);
	if (originy < lower) {
		originy = lower;
	}
}

int TextureBrowser::getOriginY ()
{
	if (m_originInvalid) {
		m_originInvalid = false;
		clampOriginY();
		updateScroll();
	}
	return originy;
}

void TextureBrowser::setOriginY (int _originy)
{
	originy = _originy;
	clampOriginY();
	updateScroll();
	queueDraw();
}

void TextureBrowser::addActiveShadersChangedCallback (const SignalHandler& handler)
{
	g_activeShadersChangedCallbacks.connectLast(handler);
}

class ShadersObserver: public ModuleObserver
{
		Signal0 m_realiseCallbacks;
	public:
		void realise ()
		{
			m_realiseCallbacks();
		}
		void unrealise ()
		{
		}
		void insert (const SignalHandler& handler)
		{
			m_realiseCallbacks.connectLast(handler);
		}
};

namespace {
ShadersObserver g_ShadersObserver;
}

void TextureBrowser::activeShadersChanged ()
{
	heightChanged();
	m_originInvalid = true;

	g_activeShadersChangedCallbacks();
}

/**
 * @note relies on texture_directory global for the directory to use
 * 1) Load the shaders for the given directory
 * 2) Scan the remaining texture, load them and assign them a default shader (the "noshader" shader)
 * NOTE: when writing a texture plugin, or some texture extensions, this function may need to be overriden, and made
 * available through the IShaders interface
 * NOTE: for texture window layout:
 * all shaders are stored with alphabetical order after load
 * previously loaded and displayed stuff is hidden, only in-use and newly loaded is shown
 * ( the GL textures are not flushed though)
 */
static bool texture_name_ignore (const std::string& name)
{
	if (string::contains(name, "_nm"))
		return true;
	if (string::contains(name, "_gm"))
		return true;
	if (string::contains(name, "_sm"))
		return true;

	return string::contains(name, "tex_terrain") && !string::contains(name, "dummy");
}

class LoadTexturesByTypeVisitor: public ImageModules::Visitor
{
	private:
		const std::string m_dirstring;
		// Modal dialog window to display progress
		gtkutil::ModalProgressDialog _dialog;

		struct TextureDirectoryLoadTextureCaller
		{
				typedef const std::string& first_argument_type;

				const std::string& _directory;
				const gtkutil::ModalProgressDialog& _dialog;

				// Constructor
				TextureDirectoryLoadTextureCaller (const std::string& directory,
						const gtkutil::ModalProgressDialog& dialog) :
					_directory(directory), _dialog(dialog)
				{
				}

				// Functor operator
				void operator() (const std::string& texture)
				{
					std::string name = _directory + os::stripExtension(texture);

					if (texture_name_ignore(name))
						return;

					if (!shader_valid(name)) {
						g_warning("Skipping invalid texture name: [%s]\n", name.c_str());
						return;
					}

					// Update the text in the dialog
					_dialog.setText(name);

					// if a texture is already in use to represent a shader, ignore it
					IShader* shaderPtr = GlobalShaderSystem().getShaderForName(name);
					shaderPtr->DecRef();
				}
		};

	public:
		LoadTexturesByTypeVisitor (const std::string& dirstring) :
			m_dirstring(dirstring), _dialog(MainFrame_getWindow(), _("Loading textures"))
		{
		}
		void visit (const std::string& minor, const _QERPlugImageTable& table) const
		{
			TextureDirectoryLoadTextureCaller functor(m_dirstring, _dialog);
			GlobalFileSystem().forEachFile(m_dirstring, minor, makeCallback1(functor));
		}
};

void TextureBrowser::showDirectory (const std::string& directory)
{
	currentDirectory = directory;
	heightChanged();

	// load remaining texture files
	Radiant_getImageModules().foreachModule(LoadTexturesByTypeVisitor(GlobalTexturePrefix_get() + directory));

	// we'll display the newly loaded textures + all the ones already in use
	GlobalRegistry().set(RKEY_TEXTURES_HIDE_UNUSED, "0");
	GlobalRegistry().set(RKEY_TEXTURES_HIDE_INVALID, "0");
}

void TextureBrowser::showStartupShaders ()
{
	showDirectory("tex_common/");
}

//++timo NOTE: this is a mix of Shader module stuff and texture explorer
// it might need to be split in parts or moved out .. dunno
// scroll origin so the specified texture is completely on screen
// if current texture is not displayed, nothing is changed
void TextureBrowser::focus (const std::string& name)
{
	TextureLayout layout;
	// scroll origin so the texture is completely on screen
	for (GlobalShaderSystem().beginActiveShadersIterator(); !GlobalShaderSystem().endActiveShadersIterator(); GlobalShaderSystem().incrementActiveShadersIterator()) {
		const IShader* shader = GlobalShaderSystem().dereferenceActiveShadersIterator();

		if (!isTextureShown(shader))
			continue;

		int x, y;
		getNextTexturePosition(layout, shader->getTexture(), &x, &y);
		const qtexture_t* q = shader->getTexture();
		if (!q)
			break;

		// we have found when texdef->name and the shader name match
		// NOTE: as everywhere else for our comparisons, we are not case sensitive
		if (shader_equal(name, shader->getName())) {
			const int textureHeight = getTextureHeight(q);

			int originy = getOriginY();
			if (y > originy) {
				originy = y;
			}

			if (y - textureHeight < originy - height) {
				originy = (y - textureHeight) + height;
			}

			setOriginY(originy);
			return;
		}
	}
}

const IShader* TextureBrowser::getTextureAt (int mx, int my)
{
	my += getOriginY() - height;

	TextureLayout layout;
	for (GlobalShaderSystem().beginActiveShadersIterator(); !GlobalShaderSystem().endActiveShadersIterator(); GlobalShaderSystem().incrementActiveShadersIterator()) {
		const IShader* shader = GlobalShaderSystem().dereferenceActiveShadersIterator();

		if (!isTextureShown(shader))
			continue;

		int x, y;
		getNextTexturePosition(layout, shader->getTexture(), &x, &y);
		qtexture_t *q = shader->getTexture();
		if (!q)
			break;

		int nWidth = getTextureWidth(q);
		int nHeight = getTextureHeight(q);
		if (mx > x && mx - x < nWidth && my < y && y - my < nHeight + getFontHeight()) {
			return shader;
		}
	}

	return 0;
}

void TextureBrowser::selectTextureAt (int mx, int my)
{
	const IShader* shader = getTextureAt(mx, my);
	if (shader != 0) {
		setSelectedShader(shader->getName());
		GlobalSurfaceInspector().resetTextureClipboard();

		if (!m_rmbSelected) {
			UndoableCommand undo("textureNameSetSelected");
			Select_SetShader(shader->getName());
		}
	}
}

/*
 ============================================================================

 DRAWING

 ============================================================================
 */

/**
 * @brief relying on the shaders list to display the textures
 * we must query all qtexture_t* to manage and display through the IShaders interface
 * this allows a plugin to completely override the texture system
 */
void TextureBrowser::draw ()
{
	const int originy = getOriginY();

	Vector3 colorBackground = ColourSchemes().getColourVector3("texture_background");
	glClearColor(colorBackground[0], colorBackground[1], colorBackground[2], 0);
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glOrtho(0, width, originy - height, originy, -100, 100);
	glEnable(GL_TEXTURE_2D);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	int last_y = 0, last_height = 0;

	TextureLayout layout;
	for (GlobalShaderSystem().beginActiveShadersIterator(); !GlobalShaderSystem().endActiveShadersIterator(); GlobalShaderSystem().incrementActiveShadersIterator()) {
		const IShader* shader = GlobalShaderSystem().dereferenceActiveShadersIterator();

		if (!isTextureShown(shader))
			continue;

		int x, y;
		getNextTexturePosition(layout, shader->getTexture(), &x, &y);
		const qtexture_t *q = shader->getTexture();
		if (!q)
			break;

		const int nWidth = getTextureWidth(q);
		const int nHeight = getTextureHeight(q);

		if (y != last_y) {
			last_y = y;
			last_height = 0;
		}
		last_height = std::max(nHeight, last_height);

		// Is this texture visible?
		if ((y - nHeight - getFontHeight() < originy) && (y > originy - height)) {
			// borders rules:
			// if it's the current texture, draw a thick red line, else:
			// shaders have a white border, simple textures don't
			// if !texture_showinuse: (some textures displayed may not be in use)
			// draw an additional square around with 0.5 1 0.5 color
			if (shader_equal(getSelectedShader(), shader->getName())) {
				glLineWidth(3);
				if (m_rmbSelected) {
					glColor3f(0, 0, 1);
				} else {
					glColor3f(1, 0, 0);
				}
				glDisable(GL_TEXTURE_2D);

				glBegin(GL_LINE_LOOP);
				glVertex2i(x - 4, y - getFontHeight() + 4);
				glVertex2i(x - 4, y - getFontHeight() - nHeight - 4);
				glVertex2i(x + 4 + nWidth, y - getFontHeight() - nHeight - 4);
				glVertex2i(x + 4 + nWidth, y - getFontHeight() + 4);
				glEnd();

				glEnable(GL_TEXTURE_2D);
				glLineWidth(1);
			} else {
				glLineWidth(1);
				// shader border:
				// TODO: Highlight if material is available for this texture (mattn)
				if (!shader->IsDefault()) {
					glColor3f(1, 1, 1);
					glDisable(GL_TEXTURE_2D);

					glBegin(GL_LINE_LOOP);
					glVertex2i(x - 1, y + 1 - getFontHeight());
					glVertex2i(x - 1, y - nHeight - 1 - getFontHeight());
					glVertex2i(x + 1 + nWidth, y - nHeight - 1 - getFontHeight());
					glVertex2i(x + 1 + nWidth, y + 1 - getFontHeight());
					glEnd();
					glEnable(GL_TEXTURE_2D);
				}

				// highlight in-use textures
				if (!m_hideUnused && shader->IsInUse()) {
					glColor3f(0.5, 1, 0.5);
					glDisable(GL_TEXTURE_2D);
					glBegin(GL_LINE_LOOP);
					glVertex2i(x - 3, y + 3 - getFontHeight());
					glVertex2i(x - 3, y - nHeight - 3 - getFontHeight());
					glVertex2i(x + 3 + nWidth, y - nHeight - 3 - getFontHeight());
					glVertex2i(x + 3 + nWidth, y + 3 - getFontHeight());
					glEnd();
					glEnable(GL_TEXTURE_2D);
				}
				if (!m_hideInvalid && !shader->IsValid()) {
					glColor3f(1, 0, 0);
					glDisable(GL_TEXTURE_2D);
					glBegin(GL_LINE_LOOP);
					glVertex2i(x - 3, y + 3 - getFontHeight());
					glVertex2i(x - 3, y - nHeight - 3 - getFontHeight());
					glVertex2i(x + 3 + nWidth, y - nHeight - 3 - getFontHeight());
					glVertex2i(x + 3 + nWidth, y + 3 - getFontHeight());
					glEnd();
					glEnable(GL_TEXTURE_2D);
				}
			}

			// Draw the texture
			glBindTexture(GL_TEXTURE_2D, q->texture_number);
			glColor3f(1, 1, 1);
			glBegin(GL_QUADS);
			glTexCoord2i(0, 0);
			glVertex2i(x, y - getFontHeight());
			glTexCoord2i(1, 0);
			glVertex2i(x + nWidth, y - getFontHeight());
			glTexCoord2i(1, 1);
			glVertex2i(x + nWidth, y - getFontHeight() - nHeight);
			glTexCoord2i(0, 1);
			glVertex2i(x, y - getFontHeight() - nHeight);
			glEnd();

			// draw the texture name
			glDisable(GL_TEXTURE_2D);
			glColor3f(1, 1, 1);

			glRasterPos2i(x, y - getFontHeight() + 5);

			// don't draw the directory name
			std::string name = std::string(shader->getName());
			name = name.substr(name.rfind("/") + 1);

			// Ellipsize the name if it's too long
			unsigned int maxNameLength = 32;
			if (name.size() > maxNameLength) {
				name = name.substr(0, maxNameLength / 2) + "..." + name.substr(name.size() - maxNameLength / 2);
			}

			GlobalOpenGL().drawString(name);
			glEnable(GL_TEXTURE_2D);
		}

		//int totalHeight = abs(y) + last_height + fontHeight() + 4;
	}

	// reset the current texture
	glBindTexture(GL_TEXTURE_2D, 0);
}

void TextureBrowser::setUniformSize (int value)
{
	m_uniformTextureSize = value;
	heightChanged();
}

void TextureBrowser::queueDraw ()
{
	GtkWidget *glWidget = _glWidget;
	gtk_widget_queue_draw(glWidget);
}

void TextureBrowser::setScale (std::size_t scale)
{
	m_textureScale = scale;

	queueDraw();
}

void TextureBrowser::onMouseWheel (bool bUp)
{
	int originy = getOriginY();

	if (bUp) {
		originy += 64;
	} else {
		originy -= 64;
	}

	setOriginY(originy);
}

// GTK callback for toggling uniform texture sizing
void TextureBrowser::onToggleResizeTextures (GtkToggleToolButton* button, TextureBrowser* textureBrowser)
{
	if (gtk_toggle_tool_button_get_active(button) == TRUE) {
		textureBrowser->m_resizeTextures = true;
	} else {
		textureBrowser->m_resizeTextures = false;
	}

	// Update texture browser
	textureBrowser->heightChanged();
}

gboolean TextureBrowser::onButtonPress (GtkWidget* widget, GdkEventButton* event, TextureBrowser* textureBrowser)
{
	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 1) {
			const int pointx = static_cast<int> (event->x);
			const int pointy = static_cast<int> (event->y);
			textureBrowser->selectTextureAt(pointx, textureBrowser->height - 1 - pointy);
		}
	}
	return FALSE;
}

gboolean TextureBrowser::onButtonRelease (GtkWidget* widget, GdkEventButton* event, TextureBrowser* textureBrowser)
{
	if (event->type == GDK_BUTTON_RELEASE) {
		if (event->button == 1) {
		}
	}
	return FALSE;
}

gboolean TextureBrowser::onMouseMotion (GtkWidget *widget, GdkEventMotion *event, TextureBrowser* textureBrowser)
{
	return FALSE;
}

gboolean TextureBrowser::onMouseScroll (GtkWidget* widget, GdkEventScroll* event, TextureBrowser* textureBrowser)
{
	if (event->direction == GDK_SCROLL_UP) {
		textureBrowser->onMouseWheel(true);
	} else if (event->direction == GDK_SCROLL_DOWN) {
		textureBrowser->onMouseWheel(false);
	}
	return FALSE;
}

void TextureBrowser_scrollChanged (void* data, gdouble value)
{
	TextureBrowser *textureBrowser = reinterpret_cast<TextureBrowser*> (data);
	textureBrowser->setOriginY(-(int) value);
}

void TextureBrowser::onVerticalScroll (GtkAdjustment *adjustment, TextureBrowser* textureBrowser)
{
	textureBrowser->m_scrollAdjustment.value_changed(adjustment->value);
}

void TextureBrowser::updateScroll ()
{
	int totalHeight = getTotalHeight();

	totalHeight = std::max(totalHeight, height);

	GtkAdjustment *vadjustment = gtk_range_get_adjustment(GTK_RANGE(m_texture_scroll));
	if (!vadjustment)
		return;
	vadjustment->value = -getOriginY();
	vadjustment->page_size = height;
	vadjustment->page_increment = height / 2;
	vadjustment->step_increment = 20;
	vadjustment->lower = 0;
	vadjustment->upper = totalHeight;

	g_signal_emit_by_name(G_OBJECT (vadjustment), "changed");
}

gboolean TextureBrowser::onSizeAllocate (GtkWidget* widget, GtkAllocation* allocation, TextureBrowser* textureBrowser)
{
	textureBrowser->width = allocation->width;
	textureBrowser->height = allocation->height;
	textureBrowser->heightChanged();
	textureBrowser->m_originInvalid = true;
	textureBrowser->queueDraw();
	return FALSE;
}

gboolean TextureBrowser::onExpose (GtkWidget* widget, GdkEventExpose* event, TextureBrowser* textureBrowser)
{
	GtkWidget* glWidget = textureBrowser->_glWidget;
	gtkutil::GLWidgetSentry sentry(glWidget);
	textureBrowser->evaluateHeight();
	textureBrowser->draw();
	return FALSE;
}

namespace {
struct TextureFunctor
{
		typedef const std::string& first_argument_type;

		// TextureGroups to populate
		TextureGroups& _groups;

		// Constructor
		TextureFunctor (TextureGroups& groups) :
			_groups(groups)
		{
		}

		// Functor operator
		void operator() (const std::string& file)
		{
			const std::string texture = os::makeRelative(file, GlobalTexturePrefix_get());
			if (texture != file) {
				std::string filename = os::getFilenameFromPath(file);
				if (!filename.empty()) {
					std::string path = os::stripFilename(texture);
					_groups.insert(path);
				}
			}
		}
};

struct TextureDirectoryFunctor
{
		typedef const std::string& first_argument_type;

		// TextureGroups to populate
		TextureGroups& _groups;

		// Constructor
		TextureDirectoryFunctor (TextureGroups& groups) :
			_groups(groups)
		{
		}

		// Functor operator
		void operator() (const std::string& directory)
		{
			// skip none texture subdirs
			if (!string::startsWith(directory, "tex_"))
				return;

			_groups.insert(directory);
		}
};
}

void TextureBrowser::constructTreeView ()
{
	TextureDirectoryFunctor functorDirs(groups);
	GlobalFileSystem().forEachDirectory(GlobalTexturePrefix_get(), makeCallback1(functorDirs));
	TextureFunctor functorTextures(groups);
	GlobalShaderSystem().foreachShaderName(makeCallback1(functorTextures));
}

GtkMenuItem* TextureBrowser::constructViewMenu (GtkMenu* menu)
{
	GtkMenuItem* textures_menu_item = new_sub_menu_item_with_mnemonic(_("_View"));

	createCheckMenuItemWithMnemonic(menu, _("Hide Invalid"), "ShowInvalid");

	createSeparatorMenuItem(menu);
	createMenuItemWithMnemonic(menu, _("Show All"), "ShowAllTextures");

	return textures_menu_item;
}

void TextureBrowser::onActivateDirectoryChange (GtkMenuItem* item, TextureBrowser* textureBrowser)
{
	const std::string& dirname = std::string((const gchar*) g_object_get_data(G_OBJECT(item), "directory")) + "/";
	textureBrowser->showDirectory(dirname);
	textureBrowser->queueDraw();
}

GtkMenuItem* TextureBrowser::constructDirectoriesMenu (GtkMenu* menu)
{
	GtkMenuItem* textures_menu_item = new_sub_menu_item_with_mnemonic(_("_Directories"));

	constructTreeView();

	TextureGroups::const_iterator i = groups.begin();
	while (i != groups.end()) {
		const gchar* directory = (*i).c_str();
		GtkWidget* item = gtk_menu_item_new_with_label(directory);
		g_object_set_data(G_OBJECT(item), "directory", (gpointer) directory);
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(onActivateDirectoryChange), this);
		gtk_container_add(GTK_CONTAINER(menu), item);
		++i;
	}

	return textures_menu_item;
}

GtkWidget* TextureBrowser::createMenuBar ()
{
	GtkWidget* menu_bar = gtk_menu_bar_new();
	GtkWidget* menu_view = gtk_menu_new();
	GtkWidget* view_item = (GtkWidget*) constructViewMenu(GTK_MENU(menu_view));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), menu_view);
	gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), view_item);

	GtkWidget* menu_directories = gtk_menu_new();
	GtkWidget* directories_item = (GtkWidget*) constructDirectoriesMenu(GTK_MENU(menu_directories));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(directories_item), menu_directories);
	gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), directories_item);

	return menu_bar;
}

GtkWidget* TextureBrowser::createToolBar ()
{
	// Load the texture toolbar from the registry
	ui::ToolbarCreator toolbarCreator;
	GtkToolbar* textureToolbar = toolbarCreator.getToolbar("texture");
	if (textureToolbar != NULL) {
		gtk_widget_show(GTK_WIDGET(textureToolbar));

		// Button for toggling the resizing of textures
		GtkToolItem* sizeToggle = gtk_toggle_tool_button_new();
		GdkPixbuf* pixBuf = gtkutil::getLocalPixbuf("texwindow_uniformsize.png");
		GtkWidget* toggle_image = GTK_WIDGET(gtk_image_new_from_pixbuf(pixBuf));

		GtkTooltips* barTips = gtk_tooltips_new();
		gtk_tool_item_set_tooltip(sizeToggle, barTips, _("Clamp texture thumbnails to constant size"), "");

		gtk_tool_button_set_label(GTK_TOOL_BUTTON(sizeToggle), _("Constant size"));
		gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(sizeToggle), toggle_image);
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(sizeToggle), TRUE);

		// Insert button and connect callback
		gtk_toolbar_insert(GTK_TOOLBAR(textureToolbar), sizeToggle, 0);
		g_signal_connect(G_OBJECT(sizeToggle),
				"toggled",
				G_CALLBACK(onToggleResizeTextures),
				this);

		gdk_pixbuf_unref(pixBuf);
	}

	return GTK_WIDGET(textureToolbar);
}

GtkWidget* TextureBrowser::getWidget ()
{
	GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), createMenuBar(), false, true, 0);
	gtk_box_pack_start(GTK_BOX(vbox), createToolBar(), false, true, 0);

	GtkWidget* texhbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), texhbox, true, true, 0);

	m_texture_scroll = gtk_vscrollbar_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 1, 1, 1)));

	GtkAdjustment *vadjustment = gtk_range_get_adjustment(GTK_RANGE(m_texture_scroll));
	g_signal_connect(G_OBJECT(vadjustment), "value_changed", G_CALLBACK(onVerticalScroll),
			this);

	widget_set_visible(m_texture_scroll, true);

	gtk_box_pack_end(GTK_BOX(texhbox), m_texture_scroll, false, true, 0);

	{ // gl_widget
		GtkWidget *glWidget = _glWidget;

		gtk_widget_set_events(glWidget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
				| GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
		GTK_WIDGET_SET_FLAGS(glWidget, GTK_CAN_FOCUS);
		gtk_widget_show(glWidget);

		g_signal_connect(G_OBJECT(glWidget), "size_allocate",
				G_CALLBACK(onSizeAllocate), this);
		g_signal_connect(G_OBJECT(glWidget), "expose_event",
				G_CALLBACK(onExpose), this);
		g_signal_connect(G_OBJECT(glWidget), "button_press_event", G_CALLBACK(
						onButtonPress), this);
		g_signal_connect(G_OBJECT(glWidget), "button_release_event", G_CALLBACK(
						onButtonRelease), this);
		g_signal_connect(G_OBJECT(glWidget), "motion_notify_event", G_CALLBACK(
						onMouseMotion), this);
		g_signal_connect(G_OBJECT(glWidget), "scroll_event", G_CALLBACK(onMouseScroll),
				this);
		gtk_box_pack_start(GTK_BOX(texhbox), glWidget, true, true, 0);
	}

	gtk_container_unset_focus_chain(GTK_CONTAINER(vbox));

	gtk_widget_show_all(vbox);
	return vbox;
}

void TextureBrowser::showAll ()
{
	currentDirectory = "";
	heightChanged();
}

void TextureBrowser::refreshShaders ()
{
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Loading Shaders"));
	GlobalShaderSystem().refresh();
	showDirectory(currentDirectory);
	UpdateAllWindows();
}

void TextureScaleImport (TextureBrowser& textureBrowser, int value)
{
	int val;
	switch (value) {
	case 0:
		val = 10;
		break;
	case 1:
		val = 25;
		break;
	case 2:
		val = 50;
		break;
	case 3:
		val = 100;
		break;
	case 4:
		val = 200;
		break;
	default:
		return;
	}
	textureBrowser.setScale(val);
}
typedef ReferenceCaller1<TextureBrowser, int, TextureScaleImport> TextureScaleImportCaller;

void TextureScaleExport (TextureBrowser& textureBrowser, const IntImportCallback& importer)
{
	switch (textureBrowser.m_textureScale) {
	case 10:
		importer(0);
		break;
	case 25:
		importer(1);
		break;
	case 50:
		importer(2);
		break;
	case 100:
		importer(3);
		break;
	case 200:
		importer(4);
		break;
	}
}
typedef ReferenceCaller1<TextureBrowser, const IntImportCallback&, TextureScaleExport> TextureScaleExportCaller;

// Get the texture size preference from the prefs dialog
void TextureUniformSizeImport (TextureBrowser& textureBrowser, int value)
{
	textureBrowser.setUniformSize(value);
}
typedef ReferenceCaller1<TextureBrowser, int, TextureUniformSizeImport> TextureUniformSizeImportCaller;

// Set the value of the texture size preference widget in the prefs dialog
void TextureUniformSizeExport (TextureBrowser& textureBrowser, const IntImportCallback& importer)
{
	importer(textureBrowser.m_uniformTextureSize);
}
typedef ReferenceCaller1<TextureBrowser, const IntImportCallback&, TextureUniformSizeExport>
		TextureUniformSizeExportCaller;

void TextureBrowser::constructPage (PreferenceGroup& group)
{
	PreferencesPage* page = group.createPage(_("Texture Browser"), _("Texture Browser Preferences"));
	PrefPage*p = reinterpret_cast<PrefPage*> (page);
	const char* texture_scale[] = { "10%", "25%", "50%", "100%", "200%" };
	p->appendCombo(_("Texture thumbnail scale"), STRING_ARRAY_RANGE(texture_scale), IntImportCallback(
			TextureScaleImportCaller(*this)), IntExportCallback(TextureScaleExportCaller(*this)));
	p->appendEntry(_("Uniform texture thumbnail size (pixels)"), IntImportCallback(
			TextureUniformSizeImportCaller(*this)), IntExportCallback(TextureUniformSizeExportCaller(*this)));
}

void TextureBrowser::registerCommands ()
{
	GlobalEventManager().addCommand("RefreshShaders", MemberCaller<TextureBrowser, &TextureBrowser::refreshShaders> (
			*this));
	GlobalEventManager().addCommand("ShowAllTextures", MemberCaller<TextureBrowser, &TextureBrowser::showAll> (*this));
}

#include "preferencesystem.h"
#include "stringio.h"

void TextureBrowser_Construct ()
{
	GlobalTextureBrowser().registerCommands();

	GlobalEventManager().addRegistryToggle("ShowInUse", RKEY_TEXTURES_HIDE_UNUSED);
	GlobalEventManager().addRegistryToggle("ShowInvalid", RKEY_TEXTURES_HIDE_INVALID);

	GlobalPreferenceSystem().registerPreference("TextureScale", makeSizeStringImportCallback(MemberCaller1<
			TextureBrowser, std::size_t, &TextureBrowser::setScale> (GlobalTextureBrowser())), SizeExportStringCaller(
			GlobalTextureBrowser().m_textureScale));
	GlobalPreferenceSystem().registerPreference("TextureUniformSize", IntImportStringCaller(
			GlobalTextureBrowser().m_uniformTextureSize), IntExportStringCaller(
			GlobalTextureBrowser().m_uniformTextureSize));

	PreferencesDialog_addSettingsPage(MemberCaller1<TextureBrowser, PreferenceGroup&, &TextureBrowser::constructPage> (
			GlobalTextureBrowser()));

	GlobalShaderSystem().attach(g_ShadersObserver);
}

void TextureBrowser_Destroy ()
{
	GlobalShaderSystem().detach(g_ShadersObserver);

	GlobalShaderSystem().setActiveShadersChangedNotify(Callback());
	Textures_setModeChangedNotify(Callback());
}

TextureBrowser& GlobalTextureBrowser ()
{
	static TextureBrowser g_textureBrowser;
	return g_textureBrowser;
}
