package net.sourceforge.ufoai.validation;

import java.io.File;

import net.sourceforge.ufoai.ufoScript.PicsImage;
import net.sourceforge.ufoai.ufoScript.TranslatableString;
import net.sourceforge.ufoai.ufoScript.UINodeWindow;
import net.sourceforge.ufoai.ufoScript.UfoScriptPackage;
import net.sourceforge.ufoai.util.UfoResources;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.Path;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.emf.ecore.resource.Resource;
import org.eclipse.xtext.validation.Check;

public class UFOScriptJavaValidator extends AbstractUFOScriptJavaValidator {
	public static final String INVALID_NAME = "INVALID_NAME";


	/**
	 * Check for "base" dir.
	 * Maybe something better exists, but i dont find it.
	 * @param object An object witch contains information to source file
	 */
	private void checkUfoBase(EObject object) {
		if (UfoResources.isBaseChecked())
			return;
		Resource resource = object.eResource();
		String platformString = resource.getURI().toPlatformString(true);
		IFile file = ResourcesPlugin.getWorkspace().getRoot().getFile(new Path(platformString));
		UfoResources.checkBase(file.getLocation().toOSString());
	}

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
			warning("String should be prefixed by '_' to became translatable.",
				UfoScriptPackage.Literals.TRANSLATABLE_STRING__VALUE);
		}
	}

	@Check
	public void checkImage(PicsImage image) {
		checkUfoBase(image);

		final String id = image.getValue();
		final File file = UfoResources.getFileFromPics(id);
		if (file == null) {
			warning("Image not found.",
				UfoScriptPackage.Literals.PICS_IMAGE__VALUE);
		}
	}

}
