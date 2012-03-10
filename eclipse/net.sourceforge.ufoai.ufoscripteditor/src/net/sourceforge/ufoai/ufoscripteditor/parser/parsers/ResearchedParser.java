package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;

public class ResearchedParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new ResearchedParser(ctx);
		}

		@Override
		public String getID() {
			return "researched";
		}
	};

	public ResearchedParser(IParserContext ctx) {
		super(ctx);
	}

	@Override
	public boolean hasOnlyValue() {
		return true;
	}
}
