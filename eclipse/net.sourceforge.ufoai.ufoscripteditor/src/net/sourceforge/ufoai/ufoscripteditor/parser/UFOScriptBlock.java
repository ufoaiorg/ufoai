package net.sourceforge.ufoai.ufoscripteditor.parser;

public class UFOScriptBlock {
	private final String id;
	private final String name;
	private final int startLine;
	private final int lines;
	private final String data;
	private final IUFOParserFactory parser;

	public UFOScriptBlock(String id, String name, IUFOParserFactory parser,
			String data, int startLine, int lines) {
		this.id = id;
		this.name = name;
		this.parser = parser;
		this.data = data;
		this.startLine = startLine;
		this.lines = lines;
	}

	public String getId() {
		return id;
	}

	public String getName() {
		return name;
	}

	public int getStartLine() {
		return startLine;
	}

	public int getLines() {
		return lines;
	}

	public String getData() {
		return data;
	}

	/**
	 * @return Might return <code>null</code> if this is only a key/value data
	 *         block
	 */
	public IUFOParserFactory getParser() {
		return parser;
	}

	@Override
	public String toString() {
		return "UFOScriptBlock [data=" + data + ", id=" + id + ", lines="
				+ lines + ", name=" + name + ", parser=" + parser
				+ ", startLine=" + startLine + "]";
	}

	public boolean hasChildren() {
		return parser != null && !data.isEmpty();
	}
}
