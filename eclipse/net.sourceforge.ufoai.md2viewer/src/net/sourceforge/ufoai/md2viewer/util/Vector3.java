package net.sourceforge.ufoai.md2viewer.util;

public class Vector3 {
	private final float x;
	private final float y;
	private final float z;

	public Vector3(float x, float y, float z) {
		this.x = x;
		this.y = y;
		this.z = z;
	}

	public float getX() {
		return x;
	}

	public float getY() {
		return y;
	}

	public float getZ() {
		return z;
	}

	public float[] getFloats() {
		return new float[] { x, y, z };
	}

	@Override
	public String toString() {
		return "Vector3 [x=" + x + ", y=" + y + ", z=" + z + "]";
	}

	public Vector3 crossProduct(Vector3 other) {
		return new Vector3(y * other.getZ() - z * other.getY(), z
				* other.getX() - x * other.getZ(), x * other.getY() - y
				* other.getX());
	}

	@Override
	public int hashCode() {
		final int prime = 31;
		int result = 1;
		result = prime * result + Float.floatToIntBits(x);
		result = prime * result + Float.floatToIntBits(y);
		result = prime * result + Float.floatToIntBits(z);
		return result;
	}

	@Override
	public boolean equals(Object obj) {
		if (this == obj)
			return true;
		if (obj == null)
			return false;
		if (getClass() != obj.getClass())
			return false;
		Vector3 other = (Vector3) obj;
		if (Float.floatToIntBits(x) != Float.floatToIntBits(other.x))
			return false;
		if (Float.floatToIntBits(y) != Float.floatToIntBits(other.y))
			return false;
		if (Float.floatToIntBits(z) != Float.floatToIntBits(other.z))
			return false;
		return true;
	}

	public Vector3 add(Vector3 add) {
		return new Vector3(getX() + add.getX(), getY() + add.getY(), getZ()
				+ add.getZ());
	}
}
