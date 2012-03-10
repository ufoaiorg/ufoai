package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.sequence;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class PrecacheParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "precache";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new PrecacheParser(ctx);
		}
	};

	public PrecacheParser(IParserContext ctx) {
		super(ctx);
	}

	@Override
	public boolean hasOnlyValue() {
		return true;
	}
}
