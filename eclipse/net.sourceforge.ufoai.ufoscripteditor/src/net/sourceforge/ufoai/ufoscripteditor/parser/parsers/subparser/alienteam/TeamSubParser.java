package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.alienteam;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class TeamSubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "team";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new TeamSubParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public TeamSubParser(IParserContext ctx) {
		super(ctx);
	}
}
