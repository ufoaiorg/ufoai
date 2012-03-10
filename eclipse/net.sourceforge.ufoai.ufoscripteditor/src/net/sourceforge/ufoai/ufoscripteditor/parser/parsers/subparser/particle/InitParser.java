package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.particle;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class InitParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "init";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new InitParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public InitParser(IParserContext ctx) {
		super(ctx);
	}
}
