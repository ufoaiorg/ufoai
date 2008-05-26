package net.ufoai.sf.maputils;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.jar.JarFile;

/*

copyright 2008 UFO:Alien Invasion team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

http://www.gnu.org/licenses/gpl.html

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/**
 *
 * @author andy buckle
 */
public class MapUtils {

	private static Pattern naturalNumberEndPattern = Pattern.compile ("\\d*$");
	private static boolean verbose = true, fix = false;
	private static PrintStream out = System.out;
	private static final String jarfilename = "maputils.jar";
	private static final String svn_info_filename = "svn_info.txt";
	private static String svnInfoText = null;
	private static Pattern urlLinePattern = Pattern.compile ("^Repository Root.*?$", Pattern.MULTILINE);
	private static Pattern revisionLinePattern = Pattern.compile ("^Revision.*?$", Pattern.MULTILINE);
	private static final String subDir = "branches/maputils_java/";

	public static void main (String[] args) {
		try {
			//parse svn info from jar
			try {
				JarFile jar = new JarFile (jarfilename);
				InputStream jarIn = jar.getInputStream (jar.getEntry (svn_info_filename) );
				InputStreamReader svnInfoReader = new InputStreamReader (jarIn);
				svnInfoText = load (svnInfoReader);
				jarIn.close();
				//System.out.println(""+svnInfoText);
				Matcher urlMatcher = urlLinePattern.matcher (svnInfoText);
				urlMatcher.find();
				out.println (urlMatcher.group() );
				out.println ("Subdirectory: " + subDir);
				Matcher revMatcher = revisionLinePattern.matcher (svnInfoText);
				revMatcher.find();
				out.println (revMatcher.group() );
			} catch (Exception e) {
				out.printf ("***%s:%s%n***%s%n", e.getClass(), e.getMessage(), "while reading svn info from jar: svn_info.txt may be missing from jar.");
			}
			boolean[] argUseds=new boolean[args.length];
			for(int i=0;i<argUseds.length;i++) argUseds[i]=false;
			for (int i = 0;i < args.length;i++) {//parse mode-setting args first
				if (args[i].equalsIgnoreCase ("-fix") ) {
				    argUseds[i]=true;
				    fix = true;
				}
				if (args[i].equalsIgnoreCase ("-quiet") ) {
				    argUseds[i]=true;
				    verbose = false;
				}
				if (args[i].equalsIgnoreCase ("-h") || args[i].equalsIgnoreCase ("-help") ) {
					usage();
					System.exit (0);
				}
			}
			if (args.length == 0) noMapSpecified();
			String mapfilename = args[args.length-1];
			argUseds[args.length-1]=true;
			String extension = mapfilename.substring (mapfilename.lastIndexOf (".") );
			if (!extension.equalsIgnoreCase (".map") ) noMapSpecified();
			File mapfile = new File (mapfilename);
			if (!mapfile.exists() ) {
				out.println ("could not find map: does not exist? " + mapfilename);
				noMapSpecified();
			}
			Map map = new Map (mapfilename);
			for (int i = 0;i < args.length;i++) {//take actions
				//levelflags must be done first or BrushList.go() will not work
				if (args[i].equalsIgnoreCase ("-lvlflags") ) {
					argUseds[i]=true;
					map.calculateLevelFlags();
				}
				if (args[i].equalsIgnoreCase ("-broken") ) {
					argUseds[i]=true;
					map.findBrokenBrushes();
				}
				if (args[i].equalsIgnoreCase ("-nodraws") ) {
					BrushList.go (map);
					map.findUnexposedFaces();
				}
				if(args[i].equalsIgnoreCase("-vinfo")){
					argUseds[i]=true;
					MapUtils.printf(map.verboseInfo());
				}
				if(args[i].equalsIgnoreCase("-intersect")){
					BrushList.go (map);
					argUseds[i]=true;
					map.findIntersectingBrushes();
				}
			}
			for(int i=0;i<argUseds.length;i++){
			    if(!argUseds[i]){
				System.out.printf("*** arg %s not understood%n",args[i]);
			    }
			}
			if (fix) {//save orig with different name and save changes
				String safetySaveFilename = map.getFilenameWith (".original_0");
				while ( (new File (safetySaveFilename) ).exists() ) {
					Matcher numberMatcher = naturalNumberEndPattern.matcher (safetySaveFilename);
					if (numberMatcher.find() ) {
						String newNumber = "" + (Integer.parseInt (numberMatcher.group() ) + 1);
						safetySaveFilename = numberMatcher.replaceFirst (newNumber);
					}
				}
				TimeCounter.report ("saving original:" + safetySaveFilename);
				Map.save (safetySaveFilename, map.getOriginalText() );

				String originalFilename = map.getOriginalFilename();
				TimeCounter.report (" saving changed:" + originalFilename);
				map.saveChanged (originalFilename);
			}
		} catch (Exception ex) {
			//usage();
			ex.printStackTrace();
		}
	}

	private static String load (InputStreamReader reader) throws IOException {
		String txt = "";
		final int bs = 131072;
		int numRead = 0;
		char[] buff = new char[bs];
		while ( (numRead = reader.read (buff, 0, bs) ) != -1 ) txt += (new String (buff) ).substring (0, numRead);
		return txt;
	}

	public static void printf (String format, Object... args) {
		if (verbose) {
			out.printf (format, args);
		}
	}

	public static void noMapSpecified() {
		out.println ("*** last arg must be a .map file ***");
		usage();
		System.exit (0);
	}

	public static void usage() {
		out.println ("usage: [options] <map>");
		out.println ("  map       A UFO:AI .map file");
		out.println ("options");
		out.println ("  -help, -h  Print (this) advice and exit.");
		out.println ("  -quiet     Do not write notes to standard out. Default is verbose.");
		out.println ("  -nodraws   Set nodraw flags and textures to faces which are always");
		out.println ("             hidden by being abbutted to another face");
		out.println ("  -fix       Make changes to the map. save overwrites foo.map");
		out.println ("             also writes safety foo.map.original_0");
		out.println ("             if foo.map.original_0 exists then saves foo.map.original_1, etc");
		out.println ("  -vinfo     Print verbose info about each brush");
		out.println ("  -lvlflags  Calculate levelflags based on brush vertex coordinates.");
		out.println ("  -intersect Find intersects between opaque immutable brushes.");
		out.println();
		out.println ("will probably need to set extra memory. to allow java around 1 Gig.");
		out.println ("java -Xmx1000M -jar maputils.jar [options] <map>");
		out.println();

	}
}
