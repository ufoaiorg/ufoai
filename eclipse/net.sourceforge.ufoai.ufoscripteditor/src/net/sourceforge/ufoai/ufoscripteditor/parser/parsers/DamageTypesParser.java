package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;

public class DamageTypesParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new DamageTypesParser(ctx);
		}

		@Override
		public String getID() {
			return "damagetypes";
		}
	};

	public DamageTypesParser(IParserContext ctx) {
		super(ctx);
	}

	@Override
	public boolean hasOnlyValue() {
		return true;
	}
}
