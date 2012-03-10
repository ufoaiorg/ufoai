package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;

public class RankParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new RankParser(ctx);
		}

		@Override
		public String getID() {
			return "rank";
		}
	};

	public RankParser(IParserContext ctx) {
		super(ctx);
	}
}
