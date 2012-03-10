package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.tech;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class PreDescriptionParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "pre_description";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new PreDescriptionParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public PreDescriptionParser(IParserContext ctx) {
		super(ctx);
	}
}
