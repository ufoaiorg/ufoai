package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.campaign.SalarySubParser;

public class CampaignParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new CampaignParser(ctx);
		}

		@Override
		public String getID() {
			return "campaign";
		}
	};

	public CampaignParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(SalarySubParser.FACTORY, FACTORY);
	}
}
