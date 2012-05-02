package net.sourceforge.ufoai.validation;

import org.eclipse.xtext.validation.Check;

import net.sourceforge.ufoai.ufoScript.UINodeImage;
import net.sourceforge.ufoai.ufoScript.UINodeWindow;
import net.sourceforge.ufoai.ufoScript.UfoScriptPackage;

public class UFOScriptJavaValidator extends AbstractUFOScriptJavaValidator {
	@Check
	public void checkWindowName(UINodeWindow window) {
		String name = window.getName();
		if (name == null || name.isEmpty())
			error("Window name must not be empty",
					UfoScriptPackage.Literals.UI_NODE_WINDOW__NAME);
		else if (!name.equals(name.toLowerCase()))
			warning("Window name should be lowercase",
					UfoScriptPackage.Literals.UI_NODE_WINDOW__NAME);
	}

	@Check
	public void checkImage(UINodeImage image) {
	}
}
