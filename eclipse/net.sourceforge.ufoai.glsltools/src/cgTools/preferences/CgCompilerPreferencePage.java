package cgTools.preferences;

import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubProgressMonitor;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.jface.preference.DirectoryFieldEditor;
import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.preference.FileFieldEditor;
import org.eclipse.jface.preference.RadioGroupFieldEditor;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

import cgTools.CgToolsPlugin;
import cgTools.builder.cgBuilder;
import cgTools.builder.cgCompiler;

/**
 * This class represents a preference page that is contributed to the
 * Preferences dialog. By subclassing <samp>FieldEditorPreferencePage</samp>, we
 * can use the field support built into JFace that allows us to create a page
 * that is small and knows how to save, restore and apply itself.
 * <p>
 * This page is used to modify preferences only. They are stored in the
 * preference store that belongs to the main plug-in class. That way,
 * preferences can be accessed directly via the preference store.
 */

public class CgCompilerPreferencePage extends FieldEditorPreferencePage
		implements IWorkbenchPreferencePage {
	public CgCompilerPreferencePage() {
		super(GRID);
		setPreferenceStore(CgToolsPlugin.getDefault().getPreferenceStore());
		setDescription("Shaders compilers configuration");
	}

	/**
	 * Creates the field editors. Field editors are abstractions of the common
	 * GUI blocks needed to manipulate various types of preferences. Each field
	 * editor knows how to save and restore itself.
	 */
	@Override
	public void createFieldEditors() {
		addField(new FileFieldEditor(PreferenceConstants.GLSLPATH,
				"&GLSL front end compiler exe", getFieldEditorParent()));

		addField(new DirectoryFieldEditor(PreferenceConstants.CGPATH,
				"&Cgc Bin Directory", getFieldEditorParent()));

		String s[][] = new String[PreferenceConstants.cgProfiles.length][2];
		for (int i = 0; i < PreferenceConstants.cgProfiles.length; i++) {
			s[i][0] = PreferenceConstants.cgProfilesDescriptor[i];
			s[i][1] = PreferenceConstants.cgProfiles[i];
		}
		addField(new RadioGroupFieldEditor(PreferenceConstants.CGPROFILE,
				"Active Cg Profile", 1, s, getFieldEditorParent()));
	}

	/*
	 * (non-Javadoc)
	 *
	 * @see
	 * org.eclipse.ui.IWorkbenchPreferencePage#init(org.eclipse.ui.IWorkbench)
	 */
	public void init(IWorkbench workbench) {
	}

	@Override
	public boolean performOk() {
		doFullBuild();
		return super.performOk();
	}

	private void doFullBuild() {
		Job buildJob = new Job("Building Shaders Files") { //$NON-NLS-1$

			//$NON-NLS-1$
			/*
			 * (non-Javadoc)
			 *
			 * @see
			 * org.eclipse.core.runtime.jobs.Job#run(org.eclipse.core.runtime
			 * .IProgressMonitor)
			 */
			@Override
			protected IStatus run(IProgressMonitor monitor) {
				try {
					IProject[] projects = null;

					projects = ResourcesPlugin.getWorkspace().getRoot()
							.getProjects();

					monitor.beginTask("", projects.length * 2); //$NON-NLS-1$
					for (int i = 0; i < projects.length; i++) {
						IProject projectToBuild = projects[i];
						if (!projectToBuild.isOpen())
							continue;
						if (projectToBuild.hasNature(cgCompiler.NATURE_ID)) {
							projectToBuild.build(
									IncrementalProjectBuilder.FULL_BUILD,
									cgBuilder.BUILDER_ID, null,
									new SubProgressMonitor(monitor, 1));
						} else {
							monitor.worked(2);
						}
					}
				} catch (CoreException e) {
					return e.getStatus();
				} catch (OperationCanceledException e) {
					return Status.CANCEL_STATUS;
				} finally {
					monitor.done();
				}
				return Status.OK_STATUS;
			}
		};
		buildJob.setRule(ResourcesPlugin.getWorkspace().getRuleFactory()
				.buildRule());
		buildJob.setUser(true);
		buildJob.schedule();
	}
}
