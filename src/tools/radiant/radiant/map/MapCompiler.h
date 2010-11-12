#include "imapcompiler.h"
#include "iregistry.h"
#include "preferencesystem.h"

class MapCompiler: public IMapCompiler, public RegistryKeyObserver, public PreferenceConstructor
{
	public:
		// Radiant Module stuff
		typedef IMapCompiler Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		IMapCompiler* getTable ()
		{
			return this;
		}

	private:

		std::string _errorCheckParameters;
		std::string _errorFixParameters;
		std::string _compilerBinary;
		std::string _compileParameters;
		std::string _materialParameters;

	public:
		MapCompiler ();

		~MapCompiler ();

		void performCheck (const std::string& mapName, ICompilerObserver& observer);

		void fixErrors (const std::string& mapName, ICompilerObserver& observer);

		void compileMap (const std::string& mapName, ICompilerObserver& observer);

		void generateMaterial (const std::string& mapName, ICompilerObserver& observer);

		void keyChanged (const std::string& changedKey, const std::string& newValue);

		void constructPreferencePage (PreferenceGroup& group);

	private:

		int run(const std::string& mapName, const std::string& parameters, ICompilerObserver& observer);
};
