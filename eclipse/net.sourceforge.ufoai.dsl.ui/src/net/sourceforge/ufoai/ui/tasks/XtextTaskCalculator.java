package net.sourceforge.ufoai.ui.tasks;

import java.lang.reflect.InvocationTargetException;

import net.sourceforge.ufoai.util.StringUtil;

import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubMonitor;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.ui.actions.WorkspaceModifyOperation;
import org.eclipse.xtext.nodemodel.BidiIterator;
import org.eclipse.xtext.nodemodel.ICompositeNode;
import org.eclipse.xtext.nodemodel.INode;
import org.eclipse.xtext.nodemodel.util.NodeModelUtils;
import org.eclipse.xtext.resource.XtextResource;
import org.eclipse.xtext.ui.editor.IXtextEditorCallback;
import org.eclipse.xtext.ui.editor.XtextEditor;
import org.eclipse.xtext.ui.editor.model.IXtextDocument;
import org.eclipse.xtext.util.concurrent.IUnitOfWork;

import com.google.inject.Inject;

public class XtextTaskCalculator extends IXtextEditorCallback.NullImpl {
	public static String TASK_MARKER_TYPE = "";

	@Inject
	private UpdateTaskMarkerJob objTaskMarkJob;

	@Override
	public void afterCreatePartControl(XtextEditor editor) {
		updateTaskMarkers(editor);
	}

	// update markers after a save
	@Override
	public void afterSave(XtextEditor argEditor) {
		updateTaskMarkers(argEditor);
	}

	private void updateTaskMarkers(XtextEditor argEditor) {
		// cancel does not really make sense, as it should be called
		// just *before* saving, unfortunately there is no corresponding
		// callback
		objTaskMarkJob.cancel();
		objTaskMarkJob.setEditor(argEditor);
		if (objTaskMarkJob.isSystem()) {
			objTaskMarkJob.setSystem(false);
		}
		objTaskMarkJob.setPriority(Job.DECORATE);
		objTaskMarkJob.schedule();
	}

	public static class UpdateTaskMarkerJob extends Job {
		@Inject
		private ITaskElementChecker objElementChecker;
		XtextEditor objEditor;

		public UpdateTaskMarkerJob() {
			super("Xtext-Task-Marker-Update-Job");
		}

		void setEditor(XtextEditor argEditor) {
			objEditor = argEditor;
		}

		@Override
		public IStatus run(final IProgressMonitor argMonitor) {
			if (!argMonitor.isCanceled()) {
				try {
					new UpdateTaskMarkersOperation(objEditor, objElementChecker).run(SubMonitor.convert(argMonitor));
				} catch (Exception e) {
					e.printStackTrace();
					// don't care for now
				}
			}
			return argMonitor.isCanceled() ? Status.CANCEL_STATUS : Status.OK_STATUS;
		}
	}

	static class UpdateTaskMarkersOperation extends WorkspaceModifyOperation {
		private XtextEditor objEditor;
		private ITaskElementChecker objElementChecker;

		UpdateTaskMarkersOperation(XtextEditor argEditor, ITaskElementChecker argElementChecker) {
			objEditor = argEditor;
			objElementChecker = argElementChecker;
		}

		@Override
		protected void execute(IProgressMonitor monitor) throws CoreException, InvocationTargetException, InterruptedException {
			IResource resource = objEditor.getResource();
			resource.deleteMarkers(getMarkerType(), true, IResource.DEPTH_INFINITE);
			if (!monitor.isCanceled()) {
				createNewMarkers(objEditor, monitor);
			}
		}

		private void createNewMarkers(final XtextEditor argEditor, final IProgressMonitor argMonitor) throws CoreException {
			final IResource varResource = argEditor.getResource();
			IXtextDocument document = argEditor.getDocument();
			if (document == null)
				return;
			document.readOnly(new IUnitOfWork<Void, XtextResource>() {
				public java.lang.Void exec(XtextResource argState) throws Exception {
					if (argState != null && !argState.getContents().isEmpty()) {
						EObject varModel = argState.getContents().get(0);
						ICompositeNode varRoot = NodeModelUtils.getNode(varModel);
						visit(varRoot, varResource, argMonitor);
					}
					return null;
				}
			});
		}

		private void visit(ICompositeNode node, final IResource varResource, final IProgressMonitor argMonitor) throws CoreException {
			BidiIterator<INode> varAllContents = node.getChildren().iterator();
			while (varAllContents.hasNext() && !argMonitor.isCanceled()) {
				INode varNext = varAllContents.next();
				internalCreateMarker(varNext, varResource);
				if (varNext instanceof ICompositeNode) {
					visit((ICompositeNode) varNext, varResource, argMonitor);
				}
			}
		}

		private void internalCreateMarker(INode argNode, IResource argRresource) throws CoreException {
			String varIgnorePrefix = objElementChecker.getPrefixToIgnore(argNode);
			if (varIgnorePrefix != null) {
				IMarker varMarker = argRresource.createMarker(getMarkerType());
				String text = argNode.getText();
				int index = text.indexOf(varIgnorePrefix);
				if (index > 0) {
					text = text.substring(index + varIgnorePrefix.length());
				}
				varMarker.setAttribute(IMarker.MESSAGE, text.trim());
				varMarker.setAttribute(IMarker.LOCATION, "line " + argNode.getStartLine());
				varMarker.setAttribute(IMarker.CHAR_START, argNode.getOffset());
				varMarker.setAttribute(IMarker.CHAR_END, argNode.getOffset() + argNode.getLength());
				varMarker.setAttribute(IMarker.USER_EDITABLE, false);
			}
		}
	}

	public static String getMarkerType() {
		if (StringUtil.isNull(TASK_MARKER_TYPE)) {
			String foundMarkerType = "";
			IConfigurationElement[] config = Platform.getExtensionRegistry().getConfigurationElementsFor(
					"org.eclipse.ui.editors.annotationTypes");
			String markerType = null;
			for (IConfigurationElement e : config) {
				markerType = e.getAttribute("markerType");
				if (markerType != null && markerType.endsWith(TaskConstants.XTEXT_MARKER_SIMPLE_NAME)) {
					foundMarkerType = markerType;
					break;
				}
			}
			return foundMarkerType;
		}
		return TASK_MARKER_TYPE;
	}
}
