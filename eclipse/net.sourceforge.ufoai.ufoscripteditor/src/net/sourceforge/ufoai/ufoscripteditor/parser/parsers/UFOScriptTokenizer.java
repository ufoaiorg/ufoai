package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import java.util.StringTokenizer;

public class UFOScriptTokenizer extends StringTokenizer {
	private int line;

	public UFOScriptTokenizer(final String str) {
		super(str, " \t\r\n\f", true);
	}

	/**
	 * Will return the next ufo script token. Strings that are enclosed by
	 * quotes will be returned as one token. Whitespaces are returned as tokens,
	 * too.
	 */
	@Override
	public String nextToken() {
		String token = super.nextToken();
		if (token.startsWith("\"") && !token.endsWith("\"")) {
			final StringBuilder b = new StringBuilder();
			b.append(token);
			while (super.hasMoreTokens()) {
				token = super.nextToken();
				if (token.endsWith("\"")) {
					b.append(token);
					return b.toString();
				} else if (token.equals("\n"))
					line++;
				b.append(token);
			}
			return b.toString();
		}
		if (token.equals("\n"))
			line++;
		return token;
	}

	/**
	 * @return The current line in the parsed string.
	 */
	public int getLine() {
		return line;
	}

	/**
	 * Skips a whole block of { } enclosed data.
	 */
	public void skipBlock() {
		int depth = 0;
		while (super.hasMoreTokens()) {
			final String token = nextToken();
			if (token.equals("{")) {
				depth++;
			}
			if (token.equals("}")) {
				if (depth <= 0)
					return;
				depth--;
			}
		}
	}

	public boolean isSeperator(String token) {
		return token.equals(" ") || token.equals("\t") || token.equals("\n")
				|| token.equals("\f") || token.equals("\r");
	}

	public String getBlock() {
		int depth = 0;
		StringBuilder b = new StringBuilder();
		while (super.hasMoreTokens()) {
			final String token = nextToken();
			if (token.equals("{")) {
				depth++;
			}
			if (token.equals("}")) {
				if (depth <= 0)
					return b.toString();
				depth--;
			}
			b.append(token);
		}
		return "";
	}
}
