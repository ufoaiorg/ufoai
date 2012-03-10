package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.alienteam;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class CategorySubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "category";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new CategorySubParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public CategorySubParser(IParserContext ctx) {
		super(ctx);
	}

	@Override
	public boolean hasOnlyValue() {
		return true;
	}
}
