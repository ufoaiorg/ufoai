package net.sourceforge.ufoai.ufoscripteditor.editors;

import org.eclipse.jface.text.rules.IWhitespaceDetector;

public class UFOWhitespaceDetector implements IWhitespaceDetector {
	public boolean isWhitespace(char c) {
		return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	}
}
