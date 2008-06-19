package net.ufoai.sf.maputils;

import java.util.Vector;

/**
 * a composite face is composed of parallel abutted faces from different brushes. 
 *
 * @author andy buckle
 */
public class CompositeFace {
    
    public static final int CROSS_EXTERNAL=0;
    public static final int CROSS_NONE=1;
    public static final int CROSS_INTERNAL=2;
    
    Vector<Face> members=new Vector<Face>(3,3);
    
    /**  */
    public CompositeFace (Face f) {
	members.add (f);
    }
    
    public boolean hasMultipleMembers(){
	return members.size ()>1;
    }
    
    public void addFace(Face f){
	members.add(f);
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
		if(f.getParent().insideInclusive(p)) isInsideAtLeastOne=true;
	    }
	    if(!isInsideAtLeastOne) return false;
	}
	return true;
    }
    
    /** calculates if each vertex of each face in this composite is outside
     *  the supplied face. it excludes points on the edge edge of face f. 
     *  this condition must be met if the arg Face f is
     *  to be considered covered for nodraw setting purposes.
     *  @return true if none of the vertices of the members of the composite 
     *          face are in the middle of (edges excluded) the face f. */
    boolean compositeFaceVerticesAreOutside(Face f){
	for (Face mf:members ) {
	    //System.out.println("checking composite member face from brush "+mf.getParent().getBrushNumber());
	    Vector<Vector3D> verts = mf.getParent().getVertices();
	    for(Vector3D v:verts) {
		//if(f.getParent().insideInclusive(v) && !f.isOnEdge(v)) System.out.println("vert "+v+" vs "+f+" inside brush:"+f.getParent().insideInclusive(v) +"  onEdge: "+ f.isOnEdge(v));
		if(f.getParent().insideInclusive(v) && !f.isOnEdge(v)) return false;
	    }
	}
	return true;
    }
    
    /** @return CROSS_EXTERNAL if any of the edges of the supplied face cross the 
     *          boundary of the composite face. CROSS_INTERNAL if CROSS_EXTERNAL does not
     *          apply and any of the edges of the supplied face cross internal boundaries
     *          between members of the composite face. CROSS_NONE is returned if there
     *          are no crossings and so the supplied face is covered by a single
     *          face only*/
    public int crossesEdge(Face f){
	Vector<Edge> fEdges=f.calculateEdges();
	int internalCrosses=0;
	for(Edge e:fEdges){
	    Vector<Vector3D> crossPoints=new Vector<Vector3D>(2,2);
	    for(Face member:members){
		Vector<Edge> mEdges=member.calculateEdges();
		for(Edge mEdge: mEdges){
		    try{
			Vector3D intersection=mEdge.intersection(e);
			if(crossPoints.contains(intersection)){
			    //if present remove it
			    //pairs of intersections are only internal, therefore OK.
			    crossPoints.remove(intersection);
			    internalCrosses++;
			} else {
			    crossPoints.add(intersection);
			}
		    } catch (NotInLineWithEdgeException eNILWE){;}//no intersection carry on
		}
	    }
	    if(crossPoints.size()>0) return CROSS_EXTERNAL;//if any edge of the supplied face has an unpaired crossing
	}
	if(internalCrosses==0) {
	    return CROSS_NONE;
	} else {
	    return CROSS_INTERNAL;
	}
    }
        
    public String toString(){
	String s="";
	for(Face f:members){
	    s+=String.format("(Entity %d Brush %d Face %d)",f.getParent().getParentEntity().getNumber(),f.getParent().getBrushNumber(),f.getNumber());
	}
	return s;
    }

    
	/** this will fail where one of the composite faces doing the hiding does not
	 *  have a vertex of the face being hidden stuck on it. (and the levelflags are
	 *  different. */
	boolean coversConsideringLevelFlags(Face bf) {
		Vector<Vector3D> vertsOfFaceBf=bf.getVertices();
		for(Vector3D p:vertsOfFaceBf){
		    for(Face f:members){
			if(f.getParent().insideInclusive(p)) {//every relevant face in the composite must cover wrt lvlflags
			    if(!f.coversConsideringLevelFlags(bf)) return false;
			}
		    }
		}
		return true;
	}
     
    
    
}
