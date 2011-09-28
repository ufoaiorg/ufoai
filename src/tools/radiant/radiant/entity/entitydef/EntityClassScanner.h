#include "ieclass.h"

#include "../../../../../shared/entitiesdef.h"

class EntityClassScannerUFO: public EntityClassScanner
{
	private:

		EntityClass* initFromDefinition (const entityDef_t *definition);

		void parseFlags (EntityClass *e, const char **text);
		void parseAttribute (EntityClass *e, const entityKeyDef_t *keydef);

		const std::string getFilename () const;

	public:

		void scanFile (EntityClassCollector& collector);
};
