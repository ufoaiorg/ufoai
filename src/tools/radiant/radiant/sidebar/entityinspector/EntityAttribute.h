#ifndef ENTITYATTRIBUTE_H_
#define ENTITYATTRIBUTE_H_

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
};

#endif /* ENTITYATTRIBUTE_H_ */
