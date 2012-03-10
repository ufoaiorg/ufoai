package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.entity;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class MandatorySubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "mandatory";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new MandatorySubParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public MandatorySubParser(IParserContext ctx) {
		super(ctx);
	}
}
