package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.item;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class WeaponModSubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public String getID() {
			return "weapon_mod";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new WeaponModSubParser(ctx);
		}
	};

	public WeaponModSubParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(FireDefSubParser.FACTORY, FACTORY);
	}
}
