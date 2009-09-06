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
#include "math/Vector3.h"
#include "string/string.h"
#include "os/file.h"
#include "os/path.h"
#include "stream/memstream.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "particles.h"

#include "../mainframe.h"
#include "../referencecache.h"

#include "gtkutil/nonmodal.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/messagebox.h"

#include "../ui/common/ModelPreview.h"

static ParticleDefinition *g_currentSelectedParticle;

class ParticleBrowser
{
	public:
		int width, height;

		GtkWindow* m_parent;
		GtkWidget* m_gl_widget;
		GtkWidget* m_treeViewTree;
		GtkWidget* m_scr_win_tree;
		GtkWidget* m_scr_win_tags;

		guint m_sizeHandler;
		guint m_exposeHandler;

		Vector3 color_textureback;
		// the increment step we use against the wheel mouse
		std::size_t m_textureScale;
		// Return the display width of a texture in the texture browser
		int getTextureWidth (const qtexture_t* tex)
		{
			return 128;
			// Don't use uniform size
			//const int width = (int) (tex->width * ((float) m_textureScale / 100));
			//return width;
		}
		// Return the display height of a texture in the texture browser
		int getTextureHeight (const qtexture_t* tex)
		{
			return 128;
			// Don't use uniform size
			//const int height = (int) (tex->height * ((float) m_textureScale / 100));
			//return height;
		}

		ParticleBrowser () :
			color_textureback(0.25f, 0.25f, 0.25f), m_textureScale(50)
		{
		}
};

static ParticleBrowser g_ParticleBrowser;

static void ParticleBrowser_queueDraw (ParticleBrowser& particleBrowser)
{
	if (particleBrowser.m_gl_widget != 0) {
		gtk_widget_queue_draw(particleBrowser.m_gl_widget);
	}
}

static void Particle_SetBlendMode (ParticleDefinition *particle)
{
	switch (particle->m_blend) {
	case BLEND_REPLACE:
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		break;
	case BLEND_ONE_PARTICLE:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;
	case BLEND_BLEND:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case BLEND_ADD:
		glBlendFunc(GL_ONE, GL_ONE);
		break;
	case BLEND_FILTER:
		glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		break;
	case BLEND_INVFILTER:
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		break;
	}
}

static void Particle_Draw (ParticleBrowser& particleBrowser)
{
	ParticleDefinition *particle = g_currentSelectedParticle;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glClearColor(particleBrowser.color_textureback[0], particleBrowser.color_textureback[1],
			particleBrowser.color_textureback[2], 0);
	glViewport(0, 0, particleBrowser.width, particleBrowser.height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glOrtho(0, particleBrowser.width, particleBrowser.height, 0, -100, 100);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if (particle) {
		const int fontGap = GlobalOpenGL().m_fontHeight;
		int x = 10, y = 0;

		// Draw the texture
		const qtexture_t *q = g_currentSelectedParticle->m_image;
		if (q) {
			int nWidth = particle->m_width ? particle->m_width : particleBrowser.getTextureWidth(q);
			int nHeight = particle->m_height ? particle->m_height : particleBrowser.getTextureHeight(q);

			// should be at least visible - and some might really be very small
			if (nWidth < 10)
				nWidth *= 3;
			if (nHeight < 10)
				nHeight *= 3;

			x = (particleBrowser.width) / 2 - (nWidth / 2);
			y = (particleBrowser.height) / 2 - (nHeight / 2);

			Particle_SetBlendMode(particle);

			glBindTexture(GL_TEXTURE_2D, q->texture_number);
			glColor4f(1, 1, 1, 1);
			glBegin(GL_QUADS);
			glTexCoord2i(0, 0);
			glVertex2i(x, y - fontGap);
			glTexCoord2i(1, 0);
			glVertex2i(x + nWidth, y - fontGap);
			glTexCoord2i(1, 1);
			glVertex2i(x + nWidth, y - fontGap - nHeight);
			glTexCoord2i(0, 1);
			glVertex2i(x, y - fontGap - nHeight);
			glEnd();

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		// Draw the model
		if (particle->m_model) {
			//particle->m_model;
		}

		// draw the particle name
		glDisable(GL_TEXTURE_2D);
		glColor4f(1, 1, 1, 1);
		glRasterPos2i(10, particleBrowser.height - fontGap + 5);
		GlobalOpenGL().drawString(particle->m_id);
		glEnable(GL_TEXTURE_2D);
	}

	// reset the current texture
	glBindTexture(GL_TEXTURE_2D, 0);

	glPopAttrib();
}

static void ParticleBrowser_Selection_MouseDown (ParticleBrowser& particleBrowser, guint32 flags, int pointx,
		int pointy)
{
}

static gboolean ParticleBrowser_button_press (GtkWidget* widget, GdkEventButton* event,
		ParticleBrowser* particleBrowser)
{
	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 1) {
			ParticleBrowser_Selection_MouseDown(*particleBrowser, event->state, static_cast<int> (event->x),
					static_cast<int> (event->y));
		}
	}
	return FALSE;
}

static gboolean ParticleBrowser_size_allocate (GtkWidget* widget, GtkAllocation* allocation,
		ParticleBrowser* particleBrowser)
{
	particleBrowser->width = allocation->width;
	particleBrowser->height = allocation->height;
	ParticleBrowser_queueDraw(*particleBrowser);
	return FALSE;
}

static gboolean ParticleBrowser_expose (GtkWidget* widget, GdkEventExpose* event, ParticleBrowser* particleBrowser)
{
	if (glwidget_make_current(particleBrowser->m_gl_widget) != FALSE) {
		Particle_Draw(*particleBrowser);
		glwidget_swap_buffers(particleBrowser->m_gl_widget);
	}
	return FALSE;
}

static void ParticleBrowser_constructTreeStore (void)
{
	GtkTreeStore* store = gtk_tree_store_new(1, G_TYPE_STRING);
	GtkTreeIter iter;

	for (ParticleDefinitionMap::const_iterator i = g_particleDefinitions.begin(); i != g_particleDefinitions.end(); ++i) {
		gtk_tree_store_append(store, &iter, (GtkTreeIter*) 0);
		gtk_tree_store_set(store, &iter, 0, (*i).first.c_str(), -1);
	}

	GtkTreeModel* model = GTK_TREE_MODEL(store);

	gtk_tree_view_set_model(GTK_TREE_VIEW(g_ParticleBrowser.m_treeViewTree), model);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(g_ParticleBrowser.m_treeViewTree));

	g_object_unref(G_OBJECT(store));
}

static const char *GetModelTypeForParticle (ParticleDefinition *particle)
{
	StringOutputStream buf(128);

	buf << "models/" << particle->m_modelName << ".md2";

	const char *model = GlobalFileSystem().findFile(buf.c_str());
	if (model)
		return "md2";
	return (const char *) 0;
}

static void ParticleBrowser_loadModel (ParticleDefinition *particle)
{
	if (particle->m_modelName) {
		// TODO: Use ModelPreview
		const char *modelType = GetModelTypeForParticle(particle);
		if (!modelType)
			return;
		ModelLoader *loader = ModelLoader_forType(modelType);
		if (!loader)
			return;

		ScopeDisableScreenUpdates disableScreenUpdates(path_get_filename_start(particle->m_modelName),
				_("Loading Model"));

		StringOutputStream buf(128);
		buf << "models/" << particle->m_modelName << "." << modelType;
		AutoPtr<ArchiveFile> file(GlobalFileSystem().openFile(buf.c_str()));
		if (file) {
			particle->loadModel(loader, *file);
		} else {
			g_warning("Model load failed: '%s'\n", buf.c_str());
		}
	}
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

		ParticleDefinitionMap::iterator i = g_particleDefinitions.find(particleName);
		if (i != g_particleDefinitions.end()) {
			ParticleDefinition particle = (*i).second;

			if (g_currentSelectedParticle)
				delete g_currentSelectedParticle;
			g_currentSelectedParticle = new ParticleDefinition(particle);
			// capture the particle texture
			g_currentSelectedParticle->loadTextureForCopy();
			ParticleBrowser_loadModel(g_currentSelectedParticle);

			ParticleBrowser_queueDraw(g_ParticleBrowser);
		}
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
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(g_ParticleBrowser.m_treeViewTree), -1, "", renderer,
			"text", 0, (char const*) 0);

	ParticleBrowser_constructTreeStore();
}

GtkWidget* ParticleBrowser_constructNotebookTab ()
{
	GtkWidget* table = gtk_table_new(3, 3, FALSE);
	GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 1, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(vbox);

	{ // Particle TreeView
		g_ParticleBrowser.m_scr_win_tree = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(GTK_CONTAINER(g_ParticleBrowser.m_scr_win_tree), 0);

		// vertical only scrolling for treeview
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g_ParticleBrowser.m_scr_win_tree), GTK_POLICY_NEVER,
				GTK_POLICY_ALWAYS);

		gtk_widget_show(g_ParticleBrowser.m_scr_win_tree);

		ParticleBrowser_createTreeViewTree();

		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(g_ParticleBrowser.m_scr_win_tree), GTK_WIDGET(
				g_ParticleBrowser.m_treeViewTree));
		gtk_widget_show(GTK_WIDGET(g_ParticleBrowser.m_treeViewTree));
	}
	{ // gl_widget
		g_ParticleBrowser.m_gl_widget = glwidget_new(TRUE);
		gtk_widget_ref(g_ParticleBrowser.m_gl_widget);

		gtk_widget_set_events(g_ParticleBrowser.m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
				| GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
		GTK_WIDGET_SET_FLAGS(g_ParticleBrowser.m_gl_widget, GTK_CAN_FOCUS);

		gtk_table_attach_defaults(GTK_TABLE(table), g_ParticleBrowser.m_gl_widget, 1, 2, 1, 2);
		gtk_widget_show(g_ParticleBrowser.m_gl_widget);

		g_ParticleBrowser.m_sizeHandler = g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "size_allocate",
				G_CALLBACK(ParticleBrowser_size_allocate), &g_ParticleBrowser);
		g_ParticleBrowser.m_exposeHandler = g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "expose_event",
				G_CALLBACK(ParticleBrowser_expose), &g_ParticleBrowser);

		g_signal_connect(G_OBJECT(g_ParticleBrowser.m_gl_widget), "button_press_event", G_CALLBACK(
						ParticleBrowser_button_press), &g_ParticleBrowser);
	}

	gtk_box_pack_start(GTK_BOX(vbox), g_ParticleBrowser.m_scr_win_tree, TRUE, TRUE, 0);

	return table;
}

void ParticleBrowser_Construct (void)
{
}

void ParticleBrowser_Destroy (void)
{
	if (g_currentSelectedParticle) {
		delete g_currentSelectedParticle;
		g_currentSelectedParticle = (ParticleDefinition *) 0;
	}
}

