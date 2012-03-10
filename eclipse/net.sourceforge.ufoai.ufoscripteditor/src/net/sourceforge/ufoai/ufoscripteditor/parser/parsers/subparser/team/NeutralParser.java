package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.team;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class NeutralParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "neutral";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new NeutralParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public NeutralParser(IParserContext ctx) {
		super(ctx);
	}
}
