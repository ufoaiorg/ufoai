#ifndef ANGLESATTRIBUTE_H_
#define ANGLESATTRIBUTE_H_

class AnglesEntry
{
	public:
		GtkEntry* m_roll;
		GtkEntry* m_pitch;
		GtkEntry* m_yaw;
		AnglesEntry () :
			m_roll(0), m_pitch(0), m_yaw(0)
		{
		}
};

class AnglesAttribute: public EntityAttribute
{
		std::string m_classname;
		std::string m_key;
		AnglesEntry m_angles;
		NonModalEntry m_nonModal;
		GtkBox* m_hbox;
	public:
		AnglesAttribute (const std::string& classname, const std::string& key) :
			m_classname(classname), m_key(key), m_nonModal(ApplyCaller(*this), UpdateCaller(*this))
		{
			m_hbox = GTK_BOX(gtk_hbox_new(TRUE, 4));
			gtk_widget_show(GTK_WIDGET(m_hbox));
			{
				GtkEntry* entry = numeric_entry_new();
				gtk_box_pack_start(m_hbox, GTK_WIDGET(entry), TRUE, TRUE, 0);
				m_angles.m_pitch = entry;
				m_nonModal.connect(m_angles.m_pitch);
			}
			{
				GtkEntry* entry = numeric_entry_new();
				gtk_box_pack_start(m_hbox, GTK_WIDGET(entry), TRUE, TRUE, 0);
				m_angles.m_yaw = entry;
				m_nonModal.connect(m_angles.m_yaw);
			}
			{
				GtkEntry* entry = numeric_entry_new();
				gtk_box_pack_start(m_hbox, GTK_WIDGET(entry), TRUE, TRUE, 0);
				m_angles.m_roll = entry;
				m_nonModal.connect(m_angles.m_roll);
			}
		}

		GtkWidget* getWidget () const
		{
			return GTK_WIDGET(m_hbox);
		}
		void apply (void)
		{
			StringOutputStream angles(64);
			angles << angle_normalised(entry_get_float(m_angles.m_pitch)) << " " << angle_normalised(entry_get_float(
					m_angles.m_yaw)) << " " << angle_normalised(entry_get_float(m_angles.m_roll));
			entitySetValue(m_classname, m_key, angles.c_str());
		}
		typedef MemberCaller<AnglesAttribute, &AnglesAttribute::apply> ApplyCaller;

		void update (void)
		{
			StringOutputStream angle(32);
			const std::string& value = entityGetValueForKey(m_key);
			if (value.length() > 0) {
				DoubleVector3 pitch_yaw_roll;
				if (!string_parse_vector3(value.c_str(), pitch_yaw_roll)) {
					pitch_yaw_roll = DoubleVector3(0, 0, 0);
				}

				angle << angle_normalised(pitch_yaw_roll.x());
				gtk_entry_set_text(m_angles.m_pitch, angle.c_str());
				angle.clear();

				angle << angle_normalised(pitch_yaw_roll.y());
				gtk_entry_set_text(m_angles.m_yaw, angle.c_str());
				angle.clear();

				angle << angle_normalised(pitch_yaw_roll.z());
				gtk_entry_set_text(m_angles.m_roll, angle.c_str());
				angle.clear();
			} else {
				gtk_entry_set_text(m_angles.m_pitch, "0");
				gtk_entry_set_text(m_angles.m_yaw, "0");
				gtk_entry_set_text(m_angles.m_roll, "0");
			}
		}
		typedef MemberCaller<AnglesAttribute, &AnglesAttribute::update> UpdateCaller;
};

#endif /* ANGLESATTRIBUTE_H_ */
