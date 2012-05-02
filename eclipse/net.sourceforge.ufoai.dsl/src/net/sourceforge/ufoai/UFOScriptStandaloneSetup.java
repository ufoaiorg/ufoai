package net.sourceforge.ufoai;

/**
 * Initialization support for running Xtext languages without equinox extension
 * registry
 */
public class UFOScriptStandaloneSetup extends UFOScriptStandaloneSetupGenerated {
	public static void doSetup() {
		new UFOScriptStandaloneSetup().createInjectorAndDoEMFRegistration();
	}
}
