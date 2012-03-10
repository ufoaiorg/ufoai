package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.team;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class ActorSoundsParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "actorsounds";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new ActorSoundsParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public ActorSoundsParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(NeutralParser.FACTORY, FACTORY);
		registerSubParser(FemaleParser.FACTORY, FACTORY);
		registerSubParser(MaleParser.FACTORY, FACTORY);
	}
}
