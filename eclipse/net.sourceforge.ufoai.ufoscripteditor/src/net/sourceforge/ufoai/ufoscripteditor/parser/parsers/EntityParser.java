package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.entity.DefaultSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.entity.MandatorySubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.entity.OptionalSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.entity.RangeSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.entity.TypeSubParser;

public class EntityParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new EntityParser(ctx);
		}

		@Override
		public String getID() {
			return "entity";
		}
	};

	public EntityParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(DefaultSubParser.FACTORY, FACTORY);
		registerSubParser(MandatorySubParser.FACTORY, FACTORY);
		registerSubParser(OptionalSubParser.FACTORY, FACTORY);
		registerSubParser(RangeSubParser.FACTORY, FACTORY);
		registerSubParser(TypeSubParser.FACTORY, FACTORY);
	}
}
