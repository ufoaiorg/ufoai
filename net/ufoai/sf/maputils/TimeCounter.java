package net.ufoai.sf.maputils;

/**
 * TimeCounter.java
 * Created on 15 April 2008, 21:43
 * @author andy buckle
 */
public class TimeCounter {

	private static long timezero;
	private static boolean init = false;

	public static void report (String processStarting) {
		if (!init) {
			init = true;
			timezero = System.currentTimeMillis();
		}
		MapUtils.printf ("%dms %s%n", System.currentTimeMillis() - timezero, processStarting);
	}

	public static void reportf (String processStarting, Object... args) {
		report (String.format (processStarting, args) );
	}



}
