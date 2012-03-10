package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser;

import java.util.List;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.UFOParserRegistry;
import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptBlock;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.AbstractUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.UFOScriptTokenizer;

public abstract class AbstractUFOSubParser extends AbstractUFOParser implements
		IUFOSubParser {

	public AbstractUFOSubParser(IParserContext ctx) {
		super(ctx);
	}

	@Override
	public void parse(final String id, String name,
			final List<UFOScriptBlock> list, final UFOScriptTokenizer t,
			final AbstractUFOParser parentParser, final int line) {
		final int start = t.getLine();
		while (t.hasMoreElements()) {
			final String token = t.nextToken();
			if (t.isSeperator(token))
				continue;
			if (name == null)
				name = token;
			else {
				final IUFOParserFactory fac = getParserFactory(id, parentParser);
				final int startLine = line + t.getLine();
				final int lines = t.getLine() - start;
				if (fac != null) {
					final StringBuilder b = new StringBuilder();
					if (token.equals("{")) {
						String block = t.getBlock();
						b.append(block);
					}
					final UFOScriptBlock e = new UFOScriptBlock(id, name, fac,
							b.toString(), startLine, lines);
					list.add(e);
				}
				return;
			}
		}
	}

	private IUFOParserFactory getParserFactory(final String id,
			final AbstractUFOParser parentParser) {
		IUFOParserFactory subParser = parentParser.getSubParser(id);
		if (subParser != null)
			return subParser;
		return UFOParserRegistry.get().getFactory(id);
	}
}
