package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.item;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class ProtectionSubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "protection";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new ProtectionSubParser(ctx);
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public ProtectionSubParser(IParserContext ctx) {
		super(ctx);
	}
}
