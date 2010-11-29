#include "TexturePreviewCombo.h"

#include "gtkutil/glwidget.h"
#include "gtkutil/GLWidgetSentry.h"
#include "gtkutil/ScrolledFrame.h"

#include "texturelib.h"
#include "AutoPtr.h"
#include "radiant_i18n.h"
#include "igl.h"
#include "ishadersystem.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkscrolledwindow.h>

namespace ui
{
	// Constructor. Create GTK widgets.
	TexturePreviewCombo::TexturePreviewCombo () :
		_widget(gtk_hbox_new(FALSE, 0)), _glWidget(false), _texName(""), _infoStore(gtk_list_store_new(2,
				G_TYPE_STRING, G_TYPE_STRING))
	{
		// Cast the GLWidget object to GtkWidget for further use
		GtkWidget* glWidget = _glWidget;

		// Set up the GL preview widget
		gtk_widget_set_size_request(glWidget, 128, 128);
		g_signal_connect(G_OBJECT(glWidget), "expose-event", G_CALLBACK(_onExpose), this);
		GtkWidget* glFrame = gtk_frame_new(NULL);
		gtk_container_add(GTK_CONTAINER(glFrame), glWidget);
		gtk_box_pack_start(GTK_BOX(_widget), glFrame, FALSE, FALSE, 0);

		// Set up the info table
		GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_infoStore));
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

		GtkCellRenderer* rend;
		GtkTreeViewColumn* col;

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Attribute"), rend, "text", 0, NULL);
		g_object_set(G_OBJECT(rend), "weight", 700, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Value"), rend, "text", 1, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		GtkWidget *scroll = gtkutil::ScrolledFrame(view);

		gtk_box_pack_start(GTK_BOX(_widget), scroll, TRUE, TRUE, 0);
	}

	// Update the selected texture

	void TexturePreviewCombo::setTexture (const std::string& tex)
	{
		_texName = tex;
		refreshInfoTable();
		GtkWidget* glWidget = _glWidget;
		gtk_widget_queue_draw(glWidget);
	}

	// Refresh the info table

	void TexturePreviewCombo::refreshInfoTable ()
	{
		// Prepare the list
		gtk_list_store_clear(_infoStore);
		GtkTreeIter iter;

		// Texture name
		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Shader"), 1, _texName.c_str(), -1);
	}

	// GTK CALLBACKS
	void TexturePreviewCombo::_onExpose (GtkWidget* widget, GdkEventExpose* ev, TexturePreviewCombo* self)
	{
		// Grab the GLWidget with sentry
		gtkutil::GLWidgetSentry sentry(widget);

		// Initialise viewport
		glClearColor(0.3, 0.3, 0.3, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable( GL_DEPTH_TEST);
		glEnable( GL_TEXTURE_2D);

		// If no texture is loaded, leave window blank
		if (self->_texName.empty())
			return;

		glMatrixMode( GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, 1, 0, 1, -1, 1);

		// Get a reference to the selected shader
		IShader* shader = GlobalShaderSystem().getShaderForName(self->_texName);

		// Bind the texture from the shader
		GLTexture* tex = shader->getTexture();
		glBindTexture(GL_TEXTURE_2D, tex->texture_number);

		// Calculate the correct aspect ratio for preview
		float aspect = float(tex->width) / float(tex->height);
		float hfWidth, hfHeight;
		if (aspect > 1.0) {
			hfWidth = 0.5;
			hfHeight = 0.5 / aspect;
		} else {
			hfHeight = 0.5;
			hfWidth = 0.5 * aspect;
		}

		// Draw a polygon
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glColor3f(1, 1, 1);
		glBegin( GL_QUADS);
		glTexCoord2i(0, 1);
		glVertex2f(0.5 - hfWidth, 0.5 - hfHeight);
		glTexCoord2i(1, 1);
		glVertex2f(0.5 + hfWidth, 0.5 - hfHeight);
		glTexCoord2i(1, 0);
		glVertex2f(0.5 + hfWidth, 0.5 + hfHeight);
		glTexCoord2i(0, 0);
		glVertex2f(0.5 - hfWidth, 0.5 + hfHeight);
		glEnd();

		shader->DecRef();
	}
}
