package net.sourceforge.ufoai.validation;

import java.io.File;

import net.sourceforge.ufoai.ufoScript.NamedBlock;
import net.sourceforge.ufoai.ufoScript.Property;
import net.sourceforge.ufoai.ufoScript.PropertyValueBlock;
import net.sourceforge.ufoai.ufoScript.PropertyValueBoolean;
import net.sourceforge.ufoai.ufoScript.PropertyValueID;
import net.sourceforge.ufoai.ufoScript.PropertyValueNamedConst;
import net.sourceforge.ufoai.ufoScript.PropertyValueNumber;
import net.sourceforge.ufoai.ufoScript.PropertyValueString;
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

	private String getPath(NamedBlock block) {
		return block.getType();
	}

	private String getPath(Property property) {
		String path = null;
		while (property != null) {
			if (path == null) {
				path = property.getType();
			} else {
				path = property.getType() + "." + path;
			}
			if (property.eContainer() instanceof NamedBlock) {
				NamedBlock block = (NamedBlock) property.eContainer();
				return getPath(block) + "." + path;
			}
			if (property.eContainer() instanceof PropertyValueBlock) {
				EObject block = property.eContainer();
				if (block.eContainer() instanceof Property) {
					property = (Property) block.eContainer();
				} else {
					return null;
				}
			} else {
				return null;
			}
		}
		return path;
	}

	private UFOType getType(String path) {
		return UFOTypes.getInstance().getPathType(path);
	}

	private void validate(Property property) {
		String path = getPath(property);
		UFOType type = getType(path);

		// No rules
		if (type == null) {
			error("Property name unknown", UfoScriptPackage.Literals.PROPERTY__TYPE);
			return;
		}

		switch (type.getType()) {
		case BLOCK:
			if (!(property.getValue() instanceof PropertyValueBlock)) {
				error("Block is expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		case TRANSLATED_STRING:
			if (!(property.getValue() instanceof PropertyValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			} else {
				PropertyValueString value = (PropertyValueString) property.getValue();
				if (!value.getValue().startsWith("_")) {
					warning("String should be prefixed by '_' to became translatable.",
							UfoScriptPackage.Literals.PROPERTY__VALUE);
				}
			}
			break;
		case ENUM:
		{
			if ((property.getValue() instanceof PropertyValueString)) {
				PropertyValueString value = (PropertyValueString) property.getValue();
				if (!type.contains(value.getValue())) {
					warning("Not a valide value.",
							UfoScriptPackage.Literals.PROPERTY__VALUE);
				}
			} else if ((property.getValue() instanceof PropertyValueNamedConst)) {
				PropertyValueNamedConst value = (PropertyValueNamedConst) property.getValue();
				if (!type.contains(value.getValue())) {
					warning("Not a valide value.",
							UfoScriptPackage.Literals.PROPERTY__VALUE);
				}
			} else {
				error("Quoted string or uppercase named const expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		}
		case STRING:
			if (!(property.getValue() instanceof PropertyValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		case COLOR:
			if (!(property.getValue() instanceof PropertyValueString)) {
				error("Quoted string expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		case ID:
			if (!(property.getValue() instanceof PropertyValueID)) {
				error("Id expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		case TECH_ID:
			if (!(property.getValue() instanceof PropertyValueID)) {
				error("Id expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			} else {
				PropertyValueID value = (PropertyValueID) property.getValue();
				if (!value.getValue().getType().equals("tech")) {
					error("Tech id expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
				}
			}
			break;
		case BUILDING_ID:
			if (!(property.getValue() instanceof PropertyValueID)) {
				error("Id expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			} else {
				PropertyValueID value = (PropertyValueID) property.getValue();
				if (!value.getValue().getType().equals("building")) {
					error("Building id expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
				}
			}
			break;
		case INT:
			if (!(property.getValue() instanceof PropertyValueNumber)) {
				error("Number expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		case REAL:
			if (!(property.getValue() instanceof PropertyValueNumber)) {
				error("Number expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		case BOOLEAN:
			if (!(property.getValue() instanceof PropertyValueBoolean)) {
				error("Number expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		case PICS_IMAGE:
			if (!(property.getValue() instanceof PropertyValueString)) {
				error("Quoted image name expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			} else 	{
				checkUfoBase(property);
				PropertyValueString value = (PropertyValueString) property.getValue();
				final String id = value.getValue();
				final File file = UfoResources.getFileFromPics(id);
				if (file == null) {
					warning("Image not found.",
							UfoScriptPackage.Literals.PROPERTY__VALUE);
				}
			}
			break;
		case VEC2:
			if (!(property.getValue() instanceof PropertyValueString)) {
				error("Quoted tuple of number expected", UfoScriptPackage.Literals.PROPERTY__VALUE);
			}
			break;
		default:
			System.out.println("Type \"" + type + "\" not yet supported");
		}
	}

	@Check
	public void checkProperty(Property property) {
		validate(property);
	}

	@Check
	public void checkNamedBlock(NamedBlock block) {
		String path = getPath(block);
		UFOType type = getType(path);
		if (type == null) {
			error("Definition unknown", UfoScriptPackage.Literals.NAMED_BLOCK__TYPE);
			return;
		}

		if (type.getType() != UFOType.Type.BLOCK) {
			throw new Error("Property file is wrong. Main block must be a block type.");
		}
	}
}
