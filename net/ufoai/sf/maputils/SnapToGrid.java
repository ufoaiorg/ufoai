/*
 * SnapToGrid.java
 *
 * Created on 06 April 2008, 13:22
 *
 */

package net.ufoai.sf.maputils;

import java.io.FileNotFoundException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 *
 *
 */
public class SnapToGrid {

	Map map;

	//space, then at least one digit, minus or dot (but not a bracket or space), finishing before the next space
	private static Pattern numBoundedBySpacePatt = Pattern.compile ("[\\s][\\d\\-\\.[^\\s\\)\\(]]+");
	private static Pattern bracketBitPatt = Pattern.compile ("\\(.*\\)");//greedy ( anything )

	public SnapToGrid (Map mapIn, int grid) throws FileNotFoundException {
		map = mapIn;
		System.out.println ("Snap to Grid, spacing " + grid);
		double dGrid = (double) grid;
		System.out.println ("map file: " + mapIn.getOriginalFilename() );
		System.out.println ("SnapToGrid is broken");
	}

}

class Replacement {

	public int s, e;
	public String n;

	public Replacement (int start, int end, String replacement) {
		s = start;
		e = end;
		n = "" + replacement;
	}
}
