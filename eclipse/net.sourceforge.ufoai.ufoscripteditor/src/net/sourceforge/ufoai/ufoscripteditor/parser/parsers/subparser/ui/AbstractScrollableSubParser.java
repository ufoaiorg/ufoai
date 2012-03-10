package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

public class AbstractScrollableSubParser extends AbstractNodeSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFONodeParserFactoryAdapter() {
		@Override
		public String getID() {
			return "abstractscrollable";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new AbstractScrollableSubParser(ctx);
		}

	};

	@Override
	public IUFOSubParserFactory getNodeSubParserFactory() {
		return FACTORY;
	}

	public AbstractScrollableSubParser(IParserContext ctx) {
		super(ctx);
		IUFOSubParserFactory factory = getNodeSubParserFactory();
		// Registration for event properties
		registerSubParser(EventPropertySubParser.ONVIEWCHANGE, factory);
	}
}
