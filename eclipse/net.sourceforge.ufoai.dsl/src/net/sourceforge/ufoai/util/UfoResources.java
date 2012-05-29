package net.sourceforge.ufoai.util;

import java.io.File;

/**
 * UFOAI resource util to access to the content of the project.
 * QUESTION maybe we should use IResource (eclipse resource) instead of File
 */
public class UfoResources {

	private static String[] supportedImageExtensions = {".png", ".jpg"};
	private static String[] supportedModelExtensions = {".md2", ".md3", ".obj", ".mdl"};

	/**
	 * True if "base" dir already checked.
	 * This value can be true, and base null, if base not found
	 */
	private static boolean isBaseChecked;

	private static File base;

	/**
	 * Search for a parent file which contains "base" dir
	 * @param absolutePath
	 */
	public static void checkBase(String absolutePath) {
		File file = new File(absolutePath);
		for (int i = 0; i < 5; i++) {
			File dir = new File(file, "base");
			if (dir.exists() && dir.isDirectory()) {
				base = dir;
				isBaseChecked = true;
				return;
			}
			file = file.getParentFile();
			if (file == null) {
				break;
			}
		}
		System.out.println("UfoResources: Base dir was not found.");
		isBaseChecked = true;
	}

	public static boolean isBaseChecked() {
		return isBaseChecked;
	}

	private static File getFileFromBase(String id, String[] expectedExtensions) {
		if (base == null)
			return null;
		for (String extension: expectedExtensions) {
			File file = new File(base, id + extension);
			if (file.exists() || file.isFile()) {
				return file;
			}
		}
		return null;
	}

	/**
	 * Get a file from base/pics
	 * @param name Name of the file resource
	 * @return A file, else null if it do not exists
	 */
	public static File getFileFromPics(String id) {
		return getFileFromBase("pics/" + id, supportedImageExtensions);
	}

	/**
	 * Get a file from base/models
	 * @param name Name of the file resource
	 * @return A file, else null if it do not exists
	 */
	public static File getFileFromModels(String id) {
		return getFileFromBase("models/" + id, supportedModelExtensions);
	}

}
