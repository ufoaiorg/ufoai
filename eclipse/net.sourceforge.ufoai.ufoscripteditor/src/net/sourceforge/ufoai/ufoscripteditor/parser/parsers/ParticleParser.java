package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.particle.InitParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.particle.PhysicsParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.particle.RunParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.particle.ThinkParser;

public class ParticleParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new ParticleParser(ctx);
		}

		@Override
		public String getID() {
			return "particle";
		}
	};

	public ParticleParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(InitParser.FACTORY, FACTORY);
		registerSubParser(RunParser.FACTORY, FACTORY);
		registerSubParser(ThinkParser.FACTORY, FACTORY);
		registerSubParser(PhysicsParser.FACTORY, FACTORY);
	}

	@Override
	public int getTokenCountPerEntry(String key) {
		// TODO: Some components have three tokens if used in special situations
		// - but can also have only two if used in another situation.
		return super.getTokenCountPerEntry(key);
	}
}
