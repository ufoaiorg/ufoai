package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.sequence;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class Obj2DParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "2dobj";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new Obj2DParser(ctx);
		}
	};

	public Obj2DParser(IParserContext ctx) {
		super(ctx);
	}
}
