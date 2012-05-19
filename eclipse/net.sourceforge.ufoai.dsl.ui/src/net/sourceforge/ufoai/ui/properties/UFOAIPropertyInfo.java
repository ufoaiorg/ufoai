package net.sourceforge.ufoai.ui.properties;

import java.util.ArrayList;
import java.util.List;

import net.sourceforge.ufoai.ufoScript.UINode;
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

public class UFOAIPropertyInfo  implements IAdaptable, IPropertySource {
	List<PropertyEntry> properties;

	private static class PropertyEntry extends PropertyDescriptor {
		String category;
		String value;

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
		properties = new ArrayList<UFOAIPropertyInfo.PropertyEntry>();
		addProperty("Java Object", "Type", object.getClass().getSimpleName());

		if (object instanceof UINode) {
			addProperty("Node", "Name", ((UINode) object).getName());
		}
		if (object instanceof UITopLevelNode) {
			addProperty("Node", "Name", ((UITopLevelNode) object).getName());
		}
		if (object instanceof UINodeWindow) {
			addProperty("Window", "Child count", ""+((UINodeWindow) object).getNodes().size());
			addProperty("Window", "Panel child count", ""+((UINodeWindow) object).getPanels().size());
			addProperty("Window", "Properties count", ""+((UINodeWindow) object).getProperties().size());

			EList<UIWindowNodeProperties> properties = ((UINodeWindow) object).getProperties();
			for (UIWindowNodeProperties prop: properties) {
				if (prop instanceof UIWindowNodePropertiesBase) {
					addProperty("Window", "Close button", ((UIWindowNodePropertiesBase) prop).getClosebutton());
					addProperty("Window", "Fill", ((UIWindowNodePropertiesBase) prop).getFill());
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

	@SuppressWarnings("rawtypes")
	public Object getAdapter(Class adapter) {
		if(adapter.equals(IPropertySource.class)){
			return this;
		}
		return null;
	}

	public boolean isPropertySet(Object id) {
		return false;
	}

	public Object getPropertyValue(Object id) {
		return ((PropertyEntry)id).value;
	}

	public IPropertyDescriptor[] getPropertyDescriptors() {
		return properties.toArray(new IPropertyDescriptor[]{});
	}

	public Object getEditableValue() {
		return null;
	}

	public void setPropertyValue(Object id, Object value) {
		System.out.println("setPropertyValue Not implemented");
	}

	public void resetPropertyValue(Object id) {
		System.out.println("resetPropertyValue Not implemented");
	}
}
