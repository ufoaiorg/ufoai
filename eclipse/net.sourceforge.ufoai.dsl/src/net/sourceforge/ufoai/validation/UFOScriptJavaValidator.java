package net.sourceforge.ufoai.validation;

import net.sourceforge.ufoai.ufoScript.TranslatableString;
import net.sourceforge.ufoai.ufoScript.UINodeImage;
import net.sourceforge.ufoai.ufoScript.UINodeWindow;
import net.sourceforge.ufoai.ufoScript.UfoScriptPackage;

import org.eclipse.xtext.validation.Check;

public class UFOScriptJavaValidator extends AbstractUFOScriptJavaValidator {
	public static final String INVALID_NAME = "INVALID_NAME";

	@Check
	public void checkWindowName(UINodeWindow window) {
		String name = window.getName();
		if (name == null || name.isEmpty())
			error("Window name must not be empty", null);
		// else if (!name.equals(name.toLowerCase()))
		// warning("Window name should be lowercase",
		// UfoScriptPackage.UI_NODE_WINDOW__NAME, INVALID_NAME, name);
	}

	@Check
	public void checkTranslatable(TranslatableString translatable) {
		final String value = translatable.getValue();
		if (!value.startsWith("_")) {
			warning("String should be prefixed by'_' to became translatable.",
				UfoScriptPackage.Literals.TRANSLATABLE_STRING__VALUE);
		}
	}

	@Check
	public void checkImage(UINodeImage image) {
	}
}
