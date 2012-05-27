package net.sourceforge.ufoai.util;

public class StringUtil {
	public static boolean isNull(String str) {
		return str == null || str.isEmpty();
	}

	public static boolean isValid(String str) {
		return !isNull(str);
	}
}
