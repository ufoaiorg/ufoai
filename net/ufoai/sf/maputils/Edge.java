package net.ufoai.sf.maputils;

/**
 * Edge.java
 * Created on 24 April 2008, 21:09
 * @author andy buckle
 */
public class Edge {

	/** a vertex at the end of the edge */
	Vector3D point1, point2;
	
	/** direction of the line of the edge. the magnitude is the distance 
	 *  between the vertices. */
	Vector3D dir;
	
	/** unit vector in the direction of the edge. */
	Vector3D unitDir;
	
	/** the length of the edge */
	float length;
	
	/**  */
	public Edge (Vector3D vertex1, Vector3D vertex2) {
		point1 = vertex1;
		point2 = vertex2;
		dir=point2.subtract (point1);
		unitDir=dir.unit ();
		length=dir.magnitude ();
	}

	public String toString() {
		return point1.toString() + point2.toString();
	}
	
	public boolean isAbutted(Edge a){
		//if the vertices
		float d1, d2;
		try{
		    d1=this.distanceAlongEdge (a.point1);
		    d2=this.distanceAlongEdge (a.point2);
		} catch(NotInLineWithEdgeException eNILWE){
		    return false;
		}
		//if both of the vertices of a are not between the vertices of
		//this edge, on the same side, then they are not abutted.
		int decision=isDistanceOnEdge(d1)+isDistanceOnEdge(d2);
		return decision != 2 && decision != -2;
	}
	
	/** @return 0 if the distance supplied is greater than zero and less than the
	 *          length of the edge. Includes a margin of Epsilon.distance. 
	 *          returns -1 if the distance is below zero. returns 1 if is 
	 *          greater than the length of the edge.*/
	public int isDistanceOnEdge(float distance){
	    return distance > -Epsilon.distance && distance < length+Epsilon.distance ? 0 : (distance<1 ? -1 : 1);
	}
	
	/** @return the distance along the edge from vertex1 of the supplied point, p. 
	 *          if the point is not between vertex1 and vertex2: a negative distance is
	 *          returned if p is the other side of vertex1. a distance greater than
	 *          the length of the edge is returned if p is the other side of vertex2.
	 *  @throws NotInLineWithEdgeException if the supplied point, p is not in line with the edge*/
	public float distanceAlongEdge(Vector3D p) throws NotInLineWithEdgeException {
	    if(distanceFromLine(p)>Epsilon.distance) throw new NotInLineWithEdgeException();
	    return -((point1.subtract(p)).dot(dir)) / length;
	}
	
	/** Calculates the distance of a point from the line of the edge. */
	public float distanceFromLine(Vector3D p){
	    return (dir.cross(point1.subtract(p))).magnitude()/length;
	}
	
	/** test code. */
	public static void main (String[] args) {
	    Vector3D v1=new Vector3D(1,2,3);
	    Vector3D v2=new Vector3D(11,7,4);
	    Edge e1=new Edge(v1,v2);
	    Vector3D notOn=new Vector3D(2,1,3);
	    Vector3D onEdge=v1.add (v2).divide (2.0f);
	    Vector3D dirn=v2.subtract (v1);
	    Vector3D before1=v1.subtract (dirn);
	    Vector3D after2=v2.add (dirn);
	    try{
		System.out.println("Edge:"+e1+"  point:"+onEdge+"  distanceAlongEdge:"+e1.distanceAlongEdge(onEdge)+"  length of edge:"+e1.length);
	    } catch (NotInLineWithEdgeException eNILWE){System.out.println(eNILWE.getMessage ());}
	    
	    System.out.println("trying point not in line with edge");
	    try{
		System.out.println("Edge:"+e1+"  point:"+notOn+"  distanceAlongEdge:"+e1.distanceAlongEdge(notOn)+"  length of edge:"+e1.length);
	    } catch (NotInLineWithEdgeException eNILWE){System.out.println("NotInLineWithEdgeException thrown");}
	    
	    try{
		System.out.println("before:"+e1.isDistanceOnEdge (e1.distanceAlongEdge (before1)));
	    } catch (NotInLineWithEdgeException eNILWE){System.out.println("NotInLineWithEdgeException thrown");}
	    
	    try{
		System.out.println("on:"+e1.isDistanceOnEdge (e1.distanceAlongEdge (onEdge)));
	    } catch (NotInLineWithEdgeException eNILWE){System.out.println("NotInLineWithEdgeException thrown");}
	    
	    try{
		System.out.println("after:"+e1.isDistanceOnEdge (e1.distanceAlongEdge (after2)));
	    } catch (NotInLineWithEdgeException eNILWE){System.out.println("NotInLineWithEdgeException thrown");}
	    
	    Vector3D la=new Vector3D(0,3,4), lb=new Vector3D(0,-3,-4);
	    Edge ed=new Edge(la,lb);
	    Vector3D tp=new Vector3D(0,-4,3);
	    System.out.println("Edge:"+ed+"  point:"+tp+"  dist:"+ed.distanceFromLine(tp));
	}

}

class NotInLineWithEdgeException extends Exception {
    
    NotInLineWithEdgeException(String message){
	super(message);
    }
    
    NotInLineWithEdgeException(){;}
    
}
