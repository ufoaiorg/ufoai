#include "MapCompiler.h"
#include "MapCompileException.h"

#include "iradiant.h"
#include "ifilesystem.h"
#include "radiant_i18n.h"
#include "os/path.h"
#include "string/string.h"
#include "gtkutil/dialog.h"

#include "map.h"
#include "../exec.h"
#include "../sidebar/JobInfo.h"

namespace {
const std::string RKEY_ERROR_CHECK_PARAMETERS = "user/ui/compiler/errorcheckparameters";
const std::string RKEY_ERROR_FIX_PARAMETERS = "user/ui/compiler/errorfixparameters";
const std::string RKEY_COMPILER_BINARY = "user/ui/compiler/binary";
const std::string RKEY_COMPILE_PARAMETERS = "user/ui/compiler/parameters";
const std::string RKEY_MATERIAL_PARAMETERS = "user/ui/compiler/materialparameters";
}

MapCompiler::MapCompiler ()
{
	GlobalRegistry().addKeyObserver(this, RKEY_ERROR_CHECK_PARAMETERS);
	GlobalRegistry().addKeyObserver(this, RKEY_ERROR_FIX_PARAMETERS);
	GlobalRegistry().addKeyObserver(this, RKEY_COMPILER_BINARY);
	GlobalRegistry().addKeyObserver(this, RKEY_COMPILE_PARAMETERS);
	GlobalRegistry().addKeyObserver(this, RKEY_MATERIAL_PARAMETERS);

	// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);

	keyChanged("", "");
}

MapCompiler::~MapCompiler ()
{
}

int MapCompiler::run(const std::string& mapName, const std::string& parameters, ICompilerObserver& observer) {
	if (_compilerBinary.empty())
		throw MapCompileException(_("No compiler binary set"));

	std::string relativeMapName = GlobalFileSystem().getRelative(mapName);

	const std::string workingDir = os::stripFilename(_compilerBinary);
	char* output = NULL;

	int exitCode = exec_run_cmd(_compilerBinary + " " + parameters + " " + relativeMapName, &output, workingDir);
	if (exitCode == -1)
		return exitCode;

	std::vector<std::string> lines;
	string::splitBy(output, lines, "\n");
	for (std::vector<std::string>::const_iterator i = lines.begin(); i != lines.end(); ++i) {
		observer.notify(*i);
	}

	free(output);

	return exitCode;
}

void MapCompiler::performCheck (const std::string& mapName, ICompilerObserver& observer)
{
	run(mapName, _errorCheckParameters, observer);
}

void MapCompiler::fixErrors (const std::string& mapName, ICompilerObserver& observer)
{
	int exitCode = run(mapName, _errorFixParameters, observer);

	if (exitCode == 0) {
		// reload after fix
		GlobalMap().reload();
	}
}

inline void compileReadProgress (void *ex, void *buffer)
{
	static const gdouble compilerSteps = 7.0;
	static const gdouble substeps = 10.0; /* 0..9 in the output */
	static const gdouble stepWidth = 1.0 / compilerSteps / substeps;

	static char const* const steps[] =
			{ "LEVEL:", "UNITCHECK:", "CONNCHECK:", "FACELIGHTS:", "FINALLIGHT:", (const char *) 0 };

	ExecCmd *job = (ExecCmd *) ex;
	Exec *exec = (Exec *) job->exec;

	gchar *buf = (gchar*) buffer;

	if (strstr(buf, "(time:")) {
		job->parse_progress = FALSE;
	} else {
		const char *const *step = steps;
		while (*step) {
			if (buf != 0 && !string_equal(*step, buf)) {
				job->parse_progress = TRUE;
				break;
			}
			step++;
		}
	}

	if (job->parse_progress) {
		const char *dots = strstr(buf, "...");
		if (dots) {
			const char progress = *(dots - 1);
			if (progress >= '0' && progress <= '9') {
				exec->fraction += stepWidth;
				sidebar::JobInfo::Instance().update();
			}
		}
	}
}

void MapCompiler::compileMap (const std::string& mapName, ICompilerObserver& observer)
{
	run(mapName, _compileParameters, observer);
}

void MapCompiler::generateMaterial (const std::string& mapName, ICompilerObserver& observer)
{
	run(mapName, _materialParameters, observer);
}

// RegistryKeyObserver implementation, gets called upon key change
void MapCompiler::keyChanged (const std::string& changedKey, const std::string& newValue)
{
	_errorCheckParameters = GlobalRegistry().get(RKEY_ERROR_CHECK_PARAMETERS);
	_errorFixParameters = GlobalRegistry().get(RKEY_ERROR_FIX_PARAMETERS);
	_compilerBinary = GlobalRegistry().get(RKEY_COMPILER_BINARY);
	_compileParameters = GlobalRegistry().get(RKEY_COMPILE_PARAMETERS);
	_materialParameters = GlobalRegistry().get(RKEY_MATERIAL_PARAMETERS);
}

// PreferenceConstructor implementation, add the preference settings
void MapCompiler::constructPreferencePage (PreferenceGroup& group)
{
	PreferencesPage* page(group.createPage(_("Map Compiler"), _("Map Compiler Settings")));

	page->appendPathEntry(_("Compiler binary"), RKEY_COMPILER_BINARY, false);
	page->appendEntry(_("Compile parameters"), RKEY_COMPILE_PARAMETERS);

	page->appendEntry(_("Error checking parameters"), RKEY_ERROR_CHECK_PARAMETERS);
	page->appendEntry(_("Error fixing parameters"), RKEY_ERROR_FIX_PARAMETERS);
	page->appendEntry(_("Material generation parameters"), RKEY_MATERIAL_PARAMETERS);
}

/* Overlay dependencies class.
 */
class MapCompilerDependencies :
	public GlobalRadiantModuleRef,
	public GlobalRegistryModuleRef,
	public GlobalPreferenceSystemModuleRef
{};

/* Required code to register the module with the ModuleServer.
 */
#include "modulesystem/singletonmodule.h"

typedef SingletonModule<MapCompiler, MapCompilerDependencies> MapCompilerModule;

typedef Static<MapCompilerModule> StaticMapCompilerSystemModule;
StaticRegisterModule staticRegisterMapCompilerSystem(StaticMapCompilerSystemModule::instance());
