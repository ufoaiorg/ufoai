#ifndef SOUNDATTRIBUTE_H_
#define SOUNDATTRIBUTE_H_

#include "gtkutil/IConv.h"

class SoundAttribute: public EntityAttribute
{
		std::string m_classname;
		std::string m_key;
		BrowsedPathEntry m_entry;
		NonModalEntry m_nonModal;
	public:
		SoundAttribute (const std::string& classname, const std::string& key) :
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
			std::string value = gtkutil::IConv::localeFromUTF8(gtk_entry_get_text(GTK_ENTRY(m_entry.m_entry.m_entry)));
			entitySetValue(m_classname, m_key, value);
		}
		typedef MemberCaller<SoundAttribute, &SoundAttribute::apply> ApplyCaller;
		void update (void)
		{
			const std::string value = entityGetValueForKey(m_key);
			gtk_entry_set_text(GTK_ENTRY(m_entry.m_entry.m_entry), value.c_str());
		}
		typedef MemberCaller<SoundAttribute, &SoundAttribute::update> UpdateCaller;
		void browse (const BrowsedPathEntry::SetPathCallback& setPath)
		{
			// Display the Sound Chooser to get a sound from the user
			ui::SoundChooser sChooser;
			std::string sound = sChooser.chooseSound();
			if (sound.length() > 0) {
				setPath(sound);
				apply();
			}
		}
		typedef MemberCaller1<SoundAttribute, const BrowsedPathEntry::SetPathCallback&, &SoundAttribute::browse>
				BrowseCaller;
};

#endif /* SOUNDATTRIBUTE_H_ */
