package net.sourceforge.ufoai.ufoscripteditor.parser.validators;

public class UFOScriptValidateException extends Exception {
	private static final long serialVersionUID = 1L;
	private final int line;

	public UFOScriptValidateException(String msg, int line) {
		super(msg);
		this.line = line;
	}

	public int getLine() {
		return line;
	}
}
