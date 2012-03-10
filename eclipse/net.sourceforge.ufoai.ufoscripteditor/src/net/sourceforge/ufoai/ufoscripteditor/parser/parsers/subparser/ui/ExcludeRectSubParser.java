package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui;

import java.io.File;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

public class ExcludeRectSubParser extends AbstractUFOSubParser {
	public static final IUFOSubParserFactory FACTORY = new UFONodeParserFactoryAdapter() {
		@Override
		public String getID() {
			return "excluderect";
		}

		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new ExcludeRectSubParser(ctx);
		}

		@Override
		public String getIcon() {
			return "icons" + File.separatorChar + "ui" + File.separatorChar
					+ "property-excludrect.png";
		}

		@Override
		public boolean isIDName() {
			return true;
		}
	};

	public ExcludeRectSubParser(IParserContext ctx) {
		super(ctx);
	}
}
