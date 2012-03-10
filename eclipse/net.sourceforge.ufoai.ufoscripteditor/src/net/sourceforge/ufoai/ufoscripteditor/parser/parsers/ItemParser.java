package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.item.ProtectionSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.item.RatingSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.item.WeaponModSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.validators.ImageValidator;

public class ItemParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new ItemParser(ctx);
		}

		@Override
		public String getID() {
			return "item";
		}
	};

	public ItemParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(WeaponModSubParser.FACTORY, FACTORY);
		registerSubParser(RatingSubParser.FACTORY, FACTORY);
		registerSubParser(ProtectionSubParser.FACTORY, FACTORY);

		registerValidator(new ImageValidator());
	}
}
