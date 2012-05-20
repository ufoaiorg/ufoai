package net.sourceforge.ufoai.ui.properties;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import net.sourceforge.ufoai.ufoScript.UINode;
import net.sourceforge.ufoai.ufoScript.UINodeComponent;
import net.sourceforge.ufoai.ufoScript.UINodeWindow;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.emf.common.util.EList;
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

		if (object instanceof UINode) {
			String category = "Node";
			addProperty(category, "Name", ((UINode) object).getName());
			addPropertiesFromNode(object);
		} else if (object instanceof UINodeComponent) {
			UINodeComponent component = (UINodeComponent) object;
			String category = "Component";
			addProperty(category, "Name", component.getName());
			addProperty(category, "Child count", String.valueOf(component.getNodes().size()));
			addProperty(category, "Properties count", String.valueOf(component.getProperties().size()));
			addPropertiesFromNode(object);
		} else if (object instanceof UINodeWindow) {
			UINodeWindow window = (UINodeWindow) object;
			String category = "Window";
			addProperty(category, "Name", window.getName());
			addProperty(category, "Child count", String.valueOf(window.getNodes().size()));
			addProperty(category, "Properties count", String.valueOf(window.getProperties().size()));
			addPropertiesFromNode(object);
		}
	}

	private void addPropertiesFromNode(Object object) {
		Method getter;
		try {
			getter = object.getClass().getMethod("getProperties", new Class<?>[]{});
		} catch (SecurityException e) {
			e.printStackTrace();
			return;
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
			return;
		}
		Object propertyHolders;
		try {
			propertyHolders = getter.invoke(object, new Object[]{});
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
			return;
		} catch (IllegalAccessException e) {
			e.printStackTrace();
			return;
		} catch (InvocationTargetException e) {
			e.printStackTrace();
			return;
		}

		if (!(propertyHolders instanceof EList)) {
			System.out.println("PropertyHolder is not a list");
		}

		EList<?> list = (EList<?>) propertyHolders;
		for (Object propertyHolder : list) {
			addPropertiesFromHolders(propertyHolder);
		}
	}

	private void addPropertiesFromHolders(Object propertyHolder) {
		Method[] getters = propertyHolder.getClass().getMethods();
		String category = propertyHolder.getClass().getSimpleName();
		for (Method getter: getters) {
			if (!getter.getName().startsWith("get"))
				continue;
			String label = getter.getName().replaceFirst("get", "");
			String value;
			try {
				value = "" + getter.invoke(propertyHolder, new Object[]{});
			} catch (IllegalArgumentException e) {
				e.printStackTrace();
				continue;
			} catch (IllegalAccessException e) {
				e.printStackTrace();
				continue;
			} catch (InvocationTargetException e) {
				e.printStackTrace();
				continue;
			}

			addProperty(category, label, value);
		}
	}

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
