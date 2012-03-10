package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.team;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class FemaleParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "female";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new FemaleParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public FemaleParser(IParserContext ctx) {
		super(ctx);
	}
}
