package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

public class FuncSubParser extends SpecialSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFONodeParserFactoryAdapter() {
		@Override
		public String getID() {
			return "func";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new FuncSubParser(ctx);
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

	public FuncSubParser(IParserContext ctx) {
		super(ctx);
	}
}
