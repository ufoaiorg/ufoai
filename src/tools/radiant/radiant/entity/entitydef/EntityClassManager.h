#include "ieclass.h"

#include "string/string.h"
#include "moduleobserver.h"
#include "moduleobservers.h"
#include "eclasslib.h"

#include <map>

class EntityClassManager: public IEntityClassManager, public ModuleObserver
{
	private:

		class RadiantEclassCollector: public EntityClassCollector
		{
			private:
				EntityClassManager* _mgr;
			public:

				RadiantEclassCollector (EntityClassManager *self) :
					_mgr(self)
				{
				}

				void insert (EntityClass* eclass)
				{
					_mgr->InsertAlphabetized(eclass);
				}
				void insert (const std::string& name, const ListAttributeType& list)
				{
					_mgr->g_listTypes.insert(ListAttributeTypes::value_type(name, list));
				}
		};

		RadiantEclassCollector g_collector;

		typedef std::map<std::string, EntityClass*, RawStringLessNoCase> EntityClasses;
		EntityClasses g_entityClasses;
		EntityClass *eclass_bad;
		typedef std::map<std::string, ListAttributeType> ListAttributeTypes;
		ListAttributeTypes g_listTypes;

		std::size_t m_unrealised;
		ModuleObservers m_observers;

		void Construct ();
		void Clear ();
		void CleanEntityList (EntityClasses& entityClasses);
		EntityClass* InsertSortedList (EntityClasses& entityClasses, EntityClass *entityClass);
		EntityClass* InsertAlphabetized (EntityClass *e);

	public:

		EntityClassManager ();
		~EntityClassManager ();

		EntityClass* findOrInsert (const std::string& name, bool has_brushes);
		const ListAttributeType* findListType (const std::string& name);
		void forEach (EntityClassVisitor& visitor);
		void attach (ModuleObserver& observer);
		void detach (ModuleObserver& observer);
		void realise ();
		void unrealise ();
};

EntityClassScanner* GlobalEntityClassScanner ();
