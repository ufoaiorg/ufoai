package net.sourceforge.ufoai.util;

import net.sourceforge.ufoai.UFOAIPlugin;

import org.eclipse.swt.widgets.MessageBox;
import org.eclipse.ui.IWorkbenchWindow;

public class UIUtil {
	private static final class MessageBoxRunnable implements Runnable {
		private final IWorkbenchWindow window;
		private final int style;
		private final String message;
		private int result;

		private MessageBoxRunnable(IWorkbenchWindow window, String message, int style) {
			this.window = window;
			this.style = style;
			this.message = message;
		}

		@Override
		public void run() {
			MessageBox box = new MessageBox(window.getShell(), style);
			box.setMessage(message);
			result = box.open();
		}

		public int getResult() {
			return result;
		}
	}

	public static int openMessageBox(final IWorkbenchWindow window, final String message, final int style) {
		if (window == null || window.getShell() == null) {
			return -1;
		}
		if (StringUtil.isNull(message)) {
			return -1;
		}

		try {
			MessageBoxRunnable runnable = new MessageBoxRunnable(window, message, style);
			window.getShell().getDisplay().syncExec(runnable);
			return runnable.getResult();
		} catch (Exception e) {
			return -1;
		}
	}

	public static int openMessageBox(String message, int style) {
		return openMessageBox(UFOAIPlugin.getWorkbenchWindow(), message, style);
	}
}
