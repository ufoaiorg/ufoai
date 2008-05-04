package net.ufoai.sf.maputils;

import java.io.File;
import java.io.PrintStream;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/*
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
    
    private static Pattern naturalNumberEndPattern=Pattern.compile("\\d*$");
    private static boolean verbose=true, fix=false;
    private static PrintStream out=System.out;
    
    public static void main(String[] args) {
        try {
            for(int i=0;i<args.length;i++){//parse mode-seeting args first
                if(args[i].equalsIgnoreCase("-fix")) fix=true;
                if(args[i].equalsIgnoreCase("-quiet")) verbose=false;
                if(args[i].equalsIgnoreCase("-h") || args[i].equalsIgnoreCase("-help")) {
                    usage();
                    System.exit(0);
                }
            }
            if(args.length==0) noMapSpecified();
            String mapfilename=args[args.length-1];
            String extension=mapfilename.substring(mapfilename.lastIndexOf("."));
            if(!extension.equalsIgnoreCase(".map")) noMapSpecified();
            File mapfile=new File(mapfilename);
            if(!mapfile.exists()){
                out.println("could not find map: does not exist? "+mapfilename);
                noMapSpecified();
            }
            Map map=new Map(mapfilename);
            for(int i=0;i<args.length;i++){//take actions
                //levelflags must be done first or BrushList.go() will not work
                if(args[i].equalsIgnoreCase("-lvlflags")){
                    map.calculateLevelFlags();
                }
                if(args[i].equalsIgnoreCase("-broken")){
                    map.findBrokenBrushes();
                }
                if(args[i].equalsIgnoreCase("-nodraws")){
                    BrushList.go(map);
                    map.findUnexposedFaces();
                }
            }
            if(fix) {//save orig with different name and save changes
                String safetySaveFilename=map.getFilenameWith(".original_0");
                while( (new File(safetySaveFilename)).exists() ){
                    Matcher numberMatcher=naturalNumberEndPattern.matcher(safetySaveFilename);
                    if(numberMatcher.find()){
                        String newNumber=""+(Integer.parseInt(numberMatcher.group())+1);
                        safetySaveFilename=numberMatcher.replaceFirst(newNumber);
                    }
                }
                TimeCounter.report("saving original:"+safetySaveFilename);
                Map.save(safetySaveFilename, map.getOriginalText());

                String originalFilename=map.getOriginalFilename();
                TimeCounter.report(" saving changed:"+originalFilename);
                map.saveChanged(originalFilename);
            }
        } catch (Exception ex) {
            //usage();
            ex.printStackTrace();
        }
    }
    
    public static void printf(String format, Object... args){
        if(verbose){
            out.printf(format, args);
        }
    }
    
    public static void noMapSpecified(){
        out.println("*** last arg must be a .map file ***");
        usage();
        System.exit(0);
    }
    
    public static void usage(){
        out.println("usage: [options] <map>");
        out.println("  map       A UFO:AI .map file");
        out.println("options");
        out.println("  -help, -h Print (this) advice and exit.");
        out.println("  -quiet    Do not write notes to standard out. Default is verbose.");
        out.println("  -fix      Make changes to the map. save overwrites foo.map");
        out.println("            also writes safety foo.map.original_0");
        out.println("            if foo.map.original_0 exists then saves foo.map.original_1, etc");
        out.println("  -lvlflags Calculate levelflags based on brush vertex coordinates.");
    }
}
