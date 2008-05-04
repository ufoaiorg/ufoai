package net.ufoai.sf.maputils;

import java.io.PrintStream;
import java.text.ParseException;
import java.util.Vector;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Entity.java
 * Created on 09 April 2008, 21:12
 * @author andy buckle
 */
public class Entity {

	Map map;
	int s, e;
	Vector<KeyValuePair> keyValuePairs = new Vector<KeyValuePair> (10, 10);
	Vector<Brush> brushes = new Vector<Brush> (10, 100);
	int classtype;
	private static int entCount = 0;
	private int entNumber;

	static Pattern kvpPattern = Pattern.compile ("^[ \\t]*?\"(.*?)\"[ \\t]*?\"(.*?)\"[ \\t]*?$", Pattern.MULTILINE);
	static Pattern spawnPointPattern = Pattern.compile ("info_.+?_start");
	public static final int
	CLASS_WORLDSPAWN = 0,
					   CLASS_FUNC_GROUP = 1,
										  CLASS_FUNC_BREAKABLE = 2,
																 CLASS_MISC_MODEL = 3,
																					CLASS_MISC_PARTICLE = 4,
																										  CLASS_FUNC_DOOR = 5,
																															CLASS_FUNC_ROTATING = 6,
																																				  CLASS_TRIGGER_HURT = 7,
																																									   CLASS_INFO_ALIEN_START = 8,
																																																CLASS_INFO_CIVILIAN_START = 9,
																																																							CLASS_INFO_HUMAN_START = 10,
																																																													 CLASS_INFO_PLAYER_START = 11,
																																																																			   CLASS_UGV_ALIEN_START = 12,
																																																																									   CLASS_LIGHT = 13,
																																																																													 CLASS_MISC_SOUND = 14,
																																																																																		CLASS_MISC_MISSION = 15,
																																																																																							 CLASS_MISC_MISSION_ALIEN = 16;

	/**  */
	public Entity (Map from, int startIndex, int endIndex) throws ParseException {
		entNumber = (entCount++);
		Brush.resetBrushCount();
		map = from;
		s = startIndex;
		e = endIndex;
		Matcher kvpMatcher = kvpPattern.matcher (getOriginalText() );
		while (kvpMatcher.find() ) {
			//System.out.println("groups:"+kvpMatcher.groupCount());
			if (kvpMatcher.groupCount() == 2) {
				//System.out.println(""+i+">"+kvpMatcher.group(i)+"<");
				KeyValuePair kvp = new KeyValuePair (map, s + kvpMatcher.start (1), s + kvpMatcher.end (1), s + kvpMatcher.start (2), s + kvpMatcher.end (2) );
				keyValuePairs.add ( kvp );
			} else {
				String exMsg = "regex had trouble with line ";
				for (int i = 0;i <= kvpMatcher.groupCount();i++) exMsg += String.format ("%d>%s< ", i, kvpMatcher.group (i) );
				throw new ParseException (exMsg, 0);
			}
		}
		classtype = getClassType (getValue ("classname") );
		//System.out.println("");
		if (hasBrushes() ) {
			findBrushes();
		}
	}

	public int getNumber() {
		return entNumber;
	}

	public static int getClassType (String classname) {
		if (classname.equalsIgnoreCase ("WORLDSPAWN") ) return CLASS_WORLDSPAWN;
		if (classname.equalsIgnoreCase ("FUNC_GROUP") ) return CLASS_FUNC_GROUP;
		if (classname.equalsIgnoreCase ("FUNC_BREAKABLE") ) return CLASS_FUNC_BREAKABLE;
		if (classname.equalsIgnoreCase ("MISC_MODEL") ) return CLASS_MISC_MODEL;
		if (classname.equalsIgnoreCase ("MISC_PARTICLE") ) return CLASS_MISC_PARTICLE;
		if (classname.equalsIgnoreCase ("FUNC_DOOR") ) return CLASS_FUNC_DOOR;
		if (classname.equalsIgnoreCase ("FUNC_ROTATING") ) return CLASS_FUNC_ROTATING;
		if (classname.equalsIgnoreCase ("TRIGGER_HURT") ) return CLASS_TRIGGER_HURT;
		if (classname.equalsIgnoreCase ("INFO_ALIEN_START") ) return CLASS_INFO_ALIEN_START;
		if (classname.equalsIgnoreCase ("INFO_CIVILIAN_START") ) return CLASS_INFO_CIVILIAN_START;
		if (classname.equalsIgnoreCase ("INFO_HUMAN_START") ) return CLASS_INFO_HUMAN_START;
		if (classname.equalsIgnoreCase ("INFO_PLAYER_START") ) return CLASS_INFO_PLAYER_START;
		if (classname.equalsIgnoreCase ("LIGHT") ) return CLASS_LIGHT;
		if (classname.equalsIgnoreCase ("MISC_SOUND") ) return CLASS_MISC_SOUND;
		if (classname.equalsIgnoreCase ("MISC_MISSION") ) return CLASS_MISC_MISSION;
		if (classname.equalsIgnoreCase ("MISC_MISSION_ALIEN") ) return CLASS_MISC_MISSION_ALIEN;
		throw new RuntimeException ("classname not recognised");
	}

	public void findBrushes() throws ParseException {
		int[] region = new int[] {0, s - 1};
		while ( (region = map.findBracedRegion (region[1] + 1) ) [0] != -1) {
			brushes.add (new Brush (map, this, region[0], region[1]) );
		}
	}

	public int getClassType() {
		return classtype;
	}

	public boolean hasBrushes() {
		switch (classtype) {
		case CLASS_WORLDSPAWN:
		case CLASS_FUNC_GROUP:
		case CLASS_FUNC_BREAKABLE:
		case CLASS_FUNC_ROTATING:
		case CLASS_FUNC_DOOR:
			return true;
		case CLASS_MISC_MODEL:
		case CLASS_MISC_PARTICLE:
		case CLASS_TRIGGER_HURT:
		case CLASS_INFO_ALIEN_START:
		case CLASS_INFO_CIVILIAN_START:
		case CLASS_INFO_HUMAN_START:
		case CLASS_INFO_PLAYER_START:
		case CLASS_UGV_ALIEN_START:
		case CLASS_LIGHT:
		case CLASS_MISC_SOUND:
		case CLASS_MISC_MISSION:
		case CLASS_MISC_MISSION_ALIEN:
		default:
			return false;
		}
	}

	/** @return true for entities with brushes which do not move, change or
	 *               disappear. */
	public boolean hasImmutableBrushes() {
		switch (classtype) {
		case CLASS_WORLDSPAWN:
		case CLASS_FUNC_GROUP:
			return true;
		case CLASS_FUNC_BREAKABLE:
		case CLASS_FUNC_ROTATING:
		case CLASS_FUNC_DOOR:
		case CLASS_MISC_MODEL:
		case CLASS_MISC_PARTICLE:
		case CLASS_TRIGGER_HURT:
		case CLASS_INFO_ALIEN_START:
		case CLASS_INFO_CIVILIAN_START:
		case CLASS_INFO_HUMAN_START:
		case CLASS_INFO_PLAYER_START:
		case CLASS_UGV_ALIEN_START:
		case CLASS_LIGHT:
		case CLASS_MISC_SOUND:
		case CLASS_MISC_MISSION:
		case CLASS_MISC_MISSION_ALIEN:
		default:
			return false;
		}
	}

	/** @return the part of the text file upon which this object is based */
	public String getOriginalText() {
		return map.toString (s, e);
	}

	public void writeReformedText (PrintStream to) {
		to.printf ("// entity %d%n", getNumber() );
		to.println ("{");
		for (KeyValuePair kvp: keyValuePairs) kvp.writeReformedText (to);
		for (int i = 0;i < brushes.size();i++)  {
			to.printf ("// brush %d%n", i);
			brushes.get (i).writeReformedText (to);
		}
		to.println ("}");
	}

	public String toString() {
		String classname = getValue ("classname");
		if (spawnPointPattern.matcher (classname).matches() ) {
			return classname + "(" + getValue ("origin") + ")";
		}
		if (classname.equalsIgnoreCase ("misc_model") ) {
			return classname + "(" + getValue ("origin") + ") " + getValue ("model");
		}
		if (hasBrushes() ) {
			return classname + ": " + countBrushes() + " brushes";
		}
		return classname;
	}

	public String getValue (String key) {
		for (KeyValuePair kvp: keyValuePairs) {
			if (kvp.is (key) ) return kvp.get();
		}
		throw new RuntimeException ("key not found");
	}

	public int countKeyValuePairs() {
		return keyValuePairs.size();
	}

	public KeyValuePair getKeyValuePair (int index) {
		return keyValuePairs.get (index);
	}

	public int countBrushes() {
		return brushes.size();
	}

	public Vector<Brush> getBrushes() {
		return brushes;
	}

	public void calculateLevelFlags() {
		//@TODO check for model's levelflags and check func_breakable brushes
		//for(KeyValuePair kvp:keyValuePairs) kvp.writeReformedText(to);
		for (Brush b: brushes) b.setLevelFlagsBasedOnVertexCoordinates();
	}
}
