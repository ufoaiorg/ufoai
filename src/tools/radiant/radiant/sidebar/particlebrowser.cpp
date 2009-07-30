/**
 * @file particlebrowser.cpp
 */

/*
 Copyright (C) 1997-2008 UFO:AI Team

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include "particlebrowser.h"
#include "radiant_i18n.h"

#include "ifilesystem.h"
#include "iundo.h"
#include "igl.h"
#include "iarchive.h"
#include "moduleobserver.h"
#include "texturelib.h"

#include <set>
#include <string>
#include <vector>

#include "signal/signal.h"
#include "math/vector.h"
#include "string/string.h"
#include "os/file.h"
#include "os/path.h"
#include "stream/memstream.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "particles.h"

#include "gtkutil/nonmodal.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/messagebox.h"

void ParticleBrowser_scrollChanged(void* data, gdouble value);

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
	static void adjustment_value_changed(GtkAdjustment *adjustment,
			DeferredAdjustment* self) {
		self->value_changed(adjustment->value);
	}
};

class ParticleBrowser {
public:
	int width, height;
	int originy;
	int m_nTotalHeight;

	GtkWindow* m_parent;
	GtkWidget* m_gl_widget;
	GtkWidget* m_texture_scroll;
	GtkWidget* m_treeViewTree;
	GtkWidget* m_tag_frame;
	GtkListStore* m_assigned_store;
	GtkListStore* m_available_store;
	GtkWidget* m_assigned_tree;
	GtkWidget* m_available_tree;
	GtkWidget* m_scr_win_tree;
	GtkWidget* m_scr_win_tags;
	GtkWidget* m_tag_notebook;
	GtkWidget* m_search_button;

	GtkListStore* m_all_tags_list;

	guint m_sizeHandler;
	guint m_exposeHandler;

	bool m_heightChanged;
	bool m_originInvalid;

	DeferredAdjustment m_scrollAdjustment;

	Vector3 color_textureback;
	// the increment step we use against the wheel mouse
	std::size_t m_mouseWheelScrollIncrement;
	std::size_t m_textureScale;
	// make the texture increments match the grid changes
	bool m_showShaders;
	bool m_showTextureScrollbar;
	// if true, the texture window will only display in-use shaders
	// if false, all the shaders in memory are displayed
	bool m_hideUnused;
	bool m_rmbSelected;
	// The uniform size (in pixels) that textures are resized to when m_resizeTextures is true.
	int m_uniformTextureSize;
	// Return the display width of a texture in the texture browser
	int getTextureWidth(const qtexture_t* tex) {
		// Don't use uniform size
		const int width = (int) (tex->width * ((float) m_textureScale / 100));
		return width;
	}
	// Return the display height of a texture in the texture browser
	int getTextureHeight(const qtexture_t* tex) {
		// Don't use uniform size
		const int height = (int) (tex->height * ((float) m_textureScale / 100));
		return height;
	}

	ParticleBrowser() :
		m_texture_scroll(0), m_heightChanged(true), m_originInvalid(true),
				m_scrollAdjustment(ParticleBrowser_scrollChanged, this),
				color_textureback(0.25f, 0.25f, 0.25f),
				m_mouseWheelScrollIncrement(64), m_textureScale(50),
				m_showTextureScrollbar(true) {
	}
};

class TextureLayout
{
	public:
		// texture layout functions
		// TTimo: now based on shaders
		int current_x, current_y, current_row;
};

static ParticleBrowser g_ParticleBrowser;
static void ParticleBrowser_updateScroll(ParticleBrowser& ParticleBrowser);

static void ParticleBrowser_evaluateHeight (ParticleBrowser& ParticleBrowser)
{
	if (ParticleBrowser.m_heightChanged) {
		ParticleBrowser.m_heightChanged = false;
	}
}

static int ParticleBrowser_TotalHeight (ParticleBrowser& ParticleBrowser)
{
	ParticleBrowser_evaluateHeight(ParticleBrowser);
	return ParticleBrowser.m_nTotalHeight;
}

static void ParticleBrowser_queueDraw (ParticleBrowser& ParticleBrowser)
{
	if (ParticleBrowser.m_gl_widget != 0) {
		gtk_widget_queue_draw(ParticleBrowser.m_gl_widget);
	}
}

static void ParticleBrowser_clampOriginY (ParticleBrowser& ParticleBrowser)
{
	if (ParticleBrowser.originy > 0) {
		ParticleBrowser.originy = 0;
	}
	const int lower = std::min(ParticleBrowser.height - ParticleBrowser_TotalHeight(ParticleBrowser), 0);
	if (ParticleBrowser.originy < lower) {
		ParticleBrowser.originy = lower;
	}
}

static int ParticleBrowser_getOriginY (ParticleBrowser& particleBrowser)
{
	if (particleBrowser.m_originInvalid) {
		particleBrowser.m_originInvalid = false;
		ParticleBrowser_clampOriginY(particleBrowser);
		ParticleBrowser_updateScroll(particleBrowser);
	}
	return particleBrowser.originy;
}

static void ParticleBrowser_setOriginY (ParticleBrowser& particleBrowser, int originy)
{
	particleBrowser.originy = originy;
	ParticleBrowser_clampOriginY(particleBrowser);
	ParticleBrowser_updateScroll(particleBrowser);
	ParticleBrowser_queueDraw(particleBrowser);
}

static inline int ParticleBrowser_fontHeight (ParticleBrowser& ParticleBrowser)
{
	return GlobalOpenGL().m_fontHeight;
}

static void ParticleBrowser_Selection_MouseDown (ParticleBrowser& ParticleBrowser, guint32 flags, int pointx, int pointy)
{
}

static void Particle_Draw (ParticleBrowser& ParticleBrowser)
{
	int x, y;
	glClearColor(ParticleBrowser.color_textureback[0], ParticleBrowser.color_textureback[1],
			ParticleBrowser.color_textureback[2], 0);
	glViewport(0, 0, ParticleBrowser.width, ParticleBrowser.height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


	// draw the particle name
	glDisable(GL_TEXTURE_2D);
	glColor3f(1, 1, 1);

	glRasterPos2i(x, y - ParticleBrowser_fontHeight(ParticleBrowser) + 5);

	GlobalOpenGL().drawString("particleName");
	glEnable(GL_TEXTURE_2D);

	// reset the current texture
	glBindTexture(GL_TEXTURE_2D, 0);
}

void ParticleBrowser_setScale (ParticleBrowser& ParticleBrowser, std::size_t scale)
{
	ParticleBrowser.m_textureScale = scale;

	ParticleBrowser_queueDraw(ParticleBrowser);
}

void ParticleBrowser_MouseWheel (ParticleBrowser& particleBrowser, bool bUp)
{
	int originy = ParticleBrowser_getOriginY(particleBrowser);

	if (bUp) {
		originy += int(particleBrowser.m_mouseWheelScrollIncrement);
	} else {
		originy -= int(particleBrowser.m_mouseWheelScrollIncrement);
	}

	ParticleBrowser_setOriginY(particleBrowser, originy);
}

gboolean ParticleBrowser_button_press (GtkWidget* widget, GdkEventButton* event, ParticleBrowser* ParticleBrowser)
{
	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 1) {
			ParticleBrowser_Selection_MouseDown(*ParticleBrowser, event->state, static_cast<int> (event->x),
					static_cast<int> (event->y));
		}
	}
	return FALSE;
}

gboolean ParticleBrowser_button_release (GtkWidget* widget, GdkEventButton* event, ParticleBrowser* ParticleBrowser)
{
	if (event->type == GDK_BUTTON_RELEASE) {
		if (event->button == 1) {
		}
	}
	return FALSE;
}

gboolean ParticleBrowser_motion (GtkWidget *widget, GdkEventMotion *event, ParticleBrowser* particleBrowser)
{
	return FALSE;
}

gboolean ParticleBrowser_scroll (GtkWidget* widget, GdkEventScroll* event, ParticleBrowser* particleBrowser)
{
	if (event->direction == GDK_SCROLL_UP) {
		ParticleBrowser_MouseWheel(*particleBrowser, true);
	} else if (event->direction == GDK_SCROLL_DOWN) {
		ParticleBrowser_MouseWheel(*particleBrowser, false);
	}
	return FALSE;
}

void ParticleBrowser_scrollChanged (void* data, gdouble value)
{
	//globalOutputStream() << "vertical scroll\n";
	ParticleBrowser_setOriginY(*reinterpret_cast<ParticleBrowser*> (data), -(int) value);
}

static void ParticleBrowser_verticalScroll (GtkAdjustment *adjustment, ParticleBrowser* particleBrowser)
{
	particleBrowser->m_scrollAdjustment.value_changed(adjustment->value);
}

static void ParticleBrowser_updateScroll (ParticleBrowser& ParticleBrowser)
{
	if (ParticleBrowser.m_showTextureScrollbar) {
		int totalHeight = ParticleBrowser_TotalHeight(ParticleBrowser);

		totalHeight = std::max(totalHeight, ParticleBrowser.height);

		GtkAdjustment *vadjustment = gtk_range_get_adjustment(GTK_RANGE(ParticleBrowser.m_texture_scroll));

		vadjustment->value = -ParticleBrowser_getOriginY(ParticleBrowser);
		vadjustment->page_size = ParticleBrowser.height;
		vadjustment->page_increment = ParticleBrowser.height / 2;
		vadjustment->step_increment = 20;
		vadjustment->lower = 0;
		vadjustment->upper = totalHeight;

		g_signal_emit_by_name(G_OBJECT (vadjustment), "changed");
	}
}

static void ParticleBrowser_heightChanged (ParticleBrowser& particleBrowser)
{
	particleBrowser.m_heightChanged = true;

	ParticleBrowser_updateScroll(particleBrowser);
	ParticleBrowser_queueDraw(particleBrowser);
}

static gboolean ParticleBrowser_size_allocate (GtkWidget* widget, GtkAllocation* allocation,
		ParticleBrowser* ParticleBrowser)
{
	ParticleBrowser->width = allocation->width;
	ParticleBrowser->height = allocation->height;
	ParticleBrowser_heightChanged(*ParticleBrowser);
	ParticleBrowser->m_originInvalid = true;
	ParticleBrowser_queueDraw(*ParticleBrowser);
	return FALSE;
}

static gboolean ParticleBrowser_expose (GtkWidget* widget, GdkEventExpose* event, ParticleBrowser* ParticleBrowser)
{
	if (glwidget_make_current(ParticleBrowser->m_gl_widget) != FALSE) {
		ParticleBrowser_evaluateHeight(*ParticleBrowser);
		Particle_Draw(*ParticleBrowser);
		glwidget_swap_buffers(ParticleBrowser->m_gl_widget);
	}
	return FALSE;
}

static void ParticleBrowser_constructTreeStore (void)
{
	GtkTreeStore* store = gtk_tree_store_new(1, G_TYPE_STRING);
	GtkTreeIter iter;

	Particles_Init();

	for (ParticleDefinitionMap::const_iterator i = g_particleDefinitions.begin(); i != g_particleDefinitions.end(); ++i) {
		gtk_tree_store_append(store, &iter, (GtkTreeIter*)0);
		gtk_tree_store_set(store, &iter, 0, (*i).first.c_str(), -1);
	}

	Particles_Shutdown();

	GtkTreeModel* model = GTK_TREE_MODEL(store);

	gtk_tree_view_set_model(GTK_TREE_VIEW(g_ParticleBrowser.m_treeViewTree), model);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(g_ParticleBrowser.m_treeViewTree));

	g_object_unref(G_OBJECT(store));
}

static void TreeView_onRowActivated (GtkTreeView* treeview, GtkTreePath* path, GtkTreeViewColumn* col,
		gpointer userdata)
{
	GtkTreeIter iter;

	GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

	if (gtk_tree_model_get_iter(model, &iter, path)) {
		gchar particleName[1024];

		gchar* buffer;
		gtk_tree_model_get(model, &iter, 0, &buffer, -1);
		strcpy(particleName, buffer);
		g_free(buffer);

		// TODO: Load the particle

		ParticleBrowser_queueDraw(g_ParticleBrowser);
	}
}

static void ParticleBrowser_createTreeViewTree (void)
{
	GtkCellRenderer* renderer;
	g_ParticleBrowser.m_treeViewTree = GTK_WIDGET(gtk_tree_view_new());
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(g_ParticleBrowser.m_treeViewTree), TRUE);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(g_ParticleBrowser.m_treeViewTree), FALSE);
	g_signal_connect(g_ParticleBrowser.m_treeViewTree, "row-activated", (GCallback) TreeView_onRowActivated, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(g_ParticleBrowser.m_treeViewTree), -1, "", renderer, "text", 0, (char const*)0);

	ParticleBrowser_constructTreeStore();
}

GtkWidget* ParticleBrowser_constructNotebookTab() {
	GtkWidget* table = gtk_table_new(3, 3, FALSE);
	GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 1, 3, GTK_FILL, GTK_FILL, 0,
			0);
	gtk_widget_show(vbox);

	{ // Particle TreeView
		g_ParticleBrowser.m_scr_win_tree = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(
				GTK_CONTAINER(g_ParticleBrowser.m_scr_win_tree), 0);

		// vertical only scrolling for treeview
		gtk_scrolled_window_set_policy(
				GTK_SCROLLED_WINDOW(g_ParticleBrowser.m_scr_win_tree),
				GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

		gtk_widget_show(g_ParticleBrowser.m_scr_win_tree);

		ParticleBrowser_createTreeViewTree();

		gtk_scrolled_window_add_with_viewport(
				GTK_SCROLLED_WINDOW(g_ParticleBrowser.m_scr_win_tree),
				GTK_WIDGET(
						g_ParticleBrowser.m_treeViewTree));
		gtk_widget_show(GTK_WIDGET(g_ParticleBrowser.m_treeViewTree));
	}
	{ // gl_widget scrollbar
		GtkWidget* w = gtk_vscrollbar_new(
				GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 1, 1, 1)));
		gtk_table_attach(GTK_TABLE(table), w, 2, 3, 1, 2, GTK_SHRINK, GTK_FILL,
				0, 0);
		gtk_widget_show(w);
		g_ParticleBrowser.m_texture_scroll = w;

		GtkAdjustment *vadjustment = gtk_range_get_adjustment(
				GTK_RANGE(g_ParticleBrowser.m_texture_scroll));
		g_signal_connect(G_OBJECT(vadjustment), "value_changed", G_CALLBACK(ParticleBrowser_verticalScroll),
				&g_ParticleBrowser);

		widget_set_visible(g_ParticleBrowser.m_texture_scroll,
				g_ParticleBrowser.m_showTextureScrollbar);
	}
	{ // gl_widget
		g_ParticleBrowser.m_gl_widget = glwidget_new(TRUE);
		gtk_widget_ref(g_ParticleBrowser.m_gl_widget);

		gtk_widget_set_events(g_ParticleBrowser.m_gl_widget, GDK_DESTROY
				| GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
				| GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK
				| GDK_SCROLL_MASK);
		GTK_WIDGET_SET_FLAGS(g_ParticleBrowser.m_gl_widget, GTK_CAN_FOCUS);

		gtk_table_attach_defaults(GTK_TABLE(table),
				g_ParticleBrowser.m_gl_widget, 1, 2, 1, 2);
		gtk_widget_show(g_ParticleBrowser.m_gl_widget);

		g_ParticleBrowser.m_sizeHandler
				= g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "size_allocate",
						G_CALLBACK(ParticleBrowser_size_allocate), &g_ParticleBrowser);
		g_ParticleBrowser.m_exposeHandler
				= g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "expose_event",
						G_CALLBACK(ParticleBrowser_expose), &g_ParticleBrowser);

		g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "button_press_event", G_CALLBACK(
						ParticleBrowser_button_press), &g_ParticleBrowser);
		g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "button_release_event", G_CALLBACK(
						ParticleBrowser_button_release), &g_ParticleBrowser);
		g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "motion_notify_event", G_CALLBACK(
						ParticleBrowser_motion), &g_ParticleBrowser);
		g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "scroll_event", G_CALLBACK(ParticleBrowser_scroll),
				&g_ParticleBrowser);
	}

	gtk_box_pack_start(GTK_BOX(vbox), g_ParticleBrowser.m_scr_win_tree, TRUE,
			TRUE, 0);

	return table;
}
