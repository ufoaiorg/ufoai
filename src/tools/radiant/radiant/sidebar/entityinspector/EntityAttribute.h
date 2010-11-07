#ifndef ENTITYATTRIBUTE_H_
#define ENTITYATTRIBUTE_H_

#include "../../entity.h"

namespace
{
	typedef std::list<std::string> Values;
	typedef std::map<std::string, Values> KeyValues;
	typedef std::map<std::string, KeyValues> ClassKeyValues;
	ClassKeyValues g_selectedKeyValues; /**< all selected entities keyvalues */

	const EntityClass* g_current_attributes = 0;
	std::string g_currentSelectedKey;
}

/**
 * @brief The EntityAttribute interface is used as a base class for all of the
 * individual GTK "renderers" for each key type. This allows, for example, a
 * boolean keyval to be controlled through a checkbox while a text keyval is
 * given a text entry field.
 *
 * Note that the EntityAttribute selection is based on the key TYPE not the key
 * itself, so it is not (currently) possible to use a different EntityAttribute
 * for each of two text keyvals.
 */
class EntityAttribute
{
	public:
		virtual ~EntityAttribute (void)
		{
		}
		// An EntityAttribute must return a GtkWidget that is used to interact with
		// its values.
		virtual GtkWidget* getWidget () const = 0;
		// Update the GTK widgets based on the current value of the key.
		virtual void update () = 0;

		void entitySetValue (const std::string& classname, const std::string& key,
				const std::string& value)
		{
			std::string command = "entitySetKeyValue -classname \"" + classname + "\" -key \"" + key + "\" -value \"" + value
					+ "\"";
			UndoableCommand undo(command);
			Scene_EntitySetKeyValue_Selected(classname, key, value);
		}

		/**
		 * @brief Helper method returns the first value in value list for given key for currently selected entity class or an empty string.
		 * @param key key to retrieve value for
		 * @return first value in value list or empty string
		 */
		const std::string entityGetValueForKey (const std::string& key)
		{
			ASSERT_MESSAGE(g_current_attributes != 0, "g_current_attributes is zero");
			ClassKeyValues::iterator it = g_selectedKeyValues.find(g_current_attributes->m_name);
			if (it != g_selectedKeyValues.end()) {
				KeyValues &possibleValues = (*it).second;
				KeyValues::const_iterator i = possibleValues.find(key);
				if (i != possibleValues.end()) {
					const Values &values = (*i).second;
					ASSERT_MESSAGE(!values.empty(), "Values don't exist");
					return *values.begin();
				}
			}
			return "";
		}

		inline double angle_normalised (double angle)
		{
			return float_mod(angle, 360.0);
		}

		GtkEntry* numeric_entry_new (void)
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			widget_set_size(GTK_WIDGET(entry), 10, 0);
			return entry;
		}
};

#endif /* ENTITYATTRIBUTE_H_ */
