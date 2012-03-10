package net.sourceforge.ufoai.ufoscripteditor.parser;

import java.util.HashMap;

import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.AircraftParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.AlienTeamParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.BaseParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.BaseTemplateParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.BuildingParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.CampaignParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.CityParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.ComponentParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.ComponentsParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.CraftItemParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.DamageTypesParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.EntityParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.EquipmentParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.EventsParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.FontParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.GametypeParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.IconParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.InstallationParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.InventoryParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.ItemParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.LanguageParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.MailParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.MapdefParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.MedalParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.MenuModelParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.MessageCategoryParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.MessageOptionsParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.MusicParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.NationParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.ParticleParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.RankParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.ResearchableParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.ResearchedParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.SequenceParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.TeamParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.TechnologyParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.TerrainParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.TileParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.TipsParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.TutorialParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.WindowParser;

public class UFOParserRegistry {
	private static final UFOParserRegistry registry = new UFOParserRegistry();
	private final HashMap<String, IUFOParserFactory> parsers = new HashMap<String, IUFOParserFactory>();

	private UFOParserRegistry() {
		register(AlienTeamParser.FACTORY);
		register(AircraftParser.FACTORY);
		register(BaseParser.FACTORY);
		register(BaseTemplateParser.FACTORY);
		register(BuildingParser.FACTORY);
		register(CampaignParser.FACTORY);
		register(CityParser.FACTORY);
		register(ComponentParser.FACTORY);
		register(ComponentsParser.FACTORY);
		register(CraftItemParser.FACTORY);
		register(DamageTypesParser.FACTORY);
		register(EntityParser.FACTORY);
		register(EquipmentParser.FACTORY);
		register(EventsParser.FACTORY);
		register(FontParser.FACTORY);
		register(GametypeParser.FACTORY);
		register(IconParser.FACTORY);
		register(InstallationParser.FACTORY);
		register(InventoryParser.FACTORY);
		register(ItemParser.FACTORY);
		register(LanguageParser.FACTORY);
		register(MailParser.FACTORY);
		register(MapdefParser.FACTORY);
		register(MedalParser.FACTORY);
		register(MenuModelParser.FACTORY);
		register(MessageCategoryParser.FACTORY);
		register(MessageOptionsParser.FACTORY);
		register(MusicParser.FACTORY);
		register(NationParser.FACTORY);
		register(ParticleParser.FACTORY);
		register(SequenceParser.FACTORY);
		register(RankParser.FACTORY);
		register(ResearchedParser.FACTORY);
		register(ResearchableParser.FACTORY);
		register(TeamParser.FACTORY);
		register(TechnologyParser.FACTORY);
		register(TerrainParser.FACTORY);
		register(TileParser.FACTORY);
		register(TipsParser.FACTORY);
		register(TutorialParser.FACTORY);
		register(WindowParser.FACTORY);
	}

	public static UFOParserRegistry get() {
		return registry;
	}

	public void register(IUFOParserFactory factory) {
		if (parsers.containsKey(factory.getID().toLowerCase()))
			throw new RuntimeException("Parser for " + factory.getID()
					+ " is already registered");
		parsers.put(factory.getID().toLowerCase(), factory);
	}

	public IUFOParser getParser(final String key, IParserContext ctx) {
		IUFOParserFactory factory = getFactory(key);
		if (factory == null)
			throw new RuntimeException("No factory for " + key + " registered");
		return factory.create(ctx);
	}

	public IUFOParserFactory getFactory(final String key) {
		return parsers.get(key.toLowerCase());
	}
}
