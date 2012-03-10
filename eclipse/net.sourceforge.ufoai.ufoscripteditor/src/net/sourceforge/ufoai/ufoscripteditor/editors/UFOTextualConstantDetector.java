package net.sourceforge.ufoai.ufoscripteditor.editors;

import org.eclipse.jface.text.rules.IWordDetector;

public class UFOTextualConstantDetector implements IWordDetector {
	@Override
	public boolean isWordPart(char c) {
		return ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
	}

	@Override
	public boolean isWordStart(char c) {
		return (c >= 'A' && c <= 'Z');
	}
}
