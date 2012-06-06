package net.sourceforge.ufoai.util;

import java.util.Collection;

public class StringUtil {
	public static boolean isNull(String str) {
		return str == null || str.isEmpty();
	}

	public static boolean isValid(String str) {
		return !isNull(str);
	}

	public static String join(String separator, Collection<String> collection) {
		StringBuilder builder = new StringBuilder();
		for (String element: collection) {
			if (builder.length() != 0) {
				builder.append(separator);
			}
			builder.append(element);
		}
		return builder.toString();
	}
}
