package net.sourceforge.ufoai.ufoscripteditor.parser.util;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;

import org.eclipse.jface.text.BadLocationException;

public class ParserUtil {

	/**
	 * @param line
	 * @param msg
	 *            The message to show
	 * @throws BadLocationException
	 */
	public static void addProblem(final IParserContext ctx, final int line,
			final String msg) {
		// try {
		// marker.setAttribute(IMarker.LINE_NUMBER, lineNumber)
		// final int lineOffset = ctx.getDocument().getLineOffset(line);
		// MarkerUtilities.createMarker(resource, attributes, Marker-PROBLEM);
		// ctx.getAnnotations().addAnnotation(new MarkerAnnotation(marker),
		// new Position(lineOffset));
		// } catch (BadLocationException e) {
		// throw new RuntimeException(e);
		// }
	}
}
