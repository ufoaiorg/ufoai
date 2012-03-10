package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.alienteam.CategorySubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.alienteam.EquipmentSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.alienteam.TeamSubParser;

public class AlienTeamParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new AlienTeamParser(ctx);
		}

		@Override
		public String getID() {
			return "alienteam";
		}
	};

	public AlienTeamParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(CategorySubParser.FACTORY, FACTORY);
		registerSubParser(EquipmentSubParser.FACTORY, FACTORY);
		registerSubParser(TeamSubParser.FACTORY, FACTORY);
	}
}
