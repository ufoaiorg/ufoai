#ifndef ANGLEATTRIBUTE_H_
#define ANGLEATTRIBUTE_H_

class AngleAttribute: public EntityAttribute
{
		std::string m_classname;
		std::string m_key;
		GtkEntry* m_entry;
		NonModalEntry m_nonModal;
	public:
		AngleAttribute (const std::string& classname, const std::string& key) :
			m_classname(classname), m_key(key), m_entry(0), m_nonModal(ApplyCaller(*this), UpdateCaller(*this))
		{
			GtkEntry* entry = numeric_entry_new();
			m_entry = entry;
			m_nonModal.connect(m_entry);
		}

		GtkWidget* getWidget () const
		{
			return GTK_WIDGET(m_entry);
		}
		void apply (void)
		{
			StringOutputStream angle(32);
			angle << angle_normalised(entry_get_float(m_entry));
			entitySetValue(m_classname, m_key, angle.c_str());
		}
		typedef MemberCaller<AngleAttribute, &AngleAttribute::apply> ApplyCaller;

		void update (void)
		{
			const std::string value = entityGetValueForKey(m_key);
			if (value.length() > 0) {
				StringOutputStream angle(32);
				angle << angle_normalised(string::toFloat(value));
				gtk_entry_set_text(m_entry, angle.c_str());
			} else {
				gtk_entry_set_text(m_entry, "0");
			}
		}
		typedef MemberCaller<AngleAttribute, &AngleAttribute::update> UpdateCaller;
};

#endif /* ANGLEATTRIBUTE_H_ */
