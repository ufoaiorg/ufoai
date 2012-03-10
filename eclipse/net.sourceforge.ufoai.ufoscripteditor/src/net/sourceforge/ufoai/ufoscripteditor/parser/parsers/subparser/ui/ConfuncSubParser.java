package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

public class ConfuncSubParser extends SpecialSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFONodeParserFactoryAdapter() {
		@Override
		public String getID() {
			return "confunc";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new ConfuncSubParser(ctx);
		}

	};

	@Override
	public IUFOSubParserFactory getNodeSubParserFactory() {
		return FACTORY;
	}

	// TODO: we have to override the parse function anyway for this node type
	@Override
	public boolean hasKeyValuePairs() {
		return false;
	}

	public ConfuncSubParser(IParserContext ctx) {
		super(ctx);
	}
}
