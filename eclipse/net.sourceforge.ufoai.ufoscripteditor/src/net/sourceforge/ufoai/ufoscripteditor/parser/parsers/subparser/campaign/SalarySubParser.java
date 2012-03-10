package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.campaign;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class SalarySubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "salary";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new SalarySubParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public SalarySubParser(IParserContext ctx) {
		super(ctx);
	}
}
