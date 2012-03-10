package cgTools.builder;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Map;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.resources.IResourceVisitor;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import cgTools.CgToolsPlugin;
import cgTools.preferences.PreferenceConstants;

public class cgBuilder extends IncrementalProjectBuilder {

	/**
	 * @param f
	 */
	protected void parseGLSL(IFile f) {
		String compiler = CgToolsPlugin.getDefault().getPluginPreferences()
				.getString(PreferenceConstants.GLSLPATH)
				+ " " + f.getName();
		Runtime run = Runtime.getRuntime();
		try {
			String path = f.getLocation().toOSString().substring(
					0,
					f.getLocation().toOSString().length()
							- f.getName().length());
			Process p = run.exec(compiler, null, new File(path));
			BufferedReader d = new BufferedReader(new InputStreamReader(p
					.getInputStream()));
			String str = d.readLine();
			while (str != null) {
				System.out.println(str);
				addMarkerGLSL(str, f);
				str = d.readLine();
			}
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	/**
	 * @param str
	 * @param fileName
	 */
	public void addMarkerGLSL(String str, IFile f) {
		String sub1 = str;
		boolean error = false;

		if (str.startsWith("ERROR")) {
			sub1 = str.substring("ERROR: 0:".length());
			error = true;
		} else if (str.startsWith("WARNING")) {
			sub1 = str.substring("WARNING: 0:".length());
		} else
			return;

		// Normaly, rest the (line): error code: error text
		int index = sub1.indexOf(':');
		String lineNumber = sub1.substring(0, index);
		int line = Integer.parseInt(lineNumber);
		String sub2 = sub1.substring(index + 1);

		if (error) {
			addMarker(f, sub2, line, IMarker.SEVERITY_ERROR);
		} else
			addMarker(f, sub2, line, IMarker.SEVERITY_WARNING);
	}

	/**
	 *
	 * @param f
	 */
	public void parseCgFp(IFile f) {
		String compiler = CgToolsPlugin.getDefault().getPluginPreferences()
				.getString(PreferenceConstants.CGPATH);
		String str = CgToolsPlugin.getDefault().getPluginPreferences()
				.getString(PreferenceConstants.CGPROFILE);
		String profile = "-profile ";
		if (str.equals("arb")) {
			profile += "arbfp1";
		}

		if (str.equals("p40")) {
			profile += "fp40";
		}

		if (str.equals("p30")) {
			profile += "fp30";
		}

		if (str.equals("p20")) {
			profile += "fp20";
		}

		compiler += File.separator + "cgc -nocode -quiet -strict " + profile
				+ " " + f.getName();
		parseCg(f, compiler);
	}

	/**
	 *
	 * @param f
	 */
	public void parseCgVp(IFile f) {
		String compiler = CgToolsPlugin.getDefault().getPluginPreferences()
				.getString(PreferenceConstants.CGPATH);
		String str = CgToolsPlugin.getDefault().getPluginPreferences()
				.getString(PreferenceConstants.CGPROFILE);
		String profile = "-profile ";
		if (str.equals("arb")) {
			profile += "arbvp1";
		}

		if (str.equals("p40")) {
			profile += "vp40";
		}

		if (str.equals("p30")) {
			profile += "vp30";
		}

		if (str.equals("p20")) {
			profile += "vp20";
		}

		compiler += File.separator + "cgc -nocode -quiet -strict " + profile
				+ " " + f.getName();
		parseCg(f, compiler);
	}

	/**
	 *
	 * @param f
	 */
	public void parseFx(IFile f) {
		String compiler = CgToolsPlugin.getDefault().getPluginPreferences()
				.getString(PreferenceConstants.CGPATH);
		try {
			BufferedReader in = new BufferedReader(new FileReader(f
					.getLocation().toOSString()));
			String str;
			String technique = "none";
			while ((str = in.readLine()) != null) {

				if (str.contains("technique")) {
					technique = str;
				}

				if (str.contains("compile")) {
					int index = str.indexOf("compile ");
					str = str.substring(index + "compile ".length(), str
							.length());
					index = str.indexOf(" ");
					String profile = "-profile " + str.substring(0, index);
					int index2 = str.indexOf("(");
					str = "-entry " + str.substring(index + 1, index2);
					compiler += File.separator + "cgc -nocode -quiet -strict "
							+ profile + " " + str + " " + f.getName();
					parseFx(f, compiler, technique);
				}
			}
			in.close();
		} catch (IOException e) {
		}

		//
		// String str =
		// CgToolsPlugin.getDefault().getPluginPreferences().getString(PreferenceConstants.CGPROFILE);
		// String profile = "-profile ";
		// if(str.equals("arb"))
		// {
		// profile += "arbvp1";
		// }
		//
		// if(str.equals("p40"))
		// {
		// profile += "vp40";
		// }
		//
		// if(str.equals("p30"))
		// {
		// profile += "vp30";
		// }
		//
		// if(str.equals("p20"))
		// {
		// profile += "vp20";
		// }
		//
		// compiler+=
		// File.separator+"cgc -nocode -quiet -strict "+profile+" "+f.getName();
		// parseCg(f, compiler);
	}

	/**
	 * @param f
	 */
	public void parseCg(IFile f, String compiler) {

		Runtime run = Runtime.getRuntime();
		try {
			String path = f.getLocation().toOSString().substring(
					0,
					f.getLocation().toOSString().length()
							- f.getName().length());
			Process p = run.exec(compiler, null, new File(path));
			BufferedReader d = new BufferedReader(new InputStreamReader(p
					.getInputStream()));
			String str = d.readLine();
			while (str != null) {
				addMarkerCg(str, f);
				str = d.readLine();
			}
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	/**
	 * @param f
	 */
	public void parseFx(IFile f, String compiler, String technique) {

		Runtime run = Runtime.getRuntime();
		try {
			String path = f.getLocation().toOSString().substring(
					0,
					f.getLocation().toOSString().length()
							- f.getName().length());
			Process p = run.exec(compiler, null, new File(path));
			BufferedReader d = new BufferedReader(new InputStreamReader(p
					.getInputStream()));
			String str = d.readLine();
			while (str != null) {
				addMarkerFx(technique, str, f);
				str = d.readLine();
			}
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	/**
	 * @param str
	 * @param fileName
	 */
	public void addMarkerCg(String str, IFile f) {
		String sub1 = str;

		if (str.startsWith(f.getName())) {
			sub1 = str.substring(f.getName().length());
		}
		// Normaly, rest the (line): error code: error text
		int index = sub1.indexOf(')');
		String lineNumber = sub1.substring(1, index);
		int line = Integer.parseInt(lineNumber);
		String sub2 = sub1.substring(index + 4);

		if (sub2.startsWith("error")) {
			addMarker(f, sub2, line, IMarker.SEVERITY_ERROR);
		} else
			addMarker(f, sub2, line, IMarker.SEVERITY_WARNING);
	}

	/**
	 * @param str
	 * @param fileName
	 */
	public void addMarkerFx(String technique, String str, IFile f) {
		String sub1 = str;

		if (str.startsWith(f.getName())) {
			sub1 = str.substring(f.getName().length());
		}
		// Normaly, rest the (line): error code: error text
		int index = sub1.indexOf(')');
		String lineNumber = sub1.substring(1, index);
		int line = Integer.parseInt(lineNumber);
		String sub2 = sub1.substring(index + 4);

		if (sub2.startsWith("error")) {
			addMarker(f, "[" + technique + "] " + sub2, line,
					IMarker.SEVERITY_ERROR);
		} else
			addMarker(f, "[" + technique + "] " + sub2, line,
					IMarker.SEVERITY_WARNING);
	}

	class cgDeltaVisitor implements IResourceDeltaVisitor {
		/*
		 * (non-Javadoc)
		 *
		 * @see
		 * org.eclipse.core.resources.IResourceDeltaVisitor#visit(org.eclipse
		 * .core.resources.IResourceDelta)
		 */
		public boolean visit(IResourceDelta delta) throws CoreException {
			IResource resource = delta.getResource();
			switch (delta.getKind()) {
			case IResourceDelta.ADDED:
				// handle added resource
				checkCg(resource);
				break;
			case IResourceDelta.REMOVED:
				// handle removed resource
				break;
			case IResourceDelta.CHANGED:
				// handle changed resource
				checkCg(resource);
				break;
			}
			// return true to continue visiting children.
			return true;
		}
	}

	class cgResourceVisitor implements IResourceVisitor {
		public boolean visit(IResource resource) {
			checkCg(resource);
			// return true to continue visiting children.
			return true;
		}
	}

	public static final String BUILDER_ID = "fr.trevidos.GLShaderTools.cgTools.cgBuilder";

	private static final String MARKER_TYPE = "fr.trevidos.GLShaderTools.cgTools.cgProblem";

	private void addMarker(IFile file, String message, int lineNumber,
			int severity) {
		try {
			IMarker marker = file.createMarker(MARKER_TYPE);
			marker.setAttribute(IMarker.MESSAGE, message);
			marker.setAttribute(IMarker.SEVERITY, severity);
			if (lineNumber == -1) {
				lineNumber = 1;
			}
			marker.setAttribute(IMarker.LINE_NUMBER, lineNumber);
		} catch (CoreException e) {
		}
	}

	/*
	 * (non-Javadoc)
	 *
	 * @see org.eclipse.core.internal.events.InternalBuilder#build(int,
	 * java.util.Map, org.eclipse.core.runtime.IProgressMonitor)
	 */
	@Override
	protected IProject[] build(int kind, Map args, IProgressMonitor monitor)
			throws CoreException {
		if (kind == FULL_BUILD) {
			fullBuild(monitor);
		} else {
			IResourceDelta delta = getDelta(getProject());
			if (delta == null) {
				fullBuild(monitor);
			} else {
				incrementalBuild(delta, monitor);
			}
		}
		return null;
	}

	/**
	 *
	 * @param resource
	 */
	void checkCg(IResource resource) {
		if (resource instanceof IFile && resource.getName().endsWith(".fp")) {
			IFile file = (IFile) resource;
			deleteMarkers(file);
			parseCgFp(file);
		}

		if (resource instanceof IFile && resource.getName().endsWith(".vp")) {
			IFile file = (IFile) resource;
			deleteMarkers(file);
			parseCgVp(file);
		}

		if (resource instanceof IFile && resource.getName().endsWith(".fx")) {
			IFile file = (IFile) resource;
			deleteMarkers(file);
			parseFx(file);
		}

		if (resource instanceof IFile && resource.getName().endsWith(".glsl")) {
			IFile file = (IFile) resource;
			deleteMarkers(file);
			parseGLSL(file);
		}
	}

	private void deleteMarkers(IFile file) {
		try {
			file.deleteMarkers(MARKER_TYPE, false, IResource.DEPTH_ZERO);
		} catch (CoreException ce) {
		}
	}

	protected void fullBuild(final IProgressMonitor monitor)
			throws CoreException {
		try {
			getProject().accept(new cgResourceVisitor());
		} catch (CoreException e) {
		}
	}

	protected void incrementalBuild(IResourceDelta delta,
			IProgressMonitor monitor) throws CoreException {
		// the visitor does the work.
		delta.accept(new cgDeltaVisitor());
	}
}
