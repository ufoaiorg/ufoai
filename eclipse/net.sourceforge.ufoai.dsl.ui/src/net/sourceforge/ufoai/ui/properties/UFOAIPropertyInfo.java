package net.sourceforge.ufoai.ui.properties;

import java.util.ArrayList;
import java.util.List;

import net.sourceforge.ufoai.ufoScript.UINode;
import net.sourceforge.ufoai.ufoScript.UINodePanel;
import net.sourceforge.ufoai.ufoScript.UINodeWindow;
import net.sourceforge.ufoai.ufoScript.UITopLevelNode;
import net.sourceforge.ufoai.ufoScript.UIWindowNodeProperties;
import net.sourceforge.ufoai.ufoScript.UIWindowNodePropertiesBase;

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

		public PropertyEntry(Object id, String category, String label,
				String value) {
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
		addProperty("Java Object", "Type", object.getClass().getSimpleName());

		if (object instanceof UINode) {
			addProperty("Node", "Name", ((UINode) object).getName());
		}
		if (object instanceof UITopLevelNode) {
			addProperty("Node", "Name", ((UITopLevelNode) object).getName());
		}
		if (object instanceof UINodePanel) {
			addProperty("Node", "Name", ((UINodePanel) object).getName());
		}
		if (object instanceof UINodeWindow) {
			UINodeWindow window = (UINodeWindow) object;
			addProperty("Window", "Child count",
					String.valueOf(window.getNodes().size()));
			addProperty("Window", "Panel child count",
					String.valueOf(window.getPanels().size()));
			addProperty("Window", "Properties count",
					String.valueOf(window.getProperties().size()));

			EList<UIWindowNodeProperties> properties = window.getProperties();
			for (UIWindowNodeProperties prop : properties) {
				if (prop instanceof UIWindowNodePropertiesBase) {
					UIWindowNodePropertiesBase windowProp = (UIWindowNodePropertiesBase) prop;
					addProperty("Window", "Close button",
							windowProp.getClosebutton());
					addProperty("Window", "Fill", windowProp.getFill());
					// TODO and so on for all properties...
				}
			}
		}
		// TODO and so on for all behaviours...
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
