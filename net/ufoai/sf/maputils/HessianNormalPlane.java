package net.ufoai.sf.maputils;

/**
 * HessianNormalPlane.java
 * Created on 13 April 2008, 07:08
 *
 * see http://mathworld.wolfram.com/HessianNormalForm.html
 *
 * @author andy buckle
 */
public class HessianNormalPlane {
    
    /** a unit Vector normal to the plane */
    Vector3D n;
    
    /** the distance from the plane to the origin */
    float p;
     
    
    /** calculate Hessian Normal Form from 3 points on the plane.
     *  if the points on the plane go clockwise when you look at the plane, then
     *  the unit normal will point towards you. 
     */
    public HessianNormalPlane(Vector3D p1, Vector3D p2, Vector3D p3) {
        Vector3D inPlane1=p1.subtract(p2);
        Vector3D inPlane2=p3.subtract(p2);
        Vector3D normal=inPlane1.cross(inPlane2);//cross product gives vector normal to plane
        n=normal.unit();
        //System.out.println("");
        //System.out.println("inPlane1"+inPlane1+"  inPlane2"+inPlane2+"  normal"+normal+"  unit"+n);
        p=-n.dot(p1);
    }
    
    /** @return a point on the plane. Specifically the plane's point of closest 
     *          approach to the origin.*/
    public Vector3D getClosestApproach(){
        return n.multiply(-p);
    }
    
    
    /** calculates the distance from a point to a plane 
     *  @param point the point to calculate the distance to, from <b>this</b> plane.
     *  @return the distance. if the vector pointing from <code>point</code> to the 
     *          plane is in the same direction as the normal then the distance is posotive.
     *          put another way: if the point is in the half-space to which the unit normal 
     *          points, then the distance will be negative.
     */
    public float distance(Vector3D point){
        return n.dot(point)+p;
    }
    
    /** as distance(). Takes the absolute of the result. */
    public float absDistance(Vector3D point){
        return Math.abs(n.dot(point)+p);
    }
    
    public String toString(){
        return String.format(""+n+".x%+5.1f=0",p);
    }
    
    /** calculates the cosine of the angle between <b>this</b> plane and <code>a</code> (dihedral angle). */
    public float cosTo(HessianNormalPlane a){
        return this.n.dot(a.n);
    }
    
    /** @return the point where the three planes intersect, or null if they
     *  do not make a point because; 1) any two of them are parallel, or, 2) the 
     * lines of intersection are parallel. uses Epsilon.angle for deciding 
     */
    public static Vector3D getIntersection(HessianNormalPlane a, HessianNormalPlane b, HessianNormalPlane c){
        Vector3D bcIntersectionDirn=b.n.cross(c.n);
        float sinAngle=bcIntersectionDirn.magnitude();//sin of angle between planes b and c (approx equal to angle: small angle approx)
        if(sinAngle<Epsilon.angle) return null;//planes parallel, cannot intersect
        bcIntersectionDirn=bcIntersectionDirn.divide(sinAngle);//make it into a unit vector
        float atobcintersection=a.n.dot(bcIntersectionDirn);// calculate cos of angle to normal of plane a.
        // this is equal to sin of angle to plane a. if bc intersection line is parallel to plane a, then 
        // it cannot intersect.
        if (atobcintersection<Epsilon.angle) return null;
        //see http://geometryalgorithms.com/Archive/algorithm_0104/algorithm_0104B.htm
        Vector3D term1=b.n.cross(c.n).multiply(-a.p);
        Vector3D term2=c.n.cross(a.n).multiply(-b.p);
        Vector3D term3=a.n.cross(b.n).multiply(-c.p);
        float divisor=a.n.dot(b.n.cross(c.n));
        return (term1.add(term2.add(term3))).divide(divisor);
    }
    
}
