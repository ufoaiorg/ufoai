package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

public class AbstractNodeSubParser extends UFONodeSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFONodeParserFactoryAdapter() {
		@Override
		public String getID() {
			return "abstractnode";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new AbstractNodeSubParser(ctx);
		}

	};

	@Override
	public IUFOSubParserFactory getNodeSubParserFactory() {
		return FACTORY;
	}

	public AbstractNodeSubParser(IParserContext ctx) {
		super(ctx);
		IUFOSubParserFactory factory = getNodeSubParserFactory();
		// Registration for event properties
		registerSubParser(EventPropertySubParser.ONCHANGE, factory);
		registerSubParser(EventPropertySubParser.ONCLICK, factory);
		registerSubParser(EventPropertySubParser.ONMCLICK, factory);
		registerSubParser(EventPropertySubParser.ONMOUSEENTER, factory);
		registerSubParser(EventPropertySubParser.ONMOUSELEAVE, factory);
		registerSubParser(EventPropertySubParser.ONRCLICK, factory);
		registerSubParser(EventPropertySubParser.ONWHEEL, factory);
		registerSubParser(EventPropertySubParser.ONWHEELDOWN, factory);
		registerSubParser(EventPropertySubParser.ONWHEELUP, factory);
	}
}
