package net.sourceforge.ufoai.ui.properties;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.ui.views.properties.IPropertyDescriptor;
import org.eclipse.ui.views.properties.IPropertySource;
import org.eclipse.ui.views.properties.PropertyDescriptor;

public class UFOAIPropertyInfo implements IAdaptable, IPropertySource {
	private final List<PropertyEntry> properties = new ArrayList<UFOAIPropertyInfo.PropertyEntry>();

	private static class PropertyEntry extends PropertyDescriptor {
		private final String category;
		private final String value;

		public PropertyEntry(Object id, String category, String label, String value) {
			super(id, label);
			this.category = category;
			this.value = value;
		}

		@Override
		public Object getId() {
			return this;
		}

		@Override
		public String getCategory() {
			return category;
		}
	}

	public UFOAIPropertyInfo(EObject object) {
		if (object == null) {
			addProperty("Java Object", "Type", "null");
			return;
		}

		addProperty("Java Object", "Type", object.getClass().getSimpleName());
/*
		if (object instanceof UIDefaultNode) {
			UIDefaultNode node = (UIDefaultNode) object;
			String category = "Node";
			addProperty(category, "Name", node.getName());
			addObjectProperties(node.getProperties());
		} else if (object instanceof UIComponent) {
			UIComponent component = (UIComponent) object;
			String category = "Component";
			addProperty(category, "Name", component.getName());
			addProperty(category, "Child count", String.valueOf(component.getNodes().size()));
			addProperty(category, "Properties count", String.valueOf(component.getProperties().size()));
			addObjectProperties(component.getProperties());
		} else if (object instanceof UIWindow) {
			UIWindow window = (UIWindow) object;
			String category = "Window";
			addProperty(category, "Name", window.getName());
			addProperty(category, "Child count", String.valueOf(window.getNodes().size()));
			addProperty(category, "Properties count", String.valueOf(window.getProperties().size()));
			addObjectProperties(window.getProperties());
		}
		*/
	}
/*
	private void addObjectProperties(EList<UIPropertyDefinition> properties) {
		for (UIPropertyDefinition property : properties) {
			addObjectProperties(property);
		}
	}

	private void addObjectProperties(UIPropertyDefinition property) {
		String value = null;
		String type = null;
		if (property.getValue() instanceof UIPropertyValueBoolean) {
			UIPropertyValueBoolean v = (UIPropertyValueBoolean) property.getValue();
			// TODO create an EBoolean and update the lexer
			value = v.getValue();
			type = "Boolean";
		}
		else if (property.getValue() instanceof UIPropertyValueNumber) {
			UIPropertyValueNumber v = (UIPropertyValueNumber) property.getValue();
			value = "" + v.getValue();
			type = "Number";
		}
		else if (property.getValue() instanceof UIPropertyValueString) {
			UIPropertyValueString v = (UIPropertyValueString) property.getValue();
			value = v.getValue();
			type = "String";
		}
		else if (property.getValue() instanceof UIPropertyValueFunction) {
			value = "{ ... }";
			type = "Function";
		}

		addProperty("Properties", property.getName() + ": " + type, value);
	}
*/
	private void addProperty(String category, String label, String value) {
		int id = properties.size();
		PropertyEntry entry = new PropertyEntry(id, category, label, value);
		properties.add(entry);
	}

	@Override
	@SuppressWarnings("rawtypes")
	public Object getAdapter(Class adapter) {
		if (adapter.equals(IPropertySource.class)) {
			return this;
		}
		return null;
	}

	@Override
	public boolean isPropertySet(Object id) {
		return false;
	}

	@Override
	public Object getPropertyValue(Object id) {
		return ((PropertyEntry) id).value;
	}

	@Override
	public IPropertyDescriptor[] getPropertyDescriptors() {
		return properties.toArray(new IPropertyDescriptor[] {});
	}

	@Override
	public Object getEditableValue() {
		return this;
	}

	@Override
	public void setPropertyValue(Object id, Object value) {
		System.out.println("setPropertyValue Not implemented");
	}

	@Override
	public void resetPropertyValue(Object id) {
		System.out.println("resetPropertyValue Not implemented");
	}
}
