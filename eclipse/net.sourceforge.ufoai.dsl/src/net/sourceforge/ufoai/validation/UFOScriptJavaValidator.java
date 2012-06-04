package net.sourceforge.ufoai.validation;

import java.io.File;

import net.sourceforge.ufoai.ufoScript.UFONode;
import net.sourceforge.ufoai.ufoScript.UfoScriptPackage;
import net.sourceforge.ufoai.ufoScript.ValueBoolean;
import net.sourceforge.ufoai.ufoScript.ValueCode;
import net.sourceforge.ufoai.ufoScript.ValueNamedConst;
import net.sourceforge.ufoai.ufoScript.ValueNumber;
import net.sourceforge.ufoai.ufoScript.ValueReference;
import net.sourceforge.ufoai.ufoScript.ValueString;
import net.sourceforge.ufoai.util.StringUtil;
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

	private Object getType(EObject object) {
		if (object instanceof UFONode) {
			String path = getPath((UFONode) object);
			return getType(path);
		}
		return null;
	}

	private UFOType getType(String path) {
		return UFOTypes.getInstance().getPathType(path);
	}

	private void validateReference(UFONode node, UFOType type) {
		/*if (node.getValue() instanceof ValueNull) {
			// nothing
		} else
			*/
		if (node.getValue() instanceof ValueReference) {
			ValueReference value = (ValueReference) node.getValue();
			if (value.getValue() == null) {
				// xtext should already warn about it
				return;
			}
			if (type.hasValues() && !type.contains(value.getValue().getType())) {
				String expected = StringUtil.join(" or ", type.getValues());
				error(expected + " id expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
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
			// Display error if the parent has no rules
			if (getType(node.eContainer()) == null) {
				return;
			}
			error("Property name unknown", UfoScriptPackage.Literals.UFO_NODE__TYPE);
			return;
		}

		switch (type.getType()) {
		case IDENTIFIED_BLOCK:
			if (node.getValue() != null || node.getNodes() == null) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (node.getName() == null) {
				error("Identified block expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case ANONYMOUS_BLOCK:
			if (node.getValue() != null || node.getNodes() == null) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (node.getName() != null) {
				error("Anonymous block expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case BLOCK:
			if (node.getValue() != null || node.getNodes() == null) {
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
			String value;
			if ((node.getValue() instanceof ValueString)) {
				value = ((ValueString) node.getValue()).getValue();
			} else if ((node.getValue() instanceof ValueNamedConst)) {
				value = ((ValueNamedConst) node.getValue()).getValue();
			} else if ((node.getValue() instanceof ValueNumber)) {
				value = ((ValueNumber) node.getValue()).getValue();
			} else if ((node.getValue() instanceof ValueCode)) {
				value = ((ValueCode) node.getValue()).getValue();
			} else {
				error("Quoted string or uppercase named const usualy expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
				return;
			}

			if (!type.contains(value)) {
				warning("Not a valide value.",
						UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		}
		case STRING:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case COLOR:
			validateColor(node);
			break;
		case DATE:
			validateVector(node, 3, true);
			break;
		case ID:
			validateReference(node, type);
			break;
		case INT:
			if (!(node.getValue() instanceof ValueNumber)) {
				error("Integer expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			try {
				String value = ((ValueNumber)node.getValue()).getValue();
				Integer.parseInt(value);
			} catch (NumberFormatException e) {
				error("Integer expected but real found", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case REAL:
			if (!(node.getValue() instanceof ValueNumber)) {
				error("Number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case BOOLEAN:
			if (!(node.getValue() instanceof ValueBoolean)) {
				error("Boolean expected (true or false)", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case PICS_IMAGE:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted image name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString value = (ValueString) node.getValue();
				final String id = value.getValue();
				final File file = UfoResources.getImageFileFromPics(id);
				if (file == null) {
					warning("Image not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case BASE_IMAGE:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted image name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString value = (ValueString) node.getValue();
				final String id = value.getValue();
				final File file = UfoResources.getImageFileFromBase(id);
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
				final File file = UfoResources.getModelFile(id);
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
				final File file = UfoResources.getSoundFileFromSound(id);
				if (file == null) {
					warning("Sound not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case MUSIC:
			if (!(node.getValue() instanceof ValueString)) {
				error("Quoted music name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString value = (ValueString) node.getValue();
				final String id = value.getValue();
				final File file = UfoResources.getSoundFileFromMusic(id);
				if (file == null) {
					warning("Music not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case VEC2:
			validateVector(node, 2, false);
			break;
		case VEC3:
			validateVector(node, 3, false);
			break;
		case VEC4:
			validateVector(node, 4, false);
			break;
		case SHAPE:
			validateVector(node, 4, true);
			break;
		default:
			System.out.println("Type \"" + type + "\" not yet supported");
		}
	}

	private void validateColor(UFONode node) {
		if (!(node.getValue() instanceof ValueString)) {
			error("Quoted set of components expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			return;
		}
		// check useless trail
		String value = ((ValueString)node.getValue()).getValue();
		if (!value.trim().equals(value)) {
			warning("Color contain extra spaces", UfoScriptPackage.Literals.UFO_NODE__VALUE);
		}
		// check dimension
		String[] values = value.trim().split("\\s+");
		if (values.length < 3 || values.length > 4) {
			error("Color must have 3 or 4 components", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			return;
		}
		// check element validity
		for (String v: values) {
			try {
				double d = Double.parseDouble(v);
				if (d < 0 || d > 1) {
					error("Color component must be between 0 and 1", UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			} catch (NumberFormatException e) {
				error("Vector contains non real values (" + v + ")", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
		}
	}

	private void validateVector(UFONode node, int dimension, boolean integer) {
		if (!(node.getValue() instanceof ValueString)) {
			error("Quoted set of number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			return;
		}
		// check useless trail
		String value = ((ValueString)node.getValue()).getValue();
		if (!value.trim().equals(value)) {
			warning("Data contains useless spaces", UfoScriptPackage.Literals.UFO_NODE__VALUE);
		}
		// check dimension
		String[] values = value.trim().split("\\s+");
		if (values.length != dimension) {
			error("Data must contains " + dimension + " elements", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			return;
		}
		// check element validity
		for (String v: values) {
			if (integer) {
				try {
					Integer.parseInt(v);
				} catch (NumberFormatException e) {
					error("Data contains non integer values (" + v + ")", UfoScriptPackage.Literals.UFO_NODE__VALUE);
					break;
				}
			} else {
				try {
					Double.parseDouble(v);
				} catch (NumberFormatException e) {
					error("Data contains non real values (" + v + ")", UfoScriptPackage.Literals.UFO_NODE__VALUE);
					break;
				}
			}
		}
	}

	@Check
	public void checkNode(UFONode property) {
		validate(property);
	}

}
