package cgEditor.preferences;

import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.RGB;

/**
 * Constant definitions for plug-in preferences
 */
public class PreferenceConstants {
	public static String COMMENTSTRING = "Comments";
	public static String DEFAULTSTRING = "Default";
	public static String LANGUAGESSTRING = "Language";
	public static String FUNCTIONSSTRING = "Functions";
	public static String TYPESSTRING = "Types";
	public static String SEMANTICSSTRING = "Semantics";

	public static String COMMENTCOLOR = "Comment Color";
	public static String COMMENTSTYLE = "Comment Style";

	public static String DEFAULTCOLOR = "Default Color";
	public static String DEFAULTSTYLE = "Default Style";

	public static String TYPECOLOR = "Type Color";
	public static String TYPESTYLE = "Type Style";

	public static String SEMANTICCOLOR = "Semantic Color";
	public static String SEMANTICSTYLE = "Semantic Style";

	public static String FUNCTIONCOLOR = "Function Color";
	public static String FUNCTIONSTYLE = "Function Style";

	public static String LANGUAGECOLOR = "Language Color";
	public static String LANGUAGESTYLE = "Language Style";

	public static String COMMENT[] = { COMMENTSTRING, COMMENTCOLOR,
			COMMENTSTYLE };
	public static String DEFAULT[] = { DEFAULTSTRING, DEFAULTCOLOR,
			DEFAULTSTYLE };
	public static String TYPE[] = { TYPESSTRING, TYPECOLOR, TYPESTYLE };
	public static String SEMANTIC[] = { SEMANTICSSTRING, SEMANTICCOLOR,
			SEMANTICSTYLE };
	public static String FUNCTION[] = { FUNCTIONSSTRING, FUNCTIONCOLOR,
			FUNCTIONSTYLE };
	public static String LANGUAGE[] = { LANGUAGESSTRING, LANGUAGECOLOR,
			LANGUAGESTYLE };

	public static Object DEFAULTCOMMENT[] = { new RGB(0, 128, 0),
			new Integer(SWT.NORMAL) };
	public static Object DEFAULTDEFAULT[] = { new RGB(0, 0, 0),
			new Integer(SWT.NORMAL) };
	public static Object DEFAULTTYPE[] = { new RGB(0, 0, 192),
			new Integer(SWT.ITALIC) };
	public static Object DEFAULTSEMANTIC[] = { new RGB(127, 0, 85),
			new Integer(SWT.BOLD) };
	public static Object DEFAULTFUNCTION[] = { new RGB(128, 128, 0),
			new Integer(SWT.NORMAL) };
	public static Object DEFAULTLANGUAGE[] = { new RGB(128, 0, 0),
			new Integer(SWT.BOLD) };

	public static String PREFERENCES[][] = { COMMENT, DEFAULT, TYPE, SEMANTIC,
			FUNCTION, LANGUAGE };

	public static Object DEFAULTPREFERENCES[][] = { DEFAULTCOMMENT,
			DEFAULTDEFAULT, DEFAULTTYPE, DEFAULTSEMANTIC, DEFAULTFUNCTION,
			DEFAULTLANGUAGE };
}
