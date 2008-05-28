package net.ufoai.sf.maputils;

import java.util.Vector;

/**
 * works out which brushes bounding boxes touch or intersect. Brushes may be in
 * worldspawn, func_group. brushes in func_breakable, rotating, ... entities are
 * not included, as they do not lend themselves to optimisations.
 * transparent brushes are also not included.
 *
 * Also adds a list of brushes which might interact with a brush. this is stored
 * in the brush. the list is designed for optimisations. brushes with different levelflags will
 * not be included.
 * (they can disappear, move, ...).
 * Created on 20 April 2008, 07:25
 * @author andy buckle
 */
public class BrushList {

	/** a list of immutable (not breakable or mobile) brushes from
	 *  various entities. */
	static Vector<Brush> brushes = new Vector<Brush> (100, 100);
	static Vector<CompositeFace> compositeFaces=new Vector<CompositeFace>(20,20);
	static Vector<Face> facesInComposites=new Vector<Face>(20,20);
	
	/**  */
	public static void go (Map map) {
		if (brushes.size() != 0) return;//already been done
		//System.out.println("BrushList:go");
		
		//get a reference to each Brush in; worldspawn, func_group, ...
		TimeCounter.report ("started making Brush potential interaction list.");
		Vector<Entity> entities = map.getEntities();
		for (Entity ent: entities) {
			if (ent.hasImmutableBrushes() ) {
				Vector<Brush> entityBrushes = ent.getBrushes();
				for (Brush b: entityBrushes) {
					if (!b.hasTransparentFaces()  && !b.isActorclipWeaponClipOrStepon() ) brushes.add (b);
					//System.out.println("adding a brush");
				}
			}
		}
		
		//for each brush, make a list of brushes that might touch it, and store it in the brush
		for (int i = 0;i < brushes.size();i++) {
			Brush bi = brushes.get (i); 
			//System.out.printf("brush %d has box %s%n",i,bi.boundingBoxToString());
			for (int j = 0;j < brushes.size();j++) {//j needs a ref to i and i needs a ref to j
				Brush bj = brushes.get (j);
				if (i != j && bi.boundingBoxIntersects (bj) ) {
					bi.addToBrushInteractionList (bj);
				}
			}
			//System.out.printf("BrushList.go: brush %d has %d brushes in its' interaction list%n",i,bi.interactionList.size());
		}
		
		TimeCounter.reportf ("%d immutable brushes found",brushes.size ());
		TimeCounter.report("searching for composite faces");
		
		//System.out.println("BrushList:output");
		//search for composite faces
		for (Brush b:brushes) {
		    //System.out.printf("%nbrush %d",b.getBrushNumber());
		    Vector<Face> bf=b.getFaces ();
		    for(Face f:bf){//check every face of every immutable brush to see if it is part of a composite face
			if(!facesInComposites.contains(f)){//if the composite containing this face has already been found, do not do it again
			    //System.out.printf("%n f%d",f.getNumber());
			    CompositeFace composite=new CompositeFace(f);//if f is does not form a composite face, this object will not be added to a list
			    /* clones the Vector's internal array of pointers, but does not 
			     *  clone the Brushes they point to. do the clone, because
			     *  the Vector will be changed*/
			    Vector<Brush> bvToCheck=(Vector<Brush>)b.getBrushInteractionListClone();
			    Vector<Brush> bvDone=new Vector<Brush>(6,6);
			    bvDone.add(b);//this brush's brush interaction list is already in bvToCheck.
			    //kind of like a region grow algorithm, searching through brushes connected by their interaction lists
			    while(bvToCheck.size()>0){
				Brush bChecking=bvToCheck.remove(0);
				//System.out.print("  faces of brush "+bChecking.getBrushNumber()+" ");
				bvDone.add(bChecking);//remember so we do not check it again.
				Vector<Face> fvToCheck=bChecking.getFaces();
				boolean foundAny=false;
				for(Face fToCheck:fvToCheck){
				    //System.out.printf("p&c:"+f.isParallelAndCoincident(fToCheck)+" ");
				    //System.out.printf("abt:"+tf(f.isAbutted(fToCheck))+" ");
				    if(f.isParallelAndCoincident(fToCheck) && f.isAbutted(fToCheck)){
					composite.addFace(fToCheck);
					//so we do not find the same composite again. not sure the if(contains()) is necessary
					if(!facesInComposites.contains(fToCheck)) facesInComposites.add(fToCheck);
					foundAny=true;
				    }
				}
				//if this brush had a member of the composite then the composite may propogate across it
				if(foundAny){
				    Vector<Brush> bvMaybeAddToCheck=bChecking.getBrushInteractionList();
				    for(Brush bMaybeAddToCheck:bvMaybeAddToCheck){
					if(!bvDone.contains(bMaybeAddToCheck)) bvToCheck.add(bMaybeAddToCheck);
				    }
				}
			    }
			    //System.out.printf("composite has %d faces%n",composite.members.size());
			    if(composite.hasMultipleMembers()){//if there is more than one face in this composite
				compositeFaces.add(composite);
				
				for(Brush bDone:bvDone){//the done list is a list of brushes nearby
				    bDone.addToCompositeFaceInterationList(composite);
				}
			    }
			}
		    }
		}
		//System.out.println();
		TimeCounter.reportf("found %d composite faces",compositeFaces.size());
	}
		
		
	
	public static Vector<Brush> getImmutableOpaqueBrushes() {
		return brushes;
	}
	
	private static String tf(boolean val){
	    return val?"t":"f";
	}

}
