package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.mapdef;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class UFOsSubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "ufos";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new UFOsSubParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public UFOsSubParser(IParserContext ctx) {
		super(ctx);
	}
}
