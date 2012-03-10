package net.sourceforge.ufoai.ufoscripteditor.parser;

import java.util.List;

import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

public interface IUFOParser {
	List<UFOScriptBlock> getInnerScriptBlocks(String string, int line);

	IUFOSubParserFactory createSubParserFactory(final String id);

	boolean hasKeyValuePairs();

	IParserContext getParserContext();

	void registerSubParser(final IUFOSubParserFactory parser,
			final IUFOParserFactory parentFactory);

	void validate();
}
