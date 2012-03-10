package net.sourceforge.ufoai.ufoscripteditor.editors;

import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.IRule;
import org.eclipse.jface.text.rules.IToken;
import org.eclipse.jface.text.rules.NumberRule;
import org.eclipse.jface.text.rules.RuleBasedScanner;
import org.eclipse.jface.text.rules.SingleLineRule;
import org.eclipse.jface.text.rules.Token;
import org.eclipse.jface.text.rules.WhitespaceRule;
import org.eclipse.jface.text.rules.WordPatternRule;
import org.eclipse.jface.text.rules.WordRule;

public class UFOScanner extends RuleBasedScanner {

	public UFOScanner(ColorManager manager) {
		IToken value = new Token(new TextAttribute(manager
				.getColor(IUFOColorConstants.VALUE)));
		IToken var = new Token(new TextAttribute(manager
				.getColor(IUFOColorConstants.VAR)));
		IToken other = new Token(new TextAttribute(manager
				.getColor(IUFOColorConstants.DEFAULT)));

		WordRule reservedValues = new WordRule(new UFOWordDetector(),
				Token.UNDEFINED);
		reservedValues.addWord("true", value);
		reservedValues.addWord("false", value);

		IRule[] rules = new IRule[10];
		// Add rule for processing instructions
		rules[0] = new SingleLineRule("\"", "\"", value, '\\', true);
		rules[1] = new NumberRule(value);
		rules[2] = reservedValues;
		rules[3] = new WordRule(new UFOTextualConstantDetector(), value);
		rules[4] = new WordPatternRule(new UFOWordDetector(), "*cvar:", "", var);
		rules[5] = new WordPatternRule(new UFOPathDetector(), "*node:", "", var);
		rules[6] = new WordPatternRule(new UFOPathDetector(), "*", "", var);
		rules[7] = new WordPatternRule(new UFOWordDetector(), "<", ">", var);
		rules[8] = new WordRule(new UFOWordDetector(), other);

		// Add generic whitespace rule.
		rules[9] = new WhitespaceRule(new UFOWhitespaceDetector());

		setRules(rules);
	}
}
