package net.sourceforge.ufoai.validation;

public enum UFOTypes {

	/**
	 * A block
	 */
	BLOCK,

	/**
	 * A string
	 */
	STRING,

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
	BUILDING_ID
}
