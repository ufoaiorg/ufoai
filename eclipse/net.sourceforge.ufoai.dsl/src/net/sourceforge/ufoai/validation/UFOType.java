package net.sourceforge.ufoai.validation;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class UFOType {

	enum Type {
		/**
		 * A block, identified or not
		 * @see IDENTIFIED_BLOCK
		 * @see ANONYMOUS_BLOCK
		 */
		BLOCK,

		/**
		 * A block with an extra token identifier
		 * blockname identifier { ... }
		 * @see BLOCK
		 * @see ANONYMOUS_BLOCK
		 */
		IDENTIFIED_BLOCK,

		/**
		 * A block without identifier
		 * blockname { ... }
		 * @see IDENTIFIED_BLOCK
		 * @see BLOCK
		 */
		ANONYMOUS_BLOCK,

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
		 * A string witch contain a link to a model relative to base/models
		 * and do not contains file extension
		 */
		MODEL,

		/**
		 * A string witch contain a link to a sound relative to base/sound
		 * and do not contains file extension
		 */
		SOUND,

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
		 * A string witch is a color
		 */
		DATE,

		/**
		 * A boolean
		 */
		BOOLEAN,

		/**
		 * A real number
		 */
		REAL,

		/**
		 * A string containing a 2D vector
		 */
		VEC2,

		/**
		 * A string containing a 3D vector
		 */
		VEC3,

		/**
		 * A string containing a shape
		 */
		SHAPE,

		/**
		 * A reference to an object
		 */
		ID,
		TECH_ID,
		BUILDING_ID,
		PARTICLE_ID,
		MENU_MODEL_ID,
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
