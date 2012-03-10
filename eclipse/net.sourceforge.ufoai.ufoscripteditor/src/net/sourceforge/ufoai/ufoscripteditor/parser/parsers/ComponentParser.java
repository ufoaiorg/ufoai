package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class ComponentParser extends WindowParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new ComponentParser(ctx);
		}

		@Override
		public String getID() {
			return "component";
		}

		/**
		 * Components have a none standard layout.
		 */
		@Override
		public boolean isNameAfterID() {
			return false;
		}
	};

	@Override
	public IUFOSubParserFactory createSubParserFactory(final String id) {
		return new UFOSubParserFactoryAdapter() {
			@Override
			public IUFOSubParser create(IParserContext ctx) {
				return new WindowParser(ctx);
			}

			@Override
			public String getID() {
				return id;
			}
		};
	}

	public ComponentParser(IParserContext ctx) {
		super(ctx);
	}
}
