class Tokeniser;

#include <map>
#include <string>

class LicenseParser
{
	private:

		typedef std::map<std::string, bool> LicensesMap;
		LicensesMap _licensesMap;

		void parseLicensesFile (Tokeniser& tokeniser, const std::string& filename);

		void clearLicenses ();

	public:

		bool isValid (const std::string& filename) const;

		void openLicenseFile (const std::string& filename);

		LicenseParser ();

		~LicenseParser ();
};
