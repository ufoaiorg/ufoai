package net.ufoai.sf.maputils;

/**
 * Edge.java
 * Created on 24 April 2008, 21:09
 * @author andy buckle
 */
public class Edge {
    
    Vector3D point1, point2;
    
    /**  */
    public Edge(Vector3D vertex1, Vector3D vertex2) {
        point1=vertex1;
        point2=vertex2;
    }
    
    public String toString(){
        return point1.toString()+point2.toString();
    }
    
}
