package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.sequence.CameraParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.sequence.ModelParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.sequence.Obj2DParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.sequence.PrecacheParser;

public class SequenceParser extends AbstractUFOParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new SequenceParser(ctx);
		}

		@Override
		public String getID() {
			return "sequence";
		}
	};

	public SequenceParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(CameraParser.FACTORY, FACTORY);
		registerSubParser(PrecacheParser.FACTORY, FACTORY);
		registerSubParser(ModelParser.FACTORY, FACTORY);
		registerSubParser(Obj2DParser.FACTORY, FACTORY);
	}
}
