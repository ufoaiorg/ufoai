package net.sourceforge.ufoai.ufoscripteditor.editors;

import org.eclipse.jface.text.rules.IWordDetector;

public class UFOWordDetector implements IWordDetector {
	@Override
	public boolean isWordPart(char c) {
		return (Character.isLetterOrDigit(c) || c == '_' || c == '-'
				|| c == '$' || c == '#' || c == '@' || c == '~' || c == '<'
				|| c == ':' || c == '>' || c == '*' || c == '.' || c == '?');
	}

	@Override
	public boolean isWordStart(char c) {
		return (Character.isLetter(c) || c == '_');
	}
}
