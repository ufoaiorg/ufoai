package net.sourceforge.ufoai.validation;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class UFOType {

	enum Type {
		/**
		 * A block
		 */
		BLOCK,

		/**
		 * A string
		 */
		STRING,

		/**
		 * A value from an enumeration
		 */
		ENUM,

		/**
		 * A string witch contain a link to an image relative to base/pics
		 * and do not contains file extension
		 */
		PICS_IMAGE,

		/**
		 * A string which should be prefixed with '_'
		 */
		TRANSLATED_STRING,

		/**
		 * An integer number
		 */
		INT,

		/**
		 * A string witch is a color
		 */
		COLOR,

		/**
		 * A boolean
		 */
		BOOLEAN,

		/**
		 * A real number
		 */
		REAL,

		/**
		 * A string containing two integer
		 */
		VEC2,

		/**
		 * A reference to an object
		 */
		ID,
		TECH_ID,
		BUILDING_ID,
	}

	private Type type;
	private Set<String> values;

	public UFOType(String type) {
		this.type = Type.valueOf(type);
	}

	public UFOType(String type, String values) {
		this(type);
		if (values != null) {
			this.values = new HashSet<String>(Arrays.asList(values.split(",")));
		}
	}

	public Type getType() {
		return type;
	}

	public boolean contains(String value) {
		if (values == null)
			return false;
		return values.contains(value);
	}

}
