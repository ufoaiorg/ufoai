#ifndef VECTOR3ATTRIBUTE_H_
#define VECTOR3ATTRIBUTE_H_

class Vector3Entry
{
	public:
		GtkEntry* m_x;
		GtkEntry* m_y;
		GtkEntry* m_z;
		Vector3Entry () :
			m_x(0), m_y(0), m_z(0)
		{
		}
};

class Vector3Attribute: public EntityAttribute
{
		std::string m_classname;
		std::string m_key;
		Vector3Entry m_vector3;
		NonModalEntry m_nonModal;
		GtkBox* m_hbox;
	public:
		Vector3Attribute (const std::string& classname, const std::string& key) :
			m_classname(classname), m_key(key), m_nonModal(ApplyCaller(*this), UpdateCaller(*this))
		{
			m_hbox = GTK_BOX(gtk_hbox_new(TRUE, 4));
			gtk_widget_show(GTK_WIDGET(m_hbox));
			{
				GtkEntry* entry = numeric_entry_new();
				gtk_box_pack_start(m_hbox, GTK_WIDGET(entry), TRUE, TRUE, 0);
				m_vector3.m_x = entry;
				m_nonModal.connect(m_vector3.m_x);
			}
			{
				GtkEntry* entry = numeric_entry_new();
				gtk_box_pack_start(m_hbox, GTK_WIDGET(entry), TRUE, TRUE, 0);
				m_vector3.m_y = entry;
				m_nonModal.connect(m_vector3.m_y);
			}
			{
				GtkEntry* entry = numeric_entry_new();
				gtk_box_pack_start(m_hbox, GTK_WIDGET(entry), TRUE, TRUE, 0);
				m_vector3.m_z = entry;
				m_nonModal.connect(m_vector3.m_z);
			}
		}

		GtkWidget* getWidget () const
		{
			return GTK_WIDGET(m_hbox);
		}
		void apply (void)
		{
			StringOutputStream vector3(64);
			vector3 << entry_get_float(m_vector3.m_x) << " " << entry_get_float(m_vector3.m_y) << " "
					<< entry_get_float(m_vector3.m_z);
			entitySetValue(m_classname, m_key, vector3.c_str());
		}
		typedef MemberCaller<Vector3Attribute, &Vector3Attribute::apply> ApplyCaller;

		void update (void)
		{
			StringOutputStream buffer(32);
			const std::string& value = entityGetValueForKey(m_key);
			if (value.length() > 0) {
				DoubleVector3 x_y_z;
				if (!string_parse_vector3(value, x_y_z)) {
					x_y_z = DoubleVector3(0, 0, 0);
				}

				buffer << x_y_z.x();
				gtk_entry_set_text(m_vector3.m_x, buffer.c_str());
				buffer.clear();

				buffer << x_y_z.y();
				gtk_entry_set_text(m_vector3.m_y, buffer.c_str());
				buffer.clear();

				buffer << x_y_z.z();
				gtk_entry_set_text(m_vector3.m_z, buffer.c_str());
				buffer.clear();
			} else {
				gtk_entry_set_text(m_vector3.m_x, "0");
				gtk_entry_set_text(m_vector3.m_y, "0");
				gtk_entry_set_text(m_vector3.m_z, "0");
			}
		}
		typedef MemberCaller<Vector3Attribute, &Vector3Attribute::update> UpdateCaller;
};

#endif /* VECTOR3ATTRIBUTE_H_ */
