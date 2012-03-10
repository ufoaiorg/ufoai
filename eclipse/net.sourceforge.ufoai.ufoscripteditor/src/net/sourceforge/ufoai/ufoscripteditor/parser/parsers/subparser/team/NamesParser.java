package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.team;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class NamesParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "models";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new NamesParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public NamesParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(NeutralParser.FACTORY, FACTORY);
		registerSubParser(LastNameParser.FACTORY, FACTORY);
		registerSubParser(FemaleParser.FACTORY, FACTORY);
		registerSubParser(MaleParser.FACTORY, FACTORY);
	}
}
