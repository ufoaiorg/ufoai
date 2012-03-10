package net.sourceforge.ufoai.ufoscripteditor.util;

import java.io.File;

public class FileNameUtil {
	public static String defaultExtension(final String fileName,
			final String extension) {
		if (fileName == null)
			throw new IllegalArgumentException("fileName is null");
		if (extension == null)
			throw new IllegalArgumentException("extension is null");

		if (isDirectory(fileName))
			return fileName;

		if (hasExtension(fileName))
			return fileName;

		final int extExists = fileName.lastIndexOf('.');
		if (extExists == -1)
			return fileName + "." + extension;

		return fileName;
	}

	public static String removeExtension(final String fileName) {
		if (fileName == null)
			throw new IllegalArgumentException("fileName is null");

		if (!hasExtension(fileName))
			return fileName;

		final int extExists = fileName.lastIndexOf('.');
		return fileName.substring(0, extExists);
	}

	public static String replaceExtension(final String fileName,
			final String extension) {
		if (fileName == null)
			throw new IllegalArgumentException("fileName is null");
		if (extension == null)
			throw new IllegalArgumentException("extension is null");

		return removeExtension(fileName) + "." + extension;
	}

	public static boolean isDirectory(final String path) {
		if (path == null)
			throw new IllegalArgumentException("path is null");

		if (path.endsWith("/") || path.endsWith("\\"))
			return true;
		return false;
	}

	public static boolean hasExtension(final String fileName) {
		if (fileName == null)
			throw new IllegalArgumentException("fileName is null");

		return getExtension(fileName) != null;
	}

	public static String getExtension(final String fileName) {
		if (fileName == null)
			throw new IllegalArgumentException("fileName is null");

		if (isDirectory(fileName))
			return null;

		final File file = new File(fileName);
		final String name = file.getName();
		final int extExists = name.lastIndexOf('.');
		if (extExists != -1)
			return name.substring(extExists + 1);

		return null;
	}

	public static String getFileName(String pathName) {
		return new File(pathName).getName();
	}
}
