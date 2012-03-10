package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.team.ActorSoundsParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.team.ModelsParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.team.NamesParser;

public class TeamParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new TeamParser(ctx);
		}

		@Override
		public String getID() {
			return "team";
		}
	};

	public TeamParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(ModelsParser.FACTORY, FACTORY);
		registerSubParser(NamesParser.FACTORY, FACTORY);
		registerSubParser(ActorSoundsParser.FACTORY, FACTORY);
	}
}
