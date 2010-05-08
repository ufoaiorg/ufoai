#ifndef DIRECTIONATTRIBUTE_H_
#define DIRECTIONATTRIBUTE_H_

static const char* directionButtons[] = { "up", "down", "z-axis" };

class DirectionAttribute: public EntityAttribute
{
		std::string m_classname;
		std::string m_key;
		GtkEntry* m_entry;
		NonModalEntry m_nonModal;
		RadioHBox m_radio;
		NonModalRadio m_nonModalRadio;
		GtkHBox* m_hbox;
	public:
		DirectionAttribute (const std::string& classname, const std::string& key) :
			m_classname(classname), m_key(key), m_entry(0), m_nonModal(ApplyCaller(*this), UpdateCaller(*this)),
					m_radio(RadioHBox_new(STRING_ARRAY_RANGE(directionButtons))), m_nonModalRadio(ApplyRadioCaller(
							*this))
		{
			GtkEntry* entry = numeric_entry_new();
			m_entry = entry;
			m_nonModal.connect(m_entry);

			m_nonModalRadio.connect(m_radio.m_radio);

			m_hbox = GTK_HBOX(gtk_hbox_new(FALSE, 4));
			gtk_widget_show(GTK_WIDGET(m_hbox));

			gtk_box_pack_start(GTK_BOX(m_hbox), GTK_WIDGET(m_radio.m_hbox), TRUE, TRUE, 0);
			gtk_box_pack_start(GTK_BOX(m_hbox), GTK_WIDGET(m_entry), TRUE, TRUE, 0);
		}

		GtkWidget* getWidget () const
		{
			return GTK_WIDGET(m_hbox);
		}
		void apply (void)
		{
			StringOutputStream angle(32);
			angle << angle_normalised(entry_get_float(m_entry));
			entitySetValue(m_classname, m_key, angle.c_str());
		}
		typedef MemberCaller<DirectionAttribute, &DirectionAttribute::apply> ApplyCaller;

		void update (void)
		{
			const std::string& value = entityGetValueForKey(m_key);
			if (value.length() > 0) {
				const float f = string::toFloat(value);
				if (f == -1) {
					gtk_widget_set_sensitive(GTK_WIDGET(m_entry), FALSE);
					radio_button_set_active_no_signal(m_radio.m_radio, 0);
					gtk_entry_set_text(m_entry, "");
				} else if (f == -2) {
					gtk_widget_set_sensitive(GTK_WIDGET(m_entry), FALSE);
					radio_button_set_active_no_signal(m_radio.m_radio, 1);
					gtk_entry_set_text(m_entry, "");
				} else {
					gtk_widget_set_sensitive(GTK_WIDGET(m_entry), TRUE);
					radio_button_set_active_no_signal(m_radio.m_radio, 2);
					StringOutputStream angle(32);
					angle << angle_normalised(f);
					gtk_entry_set_text(m_entry, angle.c_str());
				}
			} else {
				gtk_entry_set_text(m_entry, "0");
			}
		}
		typedef MemberCaller<DirectionAttribute, &DirectionAttribute::update> UpdateCaller;

		void applyRadio (void)
		{
			const int index = radio_button_get_active(m_radio.m_radio);
			if (index == 0) {
				entitySetValue(m_classname, m_key, "-1");
			} else if (index == 1) {
				entitySetValue(m_classname, m_key, "-2");
			} else if (index == 2) {
				apply();
			}
		}
		typedef MemberCaller<DirectionAttribute, &DirectionAttribute::applyRadio> ApplyRadioCaller;
};

#endif /* DIRECTIONATTRIBUTE_H_ */
