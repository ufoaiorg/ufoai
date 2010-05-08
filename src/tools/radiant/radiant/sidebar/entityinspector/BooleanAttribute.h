#ifndef BOOLEANATTRIBUTE_H_
#define BOOLEANATTRIBUTE_H_

/**
 * @brief BooleanAttribute. A simple on/off keyval is represented by a checkbox.
 */
class BooleanAttribute: public EntityAttribute
{
		std::string m_classname;
		// The key that this BooleanAttribute is operating on.
		std::string m_key;
		// The visible checkbox
		GtkCheckButton* m_check;

		// Callback function to propagate the change to the keyval whenever the
		// checkbox is toggled. This is passed an explicit "this" pointer because
		// the GTK API is based on C not C++.
		static gboolean toggled (GtkWidget *widget, BooleanAttribute* self)
		{
			self->apply();
			return FALSE;
		}
	public:
		// Constructor
		BooleanAttribute (const std::string& classname, const std::string& key) :
			m_classname(classname), m_key(key), m_check(0)
		{
			GtkCheckButton* check = GTK_CHECK_BUTTON(gtk_check_button_new());

			// Show the checkbutton
			gtk_widget_show(GTK_WIDGET(check));

			m_check = check;

			// Connect up the callback function
			guint handler = g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(toggled), this);
			g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(handler));

			// Get the keyval after construction
			update();
		}
		// Return the checkbox as a GtkWidget
		GtkWidget* getWidget () const
		{
			return GTK_WIDGET(m_check);
		}

		// Propagate GUI changes to the underlying keyval
		void apply (void)
		{
			std::string value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_check)) ? "1" : "0";
			// Set 1 for checkbox ticked, 0 otherwise
			entitySetValue(m_classname, m_key, value);
		}

		// Retrieve keyval and update GtkWidget accordingly
		void update (void)
		{
			const std::string value = entityGetValueForKey(m_key);
			// Set checkbox to enabled for a keyval other than 0
			if (value.length() > 0) {
				toggle_button_set_active_no_signal(GTK_TOGGLE_BUTTON(m_check), string::toInt(value) != 0);
			} else {
				// No keyval found, set self to inactive
				toggle_button_set_active_no_signal(GTK_TOGGLE_BUTTON(m_check), false);
			}
		}
};

#endif /* BOOLEANATTRIBUTE_H_ */
