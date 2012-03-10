package net.sourceforge.ufoai.ufoscripteditor.parser;

public interface IUFOParserFactory {
	String getID();

	String getIcon();

	IUFOParser create(IParserContext ctx);

	/**
	 * @return <code>true</code> in case the script id has the format:
	 *         <tt>id name { ... }</tt> - <code>false</code> if not.
	 */
	boolean isNameAfterID();

	/**
	 * Some entries don't have a name, they are only marked by an id followed by
	 * a script block surrounded by { }
	 *
	 * @return <code>true</code> if the id of the block should be used as name,
	 *         <code>false</code> otherwise
	 */
	boolean isIDName();
}
