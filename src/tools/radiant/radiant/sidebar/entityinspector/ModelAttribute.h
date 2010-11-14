#ifndef MODELATTRIBUTE_H_
#define MODELATTRIBUTE_H_

#include "../../ui/modelselector/ModelSelector.h"
#include "gtkutil/IConv.h"

class ModelAttribute: public EntityAttribute
{
		std::string m_classname;
		std::string m_key;
		BrowsedPathEntry m_entry;
		NonModalEntry m_nonModal;
	public:
		ModelAttribute (const std::string& classname, const std::string& key) :
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
		typedef MemberCaller<ModelAttribute, &ModelAttribute::apply> ApplyCaller;
		void update (void)
		{
			const std::string value = entityGetValueForKey(m_key);
			gtk_entry_set_text(GTK_ENTRY(m_entry.m_entry.m_entry), value.c_str());
		}
		typedef MemberCaller<ModelAttribute, &ModelAttribute::update> UpdateCaller;
		void browse (const BrowsedPathEntry::SetPathCallback& setPath)
		{
			ui::ModelAndSkin modelAndSkin = ui::ModelSelector::chooseModel();
			if (!modelAndSkin.model.empty()) {
				if (modelAndSkin.skin != -1)
					entitySetValue(m_classname, "skin", string::toString(modelAndSkin.skin));
				setPath(modelAndSkin.model);
				apply();
			}
		}
		typedef MemberCaller1<ModelAttribute, const BrowsedPathEntry::SetPathCallback&, &ModelAttribute::browse>
				BrowseCaller;
};

#endif /* MODELATTRIBUTE_H_ */
