/**
 *
 */
package cgEditor.editors;

import cgEditor.preferences.PreferenceConstants;

/**
 * @author Martinez
 */
public class CgConfiguration extends ShaderFileConfiguration {
	public CgConfiguration() {
		super();
	}

	@Override
	protected ShaderFileScanner getTagScanner() {
		if (scanner == null) {
			scanner = new CgScanner();
			scanner.setShaderRules();
			scanner.setDefaultReturnToken(TokenManager
					.getToken(PreferenceConstants.DEFAULTSTRING));
		}
		return scanner;
	}
}
