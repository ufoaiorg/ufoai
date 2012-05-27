package net.sourceforge.ufoai;

import org.eclipse.jface.dialogs.TrayDialog;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

public class UFOAIPlugin extends AbstractUIPlugin {
	public static final String ID_PLUGIN = "net.sourceforge.ufoai"; //$NON-NLS-1$

	// The shared instance
	private static UFOAIPlugin plugin;

	/**
	 * The constructor
	 */
	public UFOAIPlugin() {
	}

	@Override
	public void start(BundleContext context) throws Exception {
		super.start(context);
		plugin = this;
		UFOScriptStandaloneSetup.doSetup();
		TrayDialog.setDialogHelpAvailable(true);
	}

	@Override
	public void stop(BundleContext context) throws Exception {
		plugin = null;
		super.stop(context);
	}

	public static UFOAIPlugin getDefault() {
		return plugin;
	}

	public static ImageDescriptor getImageDescriptor(String path) {
		return imageDescriptorFromPlugin(ID_PLUGIN, path);
	}

	public static Shell getShell() {
		return plugin.getWorkbench().getDisplay().getActiveShell();
	}

	public static IWorkbenchWindow getWorkbenchWindow() {
		IWorkbench workbench = plugin.getWorkbench();
		if (workbench.getActiveWorkbenchWindow() != null) {
			return workbench.getActiveWorkbenchWindow();
		}

		if (workbench.getWorkbenchWindowCount() == 0) {
			return null;
		}

		return workbench.getWorkbenchWindows()[0];
	}
}
