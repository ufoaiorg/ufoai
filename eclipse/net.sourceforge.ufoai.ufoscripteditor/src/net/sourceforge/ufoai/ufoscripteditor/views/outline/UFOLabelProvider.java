package net.sourceforge.ufoai.ufoscripteditor.views.outline;

import java.net.URL;

import net.sourceforge.ufoai.ufoscripteditor.Activator;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptBlock;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.ILabelProviderListener;
import org.eclipse.swt.graphics.Image;

public class UFOLabelProvider implements ILabelProvider {
	@Override
	public void addListener(ILabelProviderListener arg0) {
	}

	@Override
	public void dispose() {
	}

	@Override
	public boolean isLabelProperty(Object arg0, String arg1) {
		return false;
	}

	@Override
	public void removeListener(ILabelProviderListener arg0) {
	}

	@Override
	public Image getImage(Object arg0) {
		UFOScriptBlock block = (UFOScriptBlock) arg0;
		IUFOParserFactory factory = block.getParser();
		if (factory == null)
			return getIcon("icons/key.png");
		String iconPath = factory.getIcon();
		return getIcon(iconPath);
	}

	private Image getIcon(String iconPath) {
		ImageRegistry imageRegistry = Activator.getDefault().getImageRegistry();
		Image image = imageRegistry.get(iconPath);
		if (image == null) {
			URL url = Activator.getDefault().getBundle().getResource(iconPath);
			imageRegistry.put(iconPath, ImageDescriptor.createFromURL(url));
			image = imageRegistry.get(iconPath);
		}
		return image;
	}

	@Override
	public String getText(Object arg0) {
		UFOScriptBlock block = (UFOScriptBlock) arg0;
		return block.getName();
	}
}
