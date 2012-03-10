package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;

public class EquipmentParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new EquipmentParser(ctx);
		}

		@Override
		public String getID() {
			return "equipment";
		}
	};

	public EquipmentParser(IParserContext ctx) {
		super(ctx);
	}

	@Override
	public int getTokenCountPerEntry(String key) {
		if (key.equalsIgnoreCase("mininterest")
				|| key.equalsIgnoreCase("maxinterest"))
			return 2;
		return 3;
	}
}
