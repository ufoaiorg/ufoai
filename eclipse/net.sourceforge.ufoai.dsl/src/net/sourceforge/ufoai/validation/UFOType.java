package net.sourceforge.ufoai.validation;

import java.util.Arrays;
import java.util.Collection;
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
		 * An anonymous block, which the content looks like a map, key-values
		 * @see ANONYMOUS_BLOCK
		 */
		ANONYMOUS_MAP_BLOCK,

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
		 * A string witch contain a link to an image relative to "base/pics"
		 * directory and do not contains file extension
		 */
		PICS_IMAGE,

		/**
		 * A string witch contain a link to an image relative to "base"
		 * directory and do not contains file extension
		 */
		BASE_IMAGE,

		/**
		 * A string witch contain a prefix to a set of images relative to "base/pics/geoscape"
		 * directory.
		 */
		GEOSCAPE_IMAGE,

		/**
		 * A string witch contain a link to a model relative to "base/models"
		 * directory and do not contains file extension
		 */
		MODEL,

		/**
		 * A string witch contain a link to a sound relative to "base/sound"
		 * directory and do not contains file extension
		 */
		SOUND,

		/**
		 * A string witch contain a link to a sound relative to base/music
		 * and do not contains file extension
		 */
		MUSIC,

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
		 * A string containing a 4D vector
		 */
		VEC4,

		/**
		 * A string containing a shape
		 */
		SHAPE,

		/**
		 * A reference to an object
		 */
		ID,

		/**
		 * A reference to an object or a model name
		 */
		MODEL_OR_ID,

		/**
		 * A list of element of the same type
		 */
		LIST,

		/**
		 * A list of element of the same type,
		 * with an extra token identifier.
		 */
		IDENTIFIED_LIST,

		/**
		 * A tuple of a know number of elements, type of the element depends
		 * on the position of the element
		 */
		TUPLE
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

	public boolean hasValues() {
		return values != null && !values.isEmpty();
	}

	public boolean contains(String value) {
		if (values == null)
			return false;
		return values.contains(value);
	}

	public Collection<String> getValues() {
		return values;
	}

}
