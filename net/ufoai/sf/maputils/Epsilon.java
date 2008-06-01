package net.ufoai.sf.maputils;

/**
 * Epsilon.java
 *
 * Tolerances. used for deciding if theinag are close enough
 *
 * Created on 13 April 2008, 21:25
 * @author andy buckle
 *
 *
 */
public class Epsilon {

	/** used for deciding is a point is close enough to being in (or on the surface of)
	 *  the brush. */
	public static final float distance = 0.001f;

	/** used or deciding if things are close enough to being parallel. in radians */
	public static final float angle = 0.001f;

	/** limit for the cos of a small angle. if the small angle (in radians) is larger than
	 *  Epsilon.angle , then the cosine of the angle will be greater than cos, */
	public static final float cos = 1.0f - angle*angle / 2.0f;

	static {
		MapUtils.printf ("Epsilon distance %f  angle %f  cos angle %f%n", distance, angle, cos);
	}
}
