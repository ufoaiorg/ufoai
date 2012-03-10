package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.entity;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class TypeSubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "type";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new TypeSubParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public TypeSubParser(IParserContext ctx) {
		super(ctx);
	}
}
