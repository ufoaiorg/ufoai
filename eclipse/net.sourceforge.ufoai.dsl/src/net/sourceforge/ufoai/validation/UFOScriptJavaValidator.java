package net.sourceforge.ufoai.validation;

import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import net.sourceforge.ufoai.ufoScript.UFONode;
import net.sourceforge.ufoai.ufoScript.UfoScriptPackage;
import net.sourceforge.ufoai.ufoScript.Value;
import net.sourceforge.ufoai.ufoScript.ValueBoolean;
import net.sourceforge.ufoai.ufoScript.ValueCode;
import net.sourceforge.ufoai.ufoScript.ValueNamedConst;
import net.sourceforge.ufoai.ufoScript.ValueNumber;
import net.sourceforge.ufoai.ufoScript.ValueReference;
import net.sourceforge.ufoai.ufoScript.ValueString;
import net.sourceforge.ufoai.util.StringUtil;
import net.sourceforge.ufoai.util.UfoResources;
import net.sourceforge.ufoai.validation.UFOType.Type;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.Path;
import org.eclipse.emf.common.util.EList;
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

	private UFOType getType(EObject object) {
		if (object instanceof UFONode) {
			String path = getPath((UFONode) object);
			return getType(path);
		}
		return null;
	}

	private UFOType getType(String path) {
		return UFOTypes.getInstance().getPathType(path);
	}

	private static String[] geoscapeImages = {
		"_atmosphere",
		"_culture",
		"_day",
		"_halo",
		"_nations_overlay_glow",
		"_nations_overlay",
		"_nations",
		"_night",
		"_night_lights",
		"_population",
		"_stars",
		"_terrain"
	};

	private void validate(UFONode node, Value value, UFOType type) {
		switch (type.getType()) {
		case IDENTIFIED_BLOCK:
			if (!node.getValues().isEmpty()) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (value != null || node.getNodes() == null) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (node.getName() == null) {
				error("Identified block expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case ANONYMOUS_BLOCK:
			if (!node.getValues().isEmpty()) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (value != null || node.getNodes() == null) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (node.getName() != null) {
				error("Anonymous block expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case ANONYMOUS_MAP_BLOCK:
			if (!node.getValues().isEmpty()) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (value != null || node.getNodes() == null) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (node.getName() != null) {
				error("Anonymous block expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			Set<String> keys = new HashSet<String>();
			for (UFONode child: node.getNodes()) {
				String childType = child.getType();
				if (!keys.add(childType)) {
					error("Duplicated key", child, UfoScriptPackage.Literals.UFO_NODE__TYPE, 0);
				}
			}
			break;
		case BLOCK:
			if (!node.getValues().isEmpty()) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			if (value != null || node.getNodes() == null) {
				error("Block is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case TRANSLATED_STRING:
			if (!(value instanceof ValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else {
				ValueString vs = (ValueString) value;
				if (!vs.getValue().startsWith("_") && !vs.getValue().startsWith("*msgid:")) {
					warning("String should be prefixed by '_' to became translatable or should have a msgid reference with *msgid:ID.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case ENUM:
		{
			String v;
			if ((value instanceof ValueString)) {
				v = ((ValueString) value).getValue();
			} else if ((value instanceof ValueNamedConst)) {
				v = ((ValueNamedConst) value).getValue();
			} else if ((value instanceof ValueNumber)) {
				v = ((ValueNumber) value).getValue();
			} else if ((value instanceof ValueCode)) {
				v = ((ValueCode) value).getValue();
			} else {
				error("Quoted string or uppercase named const usualy expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
				return;
			}

			if (!type.contains(v)) {
				warning("Not a valide value.",
						UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		}
		case STRING:
			if (!(value instanceof ValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case COLOR:
			validateColor(value);
			break;
		case DATE:
			validateVector(value, 3, true);
			break;
		case ID:
			validateReference(value, type);
			break;
		case INT:
			if (!(value instanceof ValueNumber)) {
				error("Integer expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			try {
				String string = ((ValueNumber)value).getValue();
				Integer.parseInt(string);
			} catch (NumberFormatException e) {
				error("Integer expected but real found", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case REAL:
			if (!(value instanceof ValueNumber)) {
				error("Number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case BOOLEAN:
			if (!(value instanceof ValueBoolean)) {
				error("Boolean expected (true or false)", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case PICS_IMAGE:
			if (!(value instanceof ValueString)) {
				error("Quoted image name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString vs = (ValueString) value;
				final String id = vs.getValue();
				final File file = UfoResources.getImageFileFromPics(id);
				if (file == null) {
					warning("Image not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case BASE_IMAGE:
			if (!(value instanceof ValueString)) {
				error("Quoted image name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString vs = (ValueString) value;
				final String id = vs.getValue();
				final File file = UfoResources.getImageFileFromBase(id);
				if (file == null) {
					warning("Image not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case GEOSCAPE_IMAGE:
			if (!(value instanceof ValueString)) {
				error("Quoted image name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString vs = (ValueString) value;
				final String id = vs.getValue();
				for (String suffix : geoscapeImages) {
					String fileId = "geoscape/" + id + suffix;
					final File file = UfoResources.getImageFileFromPics(fileId);
					if (file == null) {
						warning("Image \"" + fileId + "\" not found.",
								UfoScriptPackage.Literals.UFO_NODE__VALUE);
					}
				}
			}
			break;
		case MODEL:
			if (!(value instanceof ValueString)) {
				error("Quoted model name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString vs = (ValueString) value;
				final String id = vs.getValue();
				final File file = UfoResources.getModelFile(id);
				if (file == null) {
					warning("Model not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case MODEL_OR_ID:
			if (value instanceof ValueReference) {
				validateReference(value, type);
			} else if (value instanceof ValueString) {
				checkUfoBase(node);
				ValueString vs = (ValueString) value;
				final String id = vs.getValue();
				final File file = UfoResources.getModelFile(id);
				if (file == null) {
					warning("Model not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			} else {
				error("Expected a reference to a menu_model or a model name", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			break;
		case SOUND:
			if (!(value instanceof ValueString)) {
				error("Quoted sound name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString v = (ValueString) value;
				final String id = v.getValue();
				final File file = UfoResources.getSoundFileFromSound(id);
				if (file == null) {
					warning("Sound not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case MUSIC:
			if (!(value instanceof ValueString)) {
				error("Quoted music name expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			} else 	{
				checkUfoBase(node);
				ValueString vs = (ValueString) value;
				final String id = vs.getValue();
				final File file = UfoResources.getSoundFileFromMusic(id);
				if (file == null) {
					warning("Music not found.",
							UfoScriptPackage.Literals.UFO_NODE__VALUE);
				}
			}
			break;
		case VEC2:
			validateVector(value, 2, false);
			break;
		case VEC3:
			validateVector(value, 3, false);
			break;
		case VEC4:
			validateVector(value, 4, false);
			break;
		case SHAPE:
			validateVector(value, 4, true);
			break;
		case LIST:
		case IDENTIFIED_LIST:
		{
			if (!node.getNodes().isEmpty()) {
				error("List is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
				return;
			}
			if (type.getType() == UFOType.Type.IDENTIFIED_LIST && node.getName() == null) {
				error("Identified block expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
			String path = getPath(node);
			UFOType elementType = getType(path + ".ELEMENT");
			if (elementType == null) {
				warning("Elements of the list has no validators", UfoScriptPackage.Literals.UFO_NODE__TYPE);
				return;
			}
			for (Value child: node.getValues()) {
				validate(node, child, elementType);
			}
			break;
		}
		case TUPLE:
		{
			if (!node.getNodes().isEmpty()) {
				error("List is expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
				return;
			}

			String path = getPath(node);
			List<UFOType> elementTypes = new ArrayList<UFOType>();
			for (int i = 1; i < 10; i++) {
				UFOType t = getType(path + ".ELEMENT" + i);
				if (t == null)
					break;
				elementTypes.add(t);
			}

			if (elementTypes.size() == 0) {
				// We hope a tuple of 0 elements is not valid too
				warning("Elements of the list has no validators", UfoScriptPackage.Literals.UFO_NODE__TYPE);
				return;
			}

			EList<Value> values = node.getValues();
			if (elementTypes.size() != values.size()) {
				error(elementTypes.size() + " elements expected, but " + values.size() + " found", UfoScriptPackage.Literals.UFO_NODE__VALUE);
				return;
			}

			for (int i = 0; i < elementTypes.size(); i++) {
				if (i >= values.size())
					break;
				UFOType t = elementTypes.get(i);
				validate(node, values.get(i), t);
			}
			break;
		}
		default:
			System.out.println("Type \"" + type + "\" not yet supported");
		}
	}

	private void validateColor(Value valueObject) {
		if (!(valueObject instanceof ValueString)) {
			error("Quoted set of components expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			return;
		}
		// check useless trail
		String value = ((ValueString)valueObject).getValue();
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

	private void validateVector(Value valueObject, int dimension, boolean integer) {
		if (!(valueObject instanceof ValueString)) {
			error("Quoted set of number expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			return;
		}
		// check useless trail
		String value = ((ValueString)valueObject).getValue();
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

	private void validateReference(Value value, UFOType type) {
		/*if (node.getValue() instanceof ValueNull) {
			// nothing
		} else
		 */
		if (value instanceof ValueReference) {
			ValueReference v = (ValueReference) value;
			if (v.getValue() == null) {
				// xtext should already warn about it
				return;
			}
			if (type.hasValues() && !type.contains(v.getValue().getType())) {
				String expected = StringUtil.join(" or ", type.getValues());
				error(expected + " id expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
			}
		} else {
			error("Id expected", UfoScriptPackage.Literals.UFO_NODE__VALUE);
		}
	}

	@Check
	public void checkNode(UFONode node) {
		String path = getPath(node);
		UFOType type = getType(path);

		// No rules
		if (type == null) {
			// Only display error if the parent has rules
			UFOType parentType = getType(node.eContainer());
			if (parentType != null) {
				// There is no check for maps
				if (parentType.getType() == Type.ANONYMOUS_MAP_BLOCK) {
					return;
				}
				error("Property name unknown", UfoScriptPackage.Literals.UFO_NODE__TYPE);
			}
			return;
		}

		if (node.getCode() != null) {
			error("This token value is not expected. It is only here for robustness", UfoScriptPackage.Literals.UFO_NODE__TYPE);
		}

		validate(node, node.getValue(), type);
	}

}
