package net.ufoai.sf.maputils;

import java.io.PrintStream;

/**
 * KeyValuePair.java
 * Created on 10 April 2008, 20:35
 * @author andy buckle
 */
public class KeyValuePair {

	int ks, ke, vs, ve;
	Map map;

	/**  */
	public KeyValuePair (Map from, int keyStart, int keyEnd, int valueStart, int valueEnd) {
		ks = keyStart;
		ke = keyEnd;
		vs = valueStart;
		ve = valueEnd;
		map = from;
		//System.out.println("KeyValuePair:>"+getKey()+"<>"+getValue()+"<");
	}

	public boolean is (String key) {
		return key.equalsIgnoreCase (getKey() );
	}

	public String get() {
		return getValue();
	}

	public String getKey() {
		return map.toString (ks, ke);
	}

	public String getValue() {
		return map.toString (vs, ve);
	}

	public String toString() {
		return ">" + getKey() + "< >" + getValue() + "<";
	}

	public void writeReformedText (PrintStream to) {
		to.printf ("\"%s\" \"%s\"%n", getKey(), getValue() );

	}

}
