package net.ufoai.sf.maputils;

import java.awt.Color;
import java.io.PrintStream;
import java.util.Vector;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Brush.java
 * Created on 12 April 2008, 09:11
 * @author andy buckle
 */
public class Brush {

	int numberOfFacesWithLevelFlagsChanged = 0;
	String comments = "";
	int s, e;
	Map map;
	private static Pattern linePattern = Pattern.compile ("^.+?$", Pattern.MULTILINE);// a line with at least one char
	private static final int DEFAULT_INCLUSIVE=0, DEFAULT_EXCLUSIVE=1;
	protected static int maxInteractionListSize=0;
	
	Vector<Face> faces = new Vector<Face> (6, 2);
	Vector<Vector3D> vertices = new Vector<Vector3D> (8, 8);
	/** list of immutable (not breakable or mobile) brushes that this brush's bounding box intersects.
	 *  brushes listed may be in func_group entities or worldspawn. */
	Vector<Brush> interactionList = new Vector<Brush> (6, 6);
	/** list of {@CompositeFace}s which contain faces from this brush. TODO sync this with: the truth and, the getter method */
	Vector<CompositeFace> compositeFaceInterationList = new Vector<CompositeFace> (4,4);
	String lastLevelFlagChangeNote = "";
	private static int brushCount = 0;
	private int brushNumber;
	private Entity parentEntity;

	/**bounding box parameters*/
	float xmax, xmin, ymax, ymin, zmax, zmin;

	/**  */
	public Brush (Map from, Entity parent , int startIndex, int endIndex) {
		parentEntity = parent;
		brushNumber = brushCount++;
		Face.resetCount();
		s = startIndex;
		e = endIndex;
		map = from;
		String txt = getOriginalText();
		Matcher faceMatcher = linePattern.matcher (txt);
		while (faceMatcher.find() ) {
			if (!faceMatcher.group().startsWith ("//") ) faces.add (new Face (map, this, s + faceMatcher.start(), s + faceMatcher.end() ) );
		}

		//System.out.println("Brush: >"+map.toString(s,e)+"<");
		///Vector3D point = new Vector3D (1.0f, 1.0f, 1.0f);
		//System.out.println("inside("+point+"): "+inside(point)) ;
		calculateVertices();
		//System.out.println("Brush.init: num vertices "+vertices.size());
		findBoundingBox();
		for(Face f:faces){
		    f.calculateVertices();
		}
	}

	public static void resetBrushCount() {
		brushCount = 0;
	}

	public int getBrushNumber() {
		return brushNumber;
	}

	public Entity getParentEntity() {
		return parentEntity;
	}

	public int countVertices() {
		return vertices.size();
	}

	public Vector<Vector3D> getVertices() {
		return vertices;
	}

	/** @return the vertices of this Brush which are on the plane of the
	 *          supplied Face. */
	public Vector<Vector3D> getVertices (Face f) {
		Vector<Vector3D> ans = new Vector<Vector3D> (4, 4);
		HessianNormalPlane h = f.getHessian();
		for (Vector3D v: vertices) {
			if (h.absDistance (v) < Epsilon.distance) ans.add (v);
		}
		return ans;
	}

	public int countFaces() {
		return faces.size();
	}

	public void addComment (String comment) {
		comments += String.format ("// %s%n", comment);
	}

	/** this method is slow, it should be rewritten if it is to be used 
	 *  frequently. calculates the coordinates of the vertices of the face.
	 *  the face is moved away from the brush (only within the scope of this
	 *  method) by a distance dist. dist is specified: will probably be an
	 *  epsilon. */
	public Vector<Vector3D> calculateVerticesOfFacePushedOut(Face f, float dist){
	    Vector<Vector3D> trueVerts=f.getVertices();
	    Vector<Vector3D> tmpvertices=new Vector<Vector3D>(6,6);
	    for(Vector3D v:trueVerts){
		tmpvertices.add(v.add(f.hessian.n.multiply(dist)));//move verts out in direction of unit normal
	    }
	    if (tmpvertices.size()==0) System.out.println("Brush.calculateVerticesOfFacePushedOut: face has no vertices");
	    return tmpvertices;
	}
	
	/** order n cubed code. aaaargh. 
	 *  TODO see if this function is the source of the (occasional) 
	 *  vertex that is not found.*/
	private void calculateVertices() {
		for (int i = 1;i < faces.size();i++) {
			for (int j = 0;j < i;j++) {
				for (int k = j + 1;k < faces.size();k++) {
					Vector3D candidate = HessianNormalPlane.getIntersection (faces.get (i).getHessian(), faces.get (j).getHessian(), faces.get (k).getHessian() );
					if (candidate != null) {//if 3 faces do not intersect at point
						if (insideInclusive (candidate) ) {//in case 3 planes of faces intersect away from brush
							vertices.add (candidate);
						}
					}
				}
			}
		}
	}

	public Vector<Face> getFaces() {
		return faces;
	}

	/** like calculateVertices but does not exclude those outside the brush.
	 *  for debugging. */
	private Vector<Vector3D> calculateAllFaceIntersections() {
		Vector<Vector3D> intersections = new Vector<Vector3D> (10, 10);
		for (int i = 1;i < faces.size();i++) {
			for (int j = 0;j < i;j++) {
				for (int k = j + 1;k < faces.size();k++) {
					Vector3D candidate = HessianNormalPlane.getIntersection (faces.get (i).getHessian(), faces.get (j).getHessian(), faces.get (k).getHessian() );
					if (candidate != null) {//if 3 faces do not intersect at point
						intersections.add (candidate);
					}
				}
			}
		}
		return intersections;
	}

	private void findBoundingBox() {
		if(vertices.size ()==0) {/* hack to ignore result of poor vertex finding */
		    MapUtils.printf ("Brush %d from Entity %d (%s) has no vertices%n",brushNumber, parentEntity.getNumber (),parentEntity.getValue ("classname"));
		    xmax=xmin=ymax=ymin=zmax=zmin=-4096;
		    return;
		}
		xmax = xmin = vertices.get (0).x;
		ymax = ymin = vertices.get (0).y;
		zmax = zmin = vertices.get (0).z;
		for (int i = 1;i < vertices.size();i++) {
			Vector3D v = vertices.get (i);
			xmax = xmax > v.x ? xmax : v.x;
			xmin = xmin < v.x ? xmin : v.x;
			ymax = ymax > v.y ? ymax : v.y;
			ymin = ymin < v.y ? ymin : v.y;
			zmax = zmax > v.z ? zmax : v.z;
			zmin = zmin < v.z ? zmin : v.z;
		}
	}

	/** @return true if <b>this</b> Brush has a bounding box that intersects
	 *  with that of the supplied bounding box. Uses Epsilon.distance */
	public boolean boundingBoxIntersects (Brush a) {
		boolean zNoOverlap = this.zmax < a.zmin-Epsilon.distance || this.zmin-Epsilon.distance > a.zmax ;
		boolean yNoOverlap = this.ymax < a.ymin-Epsilon.distance || this.ymin-Epsilon.distance > a.ymax ;
		boolean xNoOverlap = this.xmax < a.xmin-Epsilon.distance || this.xmin-Epsilon.distance > a.xmax ;
		return !zNoOverlap && !yNoOverlap && !xNoOverlap;
	}

	public String boundingBoxToString() {
		return String.format ("(%5.1f %5.1f,%5.1f %5.1f,%5.1f %5.1f)", xmin, xmax, ymin, ymax, zmin, zmax);
	}

	/** calculates  level based on vertex coordinates.  level flags are
	 *  determined by the lowest point on the brush. */
	public int calculateLevelBasedOnVertexCoordinates() {
		int level = ContentFlags.clipLevel ( (int) zmin / 64);
		int topOnLevel = ContentFlags.clipLevel ( (int) zmax / 64);
		if (topOnLevel - level > 1) MapUtils.printf ("Brush.getLevel: brush seems to span more than 2 levels%n");
		return level;
	}

	public void setLevelFlagsBasedOnVertexCoordinates() {
		int level = calculateLevelBasedOnVertexCoordinates();
		for (Face f: faces) f.setLevelFlags (level);
		if (numberOfFacesWithLevelFlagsChanged != 0) {
			//MapUtils.printf ("Brush had level flags changed. calculated to be on level %d. %s%n", level, lastLevelFlagChangeNote);
		}
	}
	
	public boolean haveLevelFlagsBeenChanged(){
	    return numberOfFacesWithLevelFlagsChanged!=0;
	}

	void notifyLevelFlagChange (String note) {
		if (lastLevelFlagChangeNote.length() != 0 && !note.equals (lastLevelFlagChangeNote) ) {
			MapUtils.printf ("Brush had faces with different levelflags set%n");
		}
		lastLevelFlagChangeNote = note;
		numberOfFacesWithLevelFlagsChanged++;
	}

	public void writeReformedText (PrintStream to) {
		to.println ("{");
		to.print (comments);
		for (Face f: faces) f.writeReformedText (to);
		to.println ("}");
	}

	/** grows the brush by an epsilon distance, so points on the surface
	 *  will be included. 
	 *  this will only work for convex polyhedra.
	 *  @return true if the supplied point is inside the brush */
	public boolean insideInclusive (Vector3D point) {
		return insideInclusive(point, Epsilon.distance);
	}
	
	public boolean insideInclusive (Vector3D point, float epsilonMargin) {
		//if the point is on the wrong side of any face, then it is outside
		for (int i = 0;i < faces.size();i++) {
			if (faces.get(i).getHessian().distance (point) > epsilonMargin) {
				//System.out.println("Brush.insideInclusive: "+point+" is outside "+faces.get(i));
				return false;
			}
		}
		return true;
	}
	
	/** shrinks the brush by an epsilon distance, so points on the surface
	 *  will be excluded. 
	 *  this will only work for convex polyhedra.
	 *  @return true if the supplied point is inside the brush */
	public boolean insideExclusive (Vector3D point) {
		//if the point is on the wrong side of any face, then it is outside
		for (int i = 0;i < faces.size();i++) {
			if (faces.get (i).getHessian().distance (point) > -Epsilon.distance) {
				return false;
			}
		}
		return true;
	}
	
	/** returns true if any of the vertices of face f are inside or within 
	 *  Epsilon of the surface if this brush*/
	public boolean insideInclusive(Face f){
	    Vector<Vector3D> verts=f.getVertices();
	    for(Vector3D p:verts) if(this.insideInclusive(p)) return true;
	    return false;
	}
	
	/** tests if all of the points in teh supplied vector are inside() this Brush */
	public boolean areInside (Vector<Vector3D> points) {
		for (Vector3D p: points) if (!this.insideInclusive (p) ) return false;
		return true;
	}

	/** @return true if the brush has at least one transparent face, otherwise returns false */
	public boolean hasTransparentFaces() {
		for (Face f: faces) {
			if (f.isTransparent() ) return true;
		}
		return false;
	}

	/** assumes the faces are all labelled the same. uses the contentflags of
	 *  the zeroth face.
	 *  @return true if the brush is an actorclip, weaponclip or stepon */
	public boolean isActorclipWeaponClipOrStepon() {
		return faces.get (0).isActorclipWeaponClipOrStepon();
	}

	/** like inside(). for debugging.
	 *  @return a list of the indices of the faces which this point is outside */
	public Vector<Integer> getExcludingFaces (Vector3D point) {
		//if the point is on the wrong side of any face, then it is outside
		Vector<Integer> excluding = new Vector<Integer> (10, 10);
		for (int i = 0;i < faces.size();i++) {
			if (faces.get (i).getHessian().distance (point) > Epsilon.distance) {
				excluding.add (i);
			}
		}
		return excluding;
	}

	/** @return the part of the text file upon which this object is based */
	public String getOriginalText() {
		return map.toString (s, e);
	}

	public void addToBrushInteractionList (Brush b) {
		interactionList.add (b);
		maxInteractionListSize = (maxInteractionListSize < interactionList.size()) ? interactionList.size() : maxInteractionListSize;
	}
	
	public void addToCompositeFaceInterationList(CompositeFace c){
	    compositeFaceInterationList.add (c);
	}

	/** @return a Vector of Brushes whose bounding boxes intersect with this Brush's. */
	public Vector<Brush> getBrushInteractionList() {
		return interactionList;
	}
	
	/** @return a Vector of Brushes whose bounding boxes intersect with this Brush's. */
	public Vector<Brush> getBrushInteractionListClone() {
	    Vector<Brush> clone=new Vector<Brush>(interactionList.size(),10);
	    for(Brush b:interactionList) clone.add(b);
	    return clone;
	}
	
	/** @return {@link CompositeFace}s featuring  faces from brushes whose 
	 *           bounding boxes intersect with this Brush's. */
	public Vector<CompositeFace> getCompositeFaceInteractionList(){
	    return compositeFaceInterationList;
	}

	void detailedInvestigation() {
		System.out.printf ("%nBrush.detailedInvestigation%n");
		//src
		writeReformedText (System.out);
		//intersections (including vertices)
		Vector<Vector3D> intersections = calculateAllFaceIntersections();
		for (Vector3D intersection: intersections) {
			System.out.print ("" + intersection + " excluded by faces:");
			Vector<Integer> excludingFaces = getExcludingFaces (intersection);
			for (Integer i: excludingFaces) System.out.print (" " + i);
			System.out.println();
		}
		//face information
		System.out.println ("faces ");
		for (int i = 0;i < faces.size();i++) {
			Face f = faces.get (i);
			HessianNormalPlane h = f.getHessian();
			System.out.printf ("%d unit normal:%s distance to origin:%5.1f%n", i, "" + h.n, h.p);
		}
		
		//display
		OrthographicProjectionFrame opf = new OrthographicProjectionFrame();
		for (Vector3D intersection: intersections) {
			opf.addPoint (intersection, Color.gray, 1);
		}
		opf.setVisible (true);

	}
	
	/** @return true if this Brush and o represent the same Brush parsed from the map file */
	public boolean equals(Object o){
	    try{
		Brush bo=(Brush)o;
		return bo.brushNumber== this.brushNumber && bo.parentEntity.getNumber() == this.parentEntity.getNumber();
	    } catch (Exception e){
		return false;
	    }
	}

	
	public String verboseInfo(){
	    return String.format("brush(%d) faces:%d vertices:%d ",this.getBrushNumber(), faces.size(), vertices.size());
	}
	
	/** sets the error texture on all faces */
	public void setError(){
	    for (Face f:faces) f.setError();
	}

}
