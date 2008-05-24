package net.ufoai.sf.maputils;

import java.util.Vector;

/**
 * a composite face is composed of parallel abutted faces from different brushes. 
 *
 * @author andy buckle
 */
public class CompositeFace {
    
    Vector<Face> members=new Vector<Face>(3,3);
    boolean sorted=false;
    
    /**  */
    public CompositeFace (Face f) {
	members.add (f);
    }
    
    public boolean hasMultipleMembers(){
	return members.size ()>1;
    }
    
    public void addFace(Face f){
	members.add(f);
	boolean sorted=false;
    }

    /** as {@link Face#isFacingAndCoincidentTo(Face)} */
    public boolean isFacingAndCoincidentTo(Face bf) {
	return members.get(0).isFacingAndCoincidentTo(bf);
    }

    /** @return true if each point is inside (or within Epsilon of)
     *          one of the parent brushes of one of the faces making up this 
     *          composite. */
    boolean areInsideParentBrushes(Vector<Vector3D> vertsOfFaceBf) {
	for(Vector3D p:vertsOfFaceBf){
	    boolean isInsideAtLeastOne=false;
	    for(Face f:members){
		if(f.getParent().inside(p)) isInsideAtLeastOne=true;
	    }
	    if(!isInsideAtLeastOne) return false;
	}
	return true;
    }
    
    public String toString(){
	String s="";
	for(Face f:members){
	    s+=String.format("(Entity %d Brush %d Face %d)",f.getParent().getParentEntity().getNumber(),f.getParent().getBrushNumber(),f.getNumber());
	}
	return s;
    }

    /*
	boolean coversConsideringLevelFlags(Face bf) {
		Vector3D 
		for(Vector3D p:vertsOfFaceBf){
		    boolean isInsideAtLeastOne=false;
		    for(Face f:members){
			if(f.getParent().inside(p)) isInsideAtLeastOne=true;
		    }
		    if(!isInsideAtLeastOne) return false;
		}
		return true;
	}
     **/
    
    
}
