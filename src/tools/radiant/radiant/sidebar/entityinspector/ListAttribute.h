#ifndef LISTATTRIBUTE_H_
#define LISTATTRIBUTE_H_

class NonModalComboBox
{
		Callback m_changed;
		guint m_changedHandler;

		static gboolean changed (GtkComboBox *widget, NonModalComboBox* self)
		{
			self->m_changed();
			return FALSE;
		}

	public:
		NonModalComboBox (const Callback& changed) :
			m_changed(changed), m_changedHandler(0)
		{
		}
		void connect (GtkComboBox* combo)
		{
			m_changedHandler = g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(changed), this);
		}
		void setActive (GtkComboBox* combo, int value)
		{
			g_signal_handler_disconnect(G_OBJECT(combo), m_changedHandler);
			gtk_combo_box_set_active(combo, value);
			connect(combo);
		}
};

class ListAttribute: public EntityAttribute
{
		std::string m_classname;
		std::string m_key;
		GtkComboBox* m_combo;
		NonModalComboBox m_nonModal;
		const ListAttributeType& m_type;
	public:
		ListAttribute (const std::string& classname, const std::string& key, const ListAttributeType& type) :
			m_classname(classname), m_key(key), m_combo(0), m_nonModal(ApplyCaller(*this)), m_type(type)
		{
			GtkComboBox* combo = GTK_COMBO_BOX(gtk_combo_box_new_text());

			for (ListAttributeType::const_iterator i = type.begin(); i != type.end(); ++i) {
				gtk_combo_box_append_text(GTK_COMBO_BOX(combo), i->first.c_str());
			}

			gtk_widget_show(GTK_WIDGET(combo));
			m_nonModal.connect(combo);

			m_combo = combo;
		}

		GtkWidget* getWidget () const
		{
			return GTK_WIDGET(m_combo);
		}
		void apply (void)
		{
			entitySetValue(m_classname, m_key,
					m_type[gtk_combo_box_get_active(m_combo)].second);
		}
		typedef MemberCaller<ListAttribute, &ListAttribute::apply> ApplyCaller;

		void update (void)
		{
			const std::string& value = entityGetValueForKey(m_key);
			ListAttributeType::const_iterator i = m_type.findValue(value);
			if (i != m_type.end()) {
				m_nonModal.setActive(m_combo, static_cast<int> (std::distance(m_type.begin(), i)));
			} else {
				m_nonModal.setActive(m_combo, 0);
			}
		}
		//typedef MemberCaller<ListAttribute, &ListAttribute::update> UpdateCaller;
};

#endif /* LISTATTRIBUTE_H_ */
