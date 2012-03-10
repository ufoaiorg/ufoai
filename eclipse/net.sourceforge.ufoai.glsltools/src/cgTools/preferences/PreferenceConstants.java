package cgTools.preferences;

/**
 * Constant definitions for plug-in preferences
 */
public class PreferenceConstants {
	public static final String CGPATH = "Cgc bin path";
	public static final String GLSLPATH = "GLSL front end compiler exe";

	public static final String CGPROFILE = "Profile preference";

	public static final String cgProfiles[] = { "arb", "p40", "p30", "p20" };
	public static final String cgProfilesDescriptor[] = { "Open &ARB",
			"OpenGL NV&40 ", "OpenGL NV&30", "OpenGL NV&2X" };
}
