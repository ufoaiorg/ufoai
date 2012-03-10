package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.sequence;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class ModelParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "model";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new ModelParser(ctx);
		}
	};

	public ModelParser(IParserContext ctx) {
		super(ctx);
	}
}
