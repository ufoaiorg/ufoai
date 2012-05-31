package net.sourceforge.ufoai.validation;

import java.io.File;

import net.sourceforge.ufoai.ufoScript.UFONode;
import net.sourceforge.ufoai.ufoScript.UfoScriptPackage;
import net.sourceforge.ufoai.ufoScript.ValueBoolean;
import net.sourceforge.ufoai.ufoScript.ValueNamedConst;
import net.sourceforge.ufoai.ufoScript.ValueNull;
import net.sourceforge.ufoai.ufoScript.ValueNumber;
import net.sourceforge.ufoai.ufoScript.ValueReference;
import net.sourceforge.ufoai.ufoScript.ValueString;
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

	private String getPath(UFONode node) {
		String path = null;
		while (node != null) {
			if (path == null) {
				path = node.getType();
			} else {
				path = node.getType() + "." + path;
			}
			if (node.eContainer() instanceof UFONode) {
				node = (UFONode) node.eContainer();
			} else {
				node = null;
			}
		}
		return path;
	}

	private UFOType getType(String path) {
		return UFOTypes.getInstance().getPathType(path);
	}

	private void validateReference(UFONode node, String referenceType) {
		if (node.getValue() instanceof ValueNull) {
			// nothing
		} else if (node.getValue() instanceof ValueReference) {
			ValueReference value = (ValueReference) node.getValue();
			if (referenceType != null && !value.getValue().getType().equals(referenceType)) {
				error(referenceType + " id expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
		} else {
			error("Id expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
		}
	}

	private void validate(UFONode node) {
		String path = getPath(node);
		UFOType type = getType(path);

		// No rules
		if (type == null) {
			error("Property name unknown", UfoScriptPackage.Literals.UFO_NODE__TYPE);
			return;
		}

		switch (type.getType()) {
		case BLOCK:
			if (node.getValue() != null || node.getNodes().isEmpty()) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case TRANSLATED_STRING:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else {
				ValueString value = (ValueString) node.getValue();
				if (!value.getValue().startsWith("_")) {
					warning("String should be prefixed by '_' to became translatable.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case ENUM:
		{
			if ((node.getValue() instanceof ValueString)) {
				ValueString value = (ValueString) node.getValue();
				if (!type.contains(value.getValue())) {
					warning("Not a valide value.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			} else if ((node.getValue() instanceof ValueNamedConst)) {
				ValueNamedConst value = (ValueNamedConst) node.getValue();
				if (!type.contains(value.getValue())) {
					warning("Not a valide value.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			} else {
				error("Quoted string or uppercase named const expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		}
		case STRING:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case COLOR:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case DATE:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case ID:
			validateReference(node, null);
			break;
		case TECH_ID:
			validateReference(node, "tech");
			break;
		case BUILDING_ID:
			validateReference(node, "building");
			break;
		case PARTICLE_ID:
			validateReference(node, "particle");
			break;
		case MENU_MODEL_ID:
			validateReference(node, "menu_model");
			break;
		case INT:
			if (!(node.getValue() instanceof ValueNumber)) {
				error("Number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case REAL:
			if (!(node.getValue() instanceof ValueNumber)) {
				error("Number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case BOOLEAN:
			if (!(node.getValue() instanceof ValueBoolean)) {
				error("Number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case PICS_IMAGE:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted image name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString value = (ValueString) node.getValue();
				final String id = value.getValue();
				final File file = UfoResources.getFileFromPics(id);
				if (file == null) {
					warning("Image not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case MODEL:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted model name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString value = (ValueString) node.getValue();
				final String id = value.getValue();
				final File file = UfoResources.getFileFromModels(id);
				if (file == null) {
					warning("Model not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case SOUND:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted sound name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString value = (ValueString) node.getValue();
				final String id = value.getValue();
				final File file = UfoResources.getFileFromSound(id);
				if (file == null) {
					warning("Sound not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case VEC2:
		case VEC3:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted tuple of number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case SHAPE:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted tuple of number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		default:
			System.out.println("Type \"" + type + "\" not yet supported");
		}
	}

	@Check
	public void checkNode(UFONode property) {
		validate(property);
	}

}
