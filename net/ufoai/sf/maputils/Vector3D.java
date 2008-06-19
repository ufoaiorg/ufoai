package net.ufoai.sf.maputils;

/**
 * Vector3D.java
 * Created on 11 October 2007, 22:24
 * @author andy buckle
 */

public class Vector3D implements Cloneable {

	public float x = 0, y = 0, z = 0;

	/**  */
	public Vector3D (float xValue, float yValue, float zValue) {
		x = xValue;
		y = yValue;
		z = zValue;
	}

	public Vector3D clone() {
		return new Vector3D (x, y, z);
	}

	public Vector3D cross (Vector3D a) {
		return new Vector3D ( this.y*a.z - this.z*a.y, this.z*a.x - this.x*a.z, this.x*a.y - this.y*a.x);
	}

	public float dot (Vector3D a) {
		return a.x*x + a.y*y + a.z*z;
	}
	
	public float cosTo(Vector3D a){
	    return this.unit().dot(a.unit());
	}
	
	/** @return true if <b>this</b> is parallel to <code>a</code>. within Epsilon.angle.
	 *               */
	public boolean isParallelOrAntiparallelTo (Vector3D a) {
		return Math.abs(this.cosTo(a)) >= Epsilon.cos ;
	}

	public Vector3D add (Vector3D v) {
		return new Vector3D (this.x + v.x, this.y + v.y, this.z + v.z);
	}

	public Vector3D subtract (Vector3D v) {
		return new Vector3D (this.x -v.x, this.y - v.y, this.z - v.z);
	}

	/** divide by a scalar */
	public Vector3D divide (float divisor) {
		return new Vector3D (x / divisor, y / divisor, z / divisor);
	}

	/** multiply by a scalar */
	public Vector3D multiply (float factor) {
		return new Vector3D (x*factor, y*factor, z*factor);
	}

	public float magnitude() {
		return (float) Math.sqrt ( (float) (x*x + y*y + z*z) );
	}

	/** @return a unit vector in the direction of this vector */
	public Vector3D unit() {
		float m = magnitude();
		return new Vector3D (x / m, y / m, z / m);
	}

	public String toString() {
		return String.format ("(%05.1f,%05.1f,%05.1f)", x, y, z);
	}

	public boolean equals(Object o){
	    try{
		Vector3D v=(Vector3D)o;
		return this.subtract(v).magnitude() <= Epsilon.distance;
	    }catch (Exception e){
		return false;
	    }
	}
	
}
