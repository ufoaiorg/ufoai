package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;

public interface IUFOSubParserFactory extends IUFOParserFactory {
	IUFOSubParser create(IParserContext ctx);

	void setParentFactory(IUFOParserFactory parentFactory);
}
