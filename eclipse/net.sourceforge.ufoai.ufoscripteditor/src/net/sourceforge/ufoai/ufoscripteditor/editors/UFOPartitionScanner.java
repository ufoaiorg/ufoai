package net.sourceforge.ufoai.ufoscripteditor.editors;

import org.eclipse.jface.text.rules.*;

public class UFOPartitionScanner extends RuleBasedPartitionScanner {
	public final static String UFO_COMMENT = "__ufo_comment";
	public final static String UFO_SCRIPT = "__ufo_script";

	public UFOPartitionScanner() {
		IToken comment = new Token(UFO_COMMENT);

		IPredicateRule[] rules = new IPredicateRule[3];

		rules[0] = new MultiLineRule("/*", "*/", comment);
		rules[1] = new EndOfLineRule("//", comment);
		rules[2] = new SingleLineRule("\"", "\"", Token.UNDEFINED, '\\', true);

		setPredicateRules(rules);
	}
}
