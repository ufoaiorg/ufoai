package net.ufoai.sf.maputils;

import java.io.PrintStream;
import java.util.Vector;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Face.java
 * Created on 12 April 2008, 10:54
 * @author andy buckle
 */
public class Face {

	int s, e;
	Map map;
	Brush parentBrush;
	private int number;
	Vector<String> parts = new Vector<String> (16, 3);
	HessianNormalPlane hessian;
	SurfaceFlags surfFlags;
	ContentFlags contentFlags;
	Vector<Vector3D> vertices=null;
	Vector<Edge> edges=new Vector<Edge>(4,4);

	private static int count = 0;
	public static final String nodrawTexture = "tex_common/nodraw";
	private static Pattern partPattern = Pattern.compile ("[\\s]([^\\s\\(\\)]+)");
	public static final int PART_INDEX_P1X = 0,
			PART_INDEX_P1Y = 1,
			PART_INDEX_P1Z = 2,
			PART_INDEX_P2X = 3,
			PART_INDEX_P2Y = 4,
			PART_INDEX_P2Z = 5,
			PART_INDEX_P3X = 6,
			PART_INDEX_P3Y = 7,
			PART_INDEX_P3Z = 8,
			PART_INDEX_TEX = 9,
			PART_INDEX_X_OFF = 10,
			PART_INDEX_Y_OFF = 11,
			PART_INDEX_ROT_ANGLE = 12,
			PART_INDEX_X_SCALE = 13,
			PART_INDEX_Y_SCALE = 14,
			PART_INDEX_CONTENT_FLAGS = 15,
			PART_INDEX_SURFACE_FLAGS = 16,
			PART_INDEX_UNKNOWN2 = 17;

	/**  */
	public Face (Map from, Brush brushFrom, int startIndex, int endIndex) {
		number = count++;
		s = startIndex;
		e = endIndex;
		map = from;
		parentBrush = brushFrom;
		Matcher partMatcher = partPattern.matcher (getOriginalText() );
		while (partMatcher.find() ) {
			parts.add (partMatcher.group (1) );
		}
		while (parts.size() < 18) parts.add ("0");//every face should have all of them, even if only to have levelflags set
		hessian = new HessianNormalPlane (getPoint (0), getPoint (1), getPoint (2) );
		surfFlags = new SurfaceFlags (getPartInt (PART_INDEX_SURFACE_FLAGS) );
		contentFlags = ContentFlags.flags (getPartInt (PART_INDEX_CONTENT_FLAGS) );
		
		//System.out.println("Face.init: Edge "+number+" has "+edges.size()+" edges");
		//System.out.println("Face.init: Edge "+number+" has "+vertices.size()+" vertices");
	}
	
	/** this must be done at the end of the parent Brush constructor, 
	 *  otherwise the Brush does not have any vertices yet, 
	 *  because it has not calculted the intersection of the planes of the 
	 *  Faces yet, because it does not have any Face objects... */
	public void calculateVerticesAndEdges(){
	    if(vertices!=null) return;//already done
	    vertices=parentBrush.getVertices (this);
	    for(int i=0;i<vertices.size ();i++){
		int v2ind=i+1;
		v2ind= (v2ind==vertices.size()) ? 0 : v2ind ;//join up last vertex with first
		edges.add (new Edge(vertices.get (i),vertices.get (v2ind)));
	    }
	}

	public Vector<Vector3D> getVertices(){
	    return vertices;
	}
	
	public Brush getParent(){
	    return parentBrush;
	}
	
	public static void resetCount() {
		count = 0;
	}

	public int getNumber() {
		return number;
	}

	public void writeReformedText (PrintStream to) {
		to.print ("(");
		for (int i = 0;i < parts.size();i++) {
			if (i == 3 || i == 6) to.print (" (");
			to.print (" " + parts.get (i) );
			if (i == 2 || i == 5 || i == 8) to.print (" )");
		}
		to.println();
	}

	/** @return the part of the text file upon which this object is based */
	public String getOriginalText() {
		return map.toString (s, e);
	}

	/** parts are <pre>
	 *  <li> 0: point 1 x
	 *  <li> 1: point 1 y
	 *  <li> 2: point 1 z
	 *  <li> 3: point 2 x
	 *  <li> 4: point 2 y
	 *  <li> 5: point 2 z
	 *  <li> 6: point 3 x
	 *  <li> 7: point 3 y
	 *  <li> 8: point 3 z
	 *  <li> 9: texture
	 *  <li>11: x_off     - Texture x-offset
	 *  <li>12: y_off     - Texture y-offset
	 *  <li>13: rot_angle - floating point value indicating texture rotation
	 *  <li>14: x_scale   - scales x-dimension of texture (negative value to flip)
	 *  <li>15: y_scale   - scales y-dimension of texture (negative value to flip)
	 *  <li>16: content flags (includes levelflags, stepon, ...)
	 *  <li>17: surface flags
	 *  <li>18: ?
	 * </pre>
	 */
	public String getPart (int index) {
		return parts.get (index);
	}

	public int getPartInt (int index) {
		return Integer.parseInt (getPart (index) );
	}

	public void setPart (int index, Object to) {
		parts.set (index, to.toString() );
	}

	/** @param pointIndex the index of the point: 0, 1 or 2. */
	public Vector3D getPoint (int pointIndex) {
		float[] coords = new float[3];
		int firstCoord = pointIndex * 3;
		for (int i = 0;i < 3;i++) {
			coords[i] = Float.parseFloat (parts.get (i + firstCoord) );
		}
		return new Vector3D (coords[0], coords[1], coords[2]);
	}

	/** calculates the Hessian Normal Form of the plane of the face */
	public HessianNormalPlane getHessian() {
		return hessian;
	}

	/** @return true if <b>this</b> is parallel to <code>a</code>. within Epsilon.angle.
	 *               uses unit normal vectors. Will return false if the normals are antiparallel.*/
	public boolean isParallelTo (Face a) {
		return this.getHessian().cosTo (a.getHessian() ) >= Epsilon.cos ;
	}

	/** @return true if <b>this</b> is antiparallel (points the other way)
	 *               to <code>a</code>. within Epsilon.angle */
	public boolean isAntiparallelTo (Face a) {
		//System.out.printf("Face.isAntiparallelTo: cos %f cf -Epsilon.cos %f%n",this.getHessian().cosTo(a.getHessian()),-Epsilon.cos);
		return this.getHessian().cosTo (a.getHessian() ) <= -Epsilon.cos ;
	}

	/** calculates if <b>this</b> Face faces Face a. The surface unit normals
	 *  must be antiparallel (ie they face each other), and, the distance
	 *  to the origin must be such that they occupy the same region of
	 *  space, to within Epsilon.distance. These are based on consideration of
	 *  the planes of the faces only - they could be offset by a long way.
	 * @return true if the faces face and touch */
	public boolean isFacingAndCoincidentTo (Face a) {
		float distance = this.getHessian().absDistance (a.getHessian().getClosestApproach() );
		//System.out.printf("Face.isFacingAndCoincidentTo: distance between planes %5.1f%n",distance);
		return distance < Epsilon.distance && this.isAntiparallelTo (a);
	}
	
	
	/** returns true if the plane's unit normals are parallel and the planes
	 *  are within Epsilon.distance of each other. */
	public boolean isParallelAndCoincident(Face a){
		float distance = this.getHessian().absDistance (a.getHessian().getClosestApproach() );
		//System.out.printf("Face.isFacingAndCoincidentTo: distance between planes %5.1f%n",distance);
		return distance < Epsilon.distance && this.isParallelTo (a);
	}
	
	/** @return true if at least one edge of <code>a</code> is abutted to 
	 *          an edge of <code>this</code> brush. */
	public boolean isAbutted(Face a){
	    for (Edge ei:this.edges){
		for (Edge ej:a.edges){
		    if(ei.isAbutted(ej)) return true;
		}
	    }
	    return false;
	}

	/** @return true if the point is on the plane of this face*/
	public boolean contains (Vector3D point) {
		return Math.abs (this.getHessian().distance (point) ) < Epsilon.distance;
	}

	/** @param level  the level to set the flags for. eg if the face is on level
	 *                3 then send 3 as the arg, and this method will set the flag for
	 *                4,5, ...., 8. */
	public void setLevelFlags (int level) {
		ContentFlags oldFlags = ContentFlags.flags (Integer.parseInt (parts.get (PART_INDEX_CONTENT_FLAGS) ) ) ;
		ContentFlags newFlags = ContentFlags.level (level);
		if (!oldFlags.equals (newFlags) ) {
			//MapUtils.printf("changed level flags");
			parentBrush.notifyLevelFlagChange (oldFlags.getDescription() + " to " + newFlags.getDescription() );
			oldFlags.mergeNewFlags (newFlags);
			parts.set (PART_INDEX_CONTENT_FLAGS, oldFlags.toString() );
		}
	}

	/** @return the levelflags masked out of the contentflags. */
	public int getLevelFlags() {
		return ContentFlags.mask (Integer.parseInt (parts.get (PART_INDEX_CONTENT_FLAGS) ) ) ;
	}

	public boolean isActorclipWeaponClipOrStepon() {
		return contentFlags.isActorclipWeaponClipOrStepon();
	}

	public boolean isNodraw(){
	    return surfFlags.isNodraw ();
	}
	
	/** sets the nodraw flag and texture. Note: does not test if the 
	  * face is already a nodraw, this should be done previously, by the 
	  * calling method.*/
	void setNodraw() {
		surfFlags.setNodraw();
		setPart (PART_INDEX_SURFACE_FLAGS, "" + surfFlags);
		setPart (PART_INDEX_TEX, "" + nodrawTexture);

	}

	public boolean isTransparent() {
		return surfFlags.isTransparent();
	}
	
	/** @return true if they represent the same faces parsed from the map file */
	public boolean equals(Object o){
	    try{
		Face fo=(Face)o;
		return fo.parentBrush.equals(this.parentBrush) && fo.number == this.number;
	    } catch (Exception e){
		return false;
	    }
	}


}
