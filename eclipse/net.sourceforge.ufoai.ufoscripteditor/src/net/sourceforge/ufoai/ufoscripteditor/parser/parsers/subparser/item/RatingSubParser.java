package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.item;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class RatingSubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "rating";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new RatingSubParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public RatingSubParser(IParserContext ctx) {
		super(ctx);
	}
}
