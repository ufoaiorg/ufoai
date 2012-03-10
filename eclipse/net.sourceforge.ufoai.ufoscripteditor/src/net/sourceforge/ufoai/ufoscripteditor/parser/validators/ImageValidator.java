package net.sourceforge.ufoai.ufoscripteditor.parser.validators;

import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptBlock;
import net.sourceforge.ufoai.ufoscripteditor.util.FileNameUtil;

public class ImageValidator implements IUFOScriptValidator {
	private final String extensions[] = new String[] { "png", "tga", "jpg" };

	@Override
	public void validate(final UFOScriptBlock block)
			throws UFOScriptValidateException {
		if (!block.getName().equalsIgnoreCase("image"))
			throw new IllegalStateException("Unknown block for image validator");

		final String relativeImagePath = block.getData();
		final String extension = FileNameUtil.getExtension(relativeImagePath);
		if (extension != null)
			throw new UFOScriptValidateException("Image has an extension",
					block.getStartLine());

		for (final String ext : extensions) {
			final String imageWithExtension = FileNameUtil.replaceExtension(
					relativeImagePath, ext);
		}
		// TODO: check path and all supported extensions
		System.out.println("Image: " + relativeImagePath);
	}

	@Override
	public String getID() {
		return "image";
	}
}
