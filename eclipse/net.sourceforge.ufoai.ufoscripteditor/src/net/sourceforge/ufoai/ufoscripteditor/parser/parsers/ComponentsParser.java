package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;

public class ComponentsParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new ComponentsParser(ctx);
		}

		@Override
		public String getID() {
			return "components";
		}
	};

	public ComponentsParser(IParserContext ctx) {
		super(ctx);
	}

	@Override
	public int getTokenCountPerEntry(String key) {
		if (key.equalsIgnoreCase("time"))
			return 2;
		return 4;
	}
}
