package net.sourceforge.ufoai.ui.hover;

import net.sourceforge.ufoai.ufoScript.NamedBlock;

import org.eclipse.emf.ecore.EObject;
import org.eclipse.xtext.ui.editor.hover.html.DefaultEObjectHoverProvider;

public class HoverProvider extends DefaultEObjectHoverProvider {
	@Override
	protected String getDocumentation(EObject eObject) {
		if (eObject instanceof NamedBlock) {
			NamedBlock block = (NamedBlock) eObject;
			// TODO: Add documentation
			return block.getName();
		}
		return null;
	}
}
