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
    Brush brush;
    private int number;
    Vector<String> parts=new Vector<String>(16,3);
    HessianNormalPlane hessian;
    SurfaceFlags surfFlags;
    ContentFlags contentFlags;
    
    private static int count=0;
    public static final String nodrawTexture="tex_common/nodraw";
    private static Pattern partPattern=Pattern.compile("[\\s]([^\\s\\(\\)]+)");
    public static final int PART_INDEX_P1X=0,
                            PART_INDEX_P1Y=1,
                            PART_INDEX_P1Z=2,
                            PART_INDEX_P2X=3,
                            PART_INDEX_P2Y=4,
                            PART_INDEX_P2Z=5,
                            PART_INDEX_P3X=6,
                            PART_INDEX_P3Y=7,
                            PART_INDEX_P3Z=8,
                            PART_INDEX_TEX=9,
                            PART_INDEX_X_OFF=10,
                            PART_INDEX_Y_OFF=11,
                            PART_INDEX_ROT_ANGLE=12,
                            PART_INDEX_X_SCALE=13,
                            PART_INDEX_Y_SCALE=14,
                            PART_INDEX_CONTENT_FLAGS=15,
                            PART_INDEX_SURFACE_FLAGS=16,
                            PART_INDEX_UNKNOWN2=17;
    
    /**  */
    public Face(Map from, Brush brushFrom, int startIndex, int endIndex) {
        number=count++;
        s=startIndex;
        e=endIndex;
        map=from;
        brush=brushFrom;
        //System.out.println("");
        //System.out.println("Face: >"+getOriginalText()+"<");
        Matcher partMatcher=partPattern.matcher(getOriginalText());
        while(partMatcher.find()){
            //System.out.print("  >"+partMatcher.group(1)+"<");
            parts.add(partMatcher.group(1));
        }
        while(parts.size()<18) parts.add("0");//every face should have all of them, even if only to have levelflags set
        //System.out.println("orig>"+getOriginalText()+"<");
        //System.out.println("refo>"+getReformedText()+"<");
        //Vector3D origin=new Vector3D(0.0f,0.0f,0.0f);
        hessian=new HessianNormalPlane(getPoint(0), getPoint(1), getPoint(2) );
        //System.out.println(""+getOriginalText()+" > "+hessian);
        //System.out.println("distance to origin: "+hessian.distance(origin));
        surfFlags=new SurfaceFlags(getPartInt(PART_INDEX_SURFACE_FLAGS));
        contentFlags=ContentFlags.flags(getPartInt(PART_INDEX_CONTENT_FLAGS));
    }
    
    public static void resetCount(){
        count=0;
    }
    
    public int getNumber(){
        return number;
    }
    
    public void writeReformedText(PrintStream to){
        to.print("(");
        for(int i=0;i<parts.size();i++){
            if(i==3 || i==6) to.print(" (");
            to.print(" "+parts.get(i));
            if(i==2 || i==5 || i==8) to.print(" )");
        }
        to.println();
    }
    
    /** @return the part of the text file upon which this object is based */
    public String getOriginalText(){
        return map.toString(s,e);
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
    public String getPart(int index){
        return parts.get(index);
    }
    
    public int getPartInt(int index){
        return Integer.parseInt(getPart(index));
    }
    
    public void setPart(int index, Object to){
        parts.set(index, to.toString());
    }
    
    /** @param pointIndex the index of the point: 0, 1 or 2. */
    public Vector3D getPoint(int pointIndex){
        float[] coords=new float[3];
        int firstCoord=pointIndex*3;
        for(int i=0;i<3;i++){
            coords[i]=Float.parseFloat(parts.get(i+firstCoord));
        }
        return new Vector3D(coords[0],coords[1],coords[2]);
    }
    
    /** calculates the Hessian Normal Form of the plane of the face */
    public HessianNormalPlane getHessian(){
        return hessian;
    }
    
    /** @return true if <b>this</b> is parallel to <code>a</code>. within Epsilon.angle */
    public boolean isParallelTo(Face a){
        return this.getHessian().cosTo(a.getHessian()) >= Epsilon.cos ;
    }
    
    /** @return true if <b>this</b> is antiparallel (points the other way) 
     *               to <code>a</code>. within Epsilon.angle */
    public boolean isAntiparallelTo(Face a){
        //System.out.printf("Face.isAntiparallelTo: cos %f cf -Epsilon.cos %f%n",this.getHessian().cosTo(a.getHessian()),-Epsilon.cos);
        return this.getHessian().cosTo(a.getHessian()) <= -Epsilon.cos ;
    }
    
    /** calculates if <b>this</b> Face faces Face a. The surface unit normals 
     *  must be antiparallel (ie they face each other), and, the distance
     *  to the origin must be such that they occupy the same region of
     *  space, to within Epsilon.distance. These are based on consideration of
     *  the planes of the faces only - they could be offset by a long way. 
     * @return true if the faces face and touch */
    public boolean isFacingAndCoincidentTo(Face a){
        float distance=this.getHessian().absDistance(a.getHessian().getClosestApproach());
        //System.out.printf("Face.isFacingAndCoincidentTo: distance between planes %5.1f%n",distance);
        return distance<Epsilon.distance && this.isAntiparallelTo(a);
    }
    
    /** @return true if the point is on the plain of this face*/
    public boolean contains(Vector3D point){
        return Math.abs(this.getHessian().distance(point))<Epsilon.distance;
    }
    
    /** @param level  the level to set the flags for. eg if the face is on level 
     *                3 then send 3 as the arg, and this method will set the flag for 
     *                4,5, ...., 8. */
    public void setLevelFlags(int level){
        ContentFlags oldFlags=ContentFlags.flags(Integer.parseInt(parts.get(PART_INDEX_CONTENT_FLAGS))) ;
        ContentFlags newFlags=ContentFlags.level(level);
        if(!oldFlags.equals(newFlags)) {
            //MapUtils.printf("changed level flags");
            brush.notifyLevelFlagChange(oldFlags.getDescription() +" to "+ newFlags.getDescription());
            oldFlags.mergeNewFlags(newFlags);
            parts.set(PART_INDEX_CONTENT_FLAGS,oldFlags.toString());
        }
    }
    
    /** @return the levelflags masked out of the contentflags. */
    public int getLevelFlags(){
        return ContentFlags.mask(Integer.parseInt(parts.get(PART_INDEX_CONTENT_FLAGS))) ;
    }
    
    public boolean isActorclipWeaponClipOrStepon(){
        return contentFlags.isActorclipWeaponClipOrStepon();
    }

    void setNodraw() {
        surfFlags.setNodraw();
        setPart(PART_INDEX_SURFACE_FLAGS,""+surfFlags);
        setPart(PART_INDEX_TEX, ""+nodrawTexture);
        
    }
    
    public boolean isTransparent(){
        return surfFlags.isTransparent();
    }
    
    
}
