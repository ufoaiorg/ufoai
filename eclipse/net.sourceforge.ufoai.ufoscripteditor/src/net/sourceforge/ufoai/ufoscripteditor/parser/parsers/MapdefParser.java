package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.mapdef.AircraftSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.mapdef.CulturesSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.mapdef.PopulationsSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.mapdef.TerrainsSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.mapdef.UFOsSubParser;

public class MapdefParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new MapdefParser(ctx);
		}

		@Override
		public String getID() {
			return "mapdef";
		}
	};

	public MapdefParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(AircraftSubParser.FACTORY, FACTORY);
		registerSubParser(UFOsSubParser.FACTORY, FACTORY);
		registerSubParser(TerrainsSubParser.FACTORY, FACTORY);
		registerSubParser(CulturesSubParser.FACTORY, FACTORY);
		registerSubParser(PopulationsSubParser.FACTORY, FACTORY);
	}
}
