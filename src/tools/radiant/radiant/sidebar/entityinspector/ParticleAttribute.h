#ifndef PARTICLEATTRIBUTE_H_
#define PARTICLEATTRIBUTE_H_

#include "../../ui/particles/ParticleSelector.h"

class ParticleAttribute: public EntityAttribute
{
		std::string m_classname;
		std::string m_key;
		BrowsedPathEntry m_entry;
		NonModalEntry m_nonModal;
	public:
		ParticleAttribute (const std::string& classname, const std::string& key) :
			m_classname(classname), m_key(key), m_entry(BrowseCaller(*this)), m_nonModal(ApplyCaller(*this),
					UpdateCaller(*this))
		{
			m_nonModal.connect(m_entry.m_entry.m_entry);
		}

		GtkWidget* getWidget () const
		{
			return GTK_WIDGET(m_entry.m_entry.m_frame);
		}
		void apply (void)
		{
			StringOutputStream value(64);
			value << ConvertUTF8ToLocale(gtk_entry_get_text(GTK_ENTRY(m_entry.m_entry.m_entry)));
			entitySetValue(m_classname, m_key, value.c_str());
		}
		typedef MemberCaller<ParticleAttribute, &ParticleAttribute::apply> ApplyCaller;
		void update (void)
		{
			const std::string value = entityGetValueForKey(m_key);
			gtk_entry_set_text(GTK_ENTRY(m_entry.m_entry.m_entry), value.c_str());
		}
		typedef MemberCaller<ParticleAttribute, &ParticleAttribute::update> UpdateCaller;
		void browse (const BrowsedPathEntry::SetPathCallback& setPath)
		{
			ui::ParticleSelector pSelector;
			std::string filename = pSelector.chooseParticle();
			if (!filename.empty()) {
				setPath(filename);
				apply();
			}
		}
		typedef MemberCaller1<ParticleAttribute, const BrowsedPathEntry::SetPathCallback&, &ParticleAttribute::browse>
				BrowseCaller;
};

#endif /* PARTICLEATTRIBUTE_H_ */
