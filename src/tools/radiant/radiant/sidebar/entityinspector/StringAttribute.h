#ifndef STRINGATTRIBUTE_H_
#define STRINGATTRIBUTE_H_

#include "gtkutil/widget.h"
#include "gtkutil/IConv.h"

/**
 * brief The StringAttribute is used for editing simple strink keyvals
 */
class StringAttribute: public EntityAttribute
{
		std::string m_classname;
		// Name of the keyval we are wrapping
		std::string m_key;
		GtkEntry* m_entry;
		NonModalEntry m_nonModal;
	public:
		// Constructor
		StringAttribute (const std::string& classname, const std::string& key) :
			m_classname(classname), m_key(key), m_entry(0), m_nonModal(ApplyCaller(*this), UpdateCaller(*this))
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			widget_set_size(GTK_WIDGET(entry), 50, 0);

			m_entry = entry;
			m_nonModal.connect(m_entry);
		}
		GtkWidget* getWidget () const
		{
			return GTK_WIDGET(m_entry);
		}
		GtkEntry* getEntry () const
		{
			return m_entry;
		}

		void apply (void)
		{
			std::string value = gtkutil::IConv::localeFromUTF8(gtk_entry_get_text(getEntry()));
			entitySetValue(m_classname, m_key, value);
		}
		typedef MemberCaller<StringAttribute, &StringAttribute::apply> ApplyCaller;

		void update (void)
		{
			const std::string& value = entityGetValueForKey(m_key);
			gtk_entry_set_text(m_entry, value.c_str());
		}
		typedef MemberCaller<StringAttribute, &StringAttribute::update> UpdateCaller;
};

#endif /* STRINGATTRIBUTE_H_ */
