package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.tech.DescriptionParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.tech.MailParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.tech.MailPreParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.tech.PreDescriptionParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.tech.RequireAndParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.tech.RequireOrParser;

public class TechnologyParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new TechnologyParser(ctx);
		}

		@Override
		public String getID() {
			return "tech";
		}
	};

	public TechnologyParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(RequireOrParser.FACTORY, FACTORY);
		registerSubParser(RequireAndParser.FACTORY, FACTORY);
		registerSubParser(DescriptionParser.FACTORY, FACTORY);
		registerSubParser(PreDescriptionParser.FACTORY, FACTORY);
		registerSubParser(MailParser.FACTORY, FACTORY);
		registerSubParser(MailPreParser.FACTORY, FACTORY);
	}
}
