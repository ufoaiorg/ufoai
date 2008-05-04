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
       
    int numberOfFacesWithLevelFlagsChanged=0;
    String comments="";
    int s, e;
    Map map;
    private static Pattern linePattern=Pattern.compile("^.+?$", Pattern.MULTILINE);// a line with at least one char
    Vector<Face> faces=new Vector<Face>(6,2);
    Vector<Vector3D> vertices=new Vector<Vector3D>(8,8);
    /** list of immutable (not breakable or mobile) brushes that this brush's bounding box intersects. 
     *  brushes listed may be in func_group entities or worldspawn. */
    Vector<Brush> interactionList=new Vector<Brush>(6,6);
    String lastLevelFlagChangeNote="";
    private static int brushCount=0;
    private int brushNumber;
    private Entity parentEntity;
    
    /**bounding box parameters*/
    float xmax, xmin, ymax, ymin, zmax, zmin;
    
    /**  */
    public Brush(Map from, Entity parent , int startIndex, int endIndex) {
        parentEntity=parent;
        brushNumber=brushCount++;
        Face.resetCount();
        s=startIndex;
        e=endIndex;
        map=from;
        String txt=getOriginalText();
        Matcher faceMatcher=linePattern.matcher(txt);
        while(faceMatcher.find()){
            if(!faceMatcher.group().startsWith("//")) faces.add(new Face(map, this, s+faceMatcher.start(), s+faceMatcher.end()));
        }
        
        //System.out.println("Brush: >"+map.toString(s,e)+"<");
        Vector3D point=new Vector3D(1.0f,1.0f,1.0f);
        //System.out.println("inside("+point+"): "+inside(point)) ;
        calculateVertices();
        findBoundingBox();
    }
    
    public static void resetBrushCount(){
        brushCount=0;
    }
    
    public int getBrushNumber(){
        return brushNumber;
    }
    
    public Entity getParentEntity(){
        return parentEntity;
    }
    
    public int countVertices(){
        return vertices.size();
    }
    
    public Vector<Vector3D> getVertices(){
        return vertices;
    }
    
    /** @return the vertices of this Brush which are on the plane of the 
     *          supplied Face. */
    public Vector<Vector3D> getVertices(Face f){
        Vector<Vector3D> ans=new Vector<Vector3D>(4,4);
        HessianNormalPlane h=f.getHessian();
        for(Vector3D v:vertices) {
            if(h.absDistance(v)<Epsilon.distance) ans.add(v);
        }
        return ans;
    }
    
    public int countFaces(){
        return faces.size();
    }
    
    public void addComment(String comment){
        comments+=String.format("// %s%n",comment);
    }
    
    
    /** order n cubed code. aaaargh */
    private void calculateVertices(){
        for(int i=1;i<faces.size();i++){
            for(int j=0;j<i;j++){
                for(int k=j+1;k<faces.size();k++){
                    Vector3D candidate=HessianNormalPlane.getIntersection(faces.get(i).getHessian(),faces.get(j).getHessian(),faces.get(k).getHessian());
                    if(candidate!=null){//if 3 faces do not intersect at point
                        if(inside(candidate)) {//in case 3 planes of faces intersect away from brush
                            vertices.add(candidate);
                        }
                    }
                }
            }
        }
    }
    
    
    /** not efficient code */
    public Vector<Edge> getEdges(){
        Vector<Edge> ans=new Vector<Edge>(6,6);
        for(int i=0;i<vertices.size();i++){
            for(int j=i+1;j<vertices.size();j++){//loop through all pairs of vertices
                for(int k=0;k<faces.size();k++){
                    Face facek=faces.get(k);
                    Vector3D verti=vertices.get(i), vertj=vertices.get(j);
                    if(facek.contains(verti) && facek.contains(vertj) ){//if both points are common to one face
                        for(int l=k+1;l<faces.size();l++){
                            Face facel=faces.get(l);
                            if(facel.contains(verti) && facel.contains(vertj)) {//and are also common to a different face
                                ans.add(new Edge(verti, vertj));//then they must be the edge at the intersection of those faces
                            }
                        }
                    }
                }
            }
        }
        return ans;
    }
    
    public Vector<Face> getFaces(){
        return faces;
    }
    
    /** like calculateVertices but does not exclude those outside the brush.
     *  for debugging. */
    private Vector<Vector3D> calculateAllFaceIntersections(){
        Vector<Vector3D> intersections=new Vector<Vector3D>(10,10);
        for(int i=1;i<faces.size();i++){
            for(int j=0;j<i;j++){
                for(int k=j+1;k<faces.size();k++){
                    Vector3D candidate=HessianNormalPlane.getIntersection(faces.get(i).getHessian(),faces.get(j).getHessian(),faces.get(k).getHessian());
                    if(candidate!=null){//if 3 faces do not intersect at point
                        intersections.add(candidate);
                    }
                }
            }
        }
        return intersections;
    }
    
    private void findBoundingBox(){
        xmax=xmin=vertices.get(0).x;
        ymax=ymin=vertices.get(0).y;
        zmax=zmin=vertices.get(0).z;
        for(int i=1;i<vertices.size();i++){
            Vector3D v=vertices.get(i);
            xmax=xmax>v.x?xmax:v.x;
            xmin=xmin<v.x?xmin:v.x;
            ymax=ymax>v.y?ymax:v.y;
            ymin=ymin<v.y?ymin:v.y;
            zmax=zmax>v.z?zmax:v.z;
            zmin=zmin<v.z?zmin:v.z;
        }
    }
    
    /** @return true if <b>this</b> Brush has a bounding box that intersects
     *  with that of the supplied bounding box. */
    public boolean boundingBoxIntersects(Brush a){
        boolean zNoOverlap= this.zmax < a.zmin || this.zmin > a.zmax ;
        boolean yNoOverlap= this.ymax < a.ymin || this.ymin > a.ymax ;
        boolean xNoOverlap= this.xmax < a.xmin || this.xmin > a.xmax ;
        return !zNoOverlap && !yNoOverlap && !xNoOverlap;
    }
    
    public String boundingBoxToString(){
        return String.format("(%5.1f %5.1f,%5.1f %5.1f,%5.1f %5.1f)",xmin, xmax, ymin, ymax, zmin, zmax);
    }
    
    /** calculates  level based on vertex coordinates.  level flags are 
     *  determined by the lowest point on the brush. */
    public int calculateLevelBasedOnVertexCoordinates(){
        int level=ContentFlags.clipLevel((int)zmin/64);
        int topOnLevel=ContentFlags.clipLevel((int)zmax/64);
        if(topOnLevel-level>1) MapUtils.printf("Brush.getLevel: brush seems to span more than 2 levels%n");
        return level;
    }
    
    public void setLevelFlagsBasedOnVertexCoordinates(){
        int level=calculateLevelBasedOnVertexCoordinates();
        for(Face f:faces) f.setLevelFlags(level);
        if(numberOfFacesWithLevelFlagsChanged!=0){
            MapUtils.printf("Brush had level flags changed. calculated to be on level %d. %s%n",level,lastLevelFlagChangeNote);
        }
    }
    
    /** assumes that the contentflags are listed the same on each face.
     * @return the Brush's levelflags masked out from the content flags.  */
    public int getLevelFlags(){
        return faces.get(0).getLevelFlags();
    }
    
    void notifyLevelFlagChange(String note){
        if(lastLevelFlagChangeNote.length()!=0 && !note.equals(lastLevelFlagChangeNote)){
            MapUtils.printf("Brush had faces with different levelflags set%n");
        }
        lastLevelFlagChangeNote=note;
        numberOfFacesWithLevelFlagsChanged++;
    }
    
    public void writeReformedText(PrintStream to){
        to.println("{");
        to.print(comments);
        for(Face f:faces) f.writeReformedText(to);
        to.println("}");
    }
    
    /** uses the epsilon tolerance. this will only work for convex polyhedra.
     *  @return true if the supplied point is inside the brush */
    public boolean inside(Vector3D point){
        //if the point is on the wrong side of any face, then it is outside
        for(int i=0;i<faces.size();i++){
            if(faces.get(i).getHessian().distance(point)>Epsilon.distance) {
                return false;
            }
        }
        return true;
    }
    
    /** tests if all of the points in teh supplied vector are inside() this Brush */
    public boolean areInside(Vector<Vector3D> points){
        for(Vector3D p:points) if(!this.inside(p)) return false;
        return true;
    }
    
    /** @return true if the brush has at least one transparent face, otherwise returns false */
    public boolean hasTransparentFaces(){
        for(Face f:faces){
            if(f.isTransparent()) return true;
        }
        return false;
    }
    
    /** assumes the faces are all labelled the same. uses the contentflags of 
     *  the zeroth face.
     *  @return true if the brush is an actorclip, weaponclip or stepon */
    public boolean isActorclipWeaponClipOrStepon(){
        return faces.get(0).isActorclipWeaponClipOrStepon();
    }
    
    /** like inside(). for debugging.
     *  @return a list of the indices of the faces which this point is outside */
    public Vector<Integer> getExcludingFaces(Vector3D point){
        //if the point is on the wrong side of any face, then it is outside
        Vector<Integer> excluding=new Vector<Integer>(10,10);
        for(int i=0;i<faces.size();i++){
            if(faces.get(i).getHessian().distance(point)>Epsilon.distance) {
                excluding.add(i);
            }
        }
        return excluding;
    }
    
    /** @return the part of the text file upon which this object is based */
    public String getOriginalText(){
        return map.toString(s,e);
    }
    
    public void addToInteractionList(Brush b){
        interactionList.add(b);
    }
    
    public Vector<Brush> getInteractionList(){
        return interactionList;
    }

    void detailedInvestigation() {
        System.out.printf("%nBrush.detailedInvestigation%n");
        //src
        writeReformedText(System.out);
        //intersections (including vertices)
        Vector<Vector3D> intersections=calculateAllFaceIntersections();
        for(Vector3D intersection:intersections){
            System.out.print(""+intersection+" excluded by faces:");
            Vector<Integer> excludingFaces=getExcludingFaces(intersection);
            for(Integer i:excludingFaces) System.out.print(" "+i);
            System.out.println();
        }
        //face information
        System.out.println("faces ");
        for(int i=0;i<faces.size();i++){
            Face f=faces.get(i);
            HessianNormalPlane h=f.getHessian();
            System.out.printf("%d unit normal:%s distance to origin:%5.1f%n",i,""+h.n,h.p);
        }
        //edges
        System.out.println("edges");
        Vector<Edge> edges=this.getEdges();
        for(Edge e:edges) {
            System.out.println(""+e);
        }
        //display
        OrthographicProjectionFrame opf=new OrthographicProjectionFrame();
        for(Vector3D intersection:intersections){
            opf.addPoint(intersection,Color.gray,1);
        }
        for(Edge e:edges) opf.addLine(e.point1, e.point2, Color.gray);
        opf.setVisible(true);
        
    }
    
}
