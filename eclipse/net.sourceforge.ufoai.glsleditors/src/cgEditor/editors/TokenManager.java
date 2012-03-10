package cgEditor.editors;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.Token;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.widgets.Display;

public class TokenManager {
	protected static Map<String, Token> tokenTable = new HashMap<String, Token>(
			5);
	protected static Map<RGB, Color> colorTable = new HashMap<RGB, Color>(5);

	/**
	 * @param rgb
	 * @return
	 */
	static public Color getColor(RGB rgb) {
		Color color = colorTable.get(rgb);
		if (color == null) {
			color = new Color(Display.getCurrent(), rgb);
			colorTable.put(rgb, color);
		}
		return color;
	}

	/**
	 * @param str
	 * @return
	 */
	static public Token getToken(String str) {
		Token token = tokenTable.get(str);
		if (token == null) {
			token = new Token(new TextAttribute(null));
			tokenTable.put(str, token);
		}
		return token;
	}
}
