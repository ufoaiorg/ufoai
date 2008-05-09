package net.ufoai.sf.maputils;

import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintStream;
import java.text.ParseException;
import java.util.Vector;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Map.java
 * Created on 06 April 2008, 13:23
 * @author andy buckle
 */
public class Map {



	/** Regular expression to remove C and C++ style comments,
	  * tested, works on multiline C style comments. */
	private static Pattern commentPattern = Pattern.compile ("//.*?$|/\\*.*?\\*/", Pattern.MULTILINE | Pattern.DOTALL);

	String origFilename, origTxt;

	Vector<Integer> commStart = new Vector<Integer> (10, 10), commEnd = new Vector<Integer> (10, 10);

	int iterationLineIndex = -1;

	final static char openBrace = "{".charAt (0), closeBrace = "}".charAt (0);

	Vector<Entity> entities = new Vector<Entity> (10, 10);

	/**  */
	public Map (String filename) throws FileNotFoundException, IOException, ParseException {
		TimeCounter.report ("loading");
		origTxt = load (filename);
		origFilename = "" + filename;
		//System.out.println(""+origTxt);
		TimeCounter.report ("finding comments");
		findComments();
		/*
		//test code
		String tmp="";
		for(int i=0;i<origTxt.length();i++){
		    tmp+= isInComment(i) ? "<"+origTxt.subSequence(i,i+1)+">" : origTxt.subSequence(i,i+1);
		}
		System.out.println(""+tmp);
		 */
		TimeCounter.report ("finding entities");
		findEntities();
		//printParsed();
		TimeCounter.report ("Map constructor finished");
	}

	public Vector<Entity> getEntities() {
		return entities;
	}

	/** returns true if the char is in a comment part of the file.
	 *  Slow for files with a lot of comments. */
	public boolean isInComment (int charIndex) {
		for (int i = 0;i < commStart.size();i++) {
			if (charIndex >= commStart.get (i) && charIndex < commEnd.get (i) ) return true;
		}
		return false;
	}

	private void findComments() {
		Matcher m = commentPattern.matcher (origTxt);
		while (m.find() ) {
			commStart.add (m.start() );
			commEnd.add (m.end() );
		}
	}

	private void findEntities() throws ParseException {
		int[] region = new int[] {0, -1};
		while ( (region = findBracedRegion (region[1] + 1) ) [0] != -1) {
			entities.add (new Entity (this, region[0], region[1]) );
		}
	}

	/** finds the next braced region in the map file. trims the defining braces
	 *  from the region.
	 *  @returns {startIndex, endIndex} or {-1, -1} if there are none. */
	int[] findBracedRegion (int startingAt) throws ParseException {
		try {
			int braceLevel = 0;
			int startRegion = 0;
			for (int i = startingAt;true;i++) {
				if (origTxt.charAt (i) == openBrace && !isInComment (i) ) {
					if (braceLevel == 0) {
						startRegion = i;
					}
					braceLevel++;
				}
				if (origTxt.charAt (i) == closeBrace && !isInComment (i) ) {
					braceLevel--;
					
					/* do not go outside the scope from which findBracedRegion() was called. */
					if (braceLevel < 0) return new int[] {-1,-1};
					
					/*brace is closed, brace region complete. */
					if (braceLevel == 0) {
						return new int[] {startRegion + 1, i};//trims defining braces
					}
				}
			}
		} catch (StringIndexOutOfBoundsException eSIOOB) {
			return new int[] { -1, -1};
		}
	}

	private static String load (String path) throws FileNotFoundException, IOException {
		String txt = "";
		FileReader fr = new FileReader (path);
		final int bs = 131072;
		int numRead = 0;
		char[] buff = new char[bs];
		while ( (numRead = fr.read (buff, 0, bs) ) != -1 ) txt += (new String (buff) ).substring (0, numRead);
		fr.close();
		return txt;
	}

	private static String removeComments (String txt) {
		return commentPattern.matcher (txt).replaceAll ("");
	}

	public void saveChanged (String newFilename) throws FileNotFoundException {
		PrintStream ps = new PrintStream (newFilename);
		printParsed (ps, "//");//put summary at the start
		for (Entity ent: entities) ent.writeReformedText (ps);
		ps.close();
	}

	public static void save (String filename, String text) throws FileNotFoundException {
		PrintStream ps = new PrintStream (filename);
		ps.println (text);
		ps.close();
	}

	/** returns a clone of the original filename */
	public String getOriginalFilename() {
		return "" + origFilename;
	}

	/** returns a pointer to the original text */
	public String getOriginalText() {
		return origTxt;
	}

	public String getFilenameWith (String suffix) {
		return origFilename + suffix;
	}

	public String toString (int start, int end) {
		return origTxt.substring (start, end);
	}

	public String toString() {
		return "" + origTxt;
	}

	public void printParsed (PrintStream to, String prefix) {
		to.printf ("%sEntities:%d%n", prefix, entities.size() );
		for (int i = 0;i < entities.size();i++) {
			Entity ent = entities.get (i);
			to.printf ("%s  %03d: %s%n", prefix, i, ent.toString() );
		}
	}

	public void printParsed() {
		printParsed (System.out, "");
	}

	public void calculateLevelFlags() {
		for (int i = 0;i < entities.size();i++) {
			entities.get (i).calculateLevelFlags();

		}
	}

	public void findBrokenBrushes() {
		TimeCounter.report ("searching for broken brushes");
		for (Entity ent: entities) {
			if (ent.hasBrushes() ) {
				Vector<Brush> brushes = ent.getBrushes();
				for (Brush b: brushes) {
					int vertices = b.countVertices();
					int faces = b.countFaces();
					if (vertices < 4 || vertices < faces) {

						String warning = String.format ("broken brush vertices:%d faces:%d at %s",
														vertices, faces, b.boundingBoxToString() );
						MapUtils.printf ("%s%n", warning);
						b.addComment (warning);
						String verts = "vertices ";
						Vector<Vector3D> vertexVectors = b.getVertices();
						for (Vector3D v: vertexVectors) verts += "" + v;
						MapUtils.printf ("%s%n", verts);
						b.addComment (verts);
						b.detailedInvestigation();
					}

				}
			}
		}
		TimeCounter.report ("finished searching for broken brushes");
	}

	public void findUnexposedFaces() {
		Vector<Brush> brushes = BrushList.getImmutableOpaqueBrushes();
		int nodrawSetCount = 0;
		for (Brush b: brushes) {//for each immutable brush, b
			Vector<Brush> possibleInteractionBrushes = b.getInteractionList();
			for (Brush pib: possibleInteractionBrushes) {//loop through the list of brushes which are near it
				//System.out.printf("Brush.findUnexposedFaces: testing if Brush %d interacts with %d%n",b.getBrushNumber(),pib.getBrushNumber() );
				Vector<Face> bFaces = b.getFaces();
				Vector<Face> piFaces = pib.getFaces();
				for (Face bf: bFaces) {
					if(!bf.isNodraw ()){//do not waste time testing if the face is already a nodraw.
						for (Face pif: piFaces) {
							//System.out.printf("Brush.findUnexposedFaces: testing if Face %d from Brush %d interacts with Face %d from Brush %d%n", bf.getNumber(),b.getBrushNumber(),pif.getNumber(),pib.getBrushNumber() );
							if (bf.isFacingAndCoincidentTo (pif) ) {
								//System.out.printf("Brush.findUnexposedFaces: passed facing and coincident planes test");
								Vector<Vector3D> vertsOfFaceBf = b.getVertices (bf);
								if (pib.areInside (vertsOfFaceBf) ) {
									//Entity parent = b.getParentEntity();
									//MapUtils.printf("set nodraw: face %d of brush %d of entity %d (%s)%n", bf.getNumber(),b.getBrushNumber(),parent.getNumber(),parent.getValue("classname"));
									nodrawSetCount++;
									bf.setNodraw();
								}
							}
						}
					}
				}
			}
		}
		TimeCounter.reportf ("finished unexposed face search. %d nodraws set", nodrawSetCount);
	}

}
