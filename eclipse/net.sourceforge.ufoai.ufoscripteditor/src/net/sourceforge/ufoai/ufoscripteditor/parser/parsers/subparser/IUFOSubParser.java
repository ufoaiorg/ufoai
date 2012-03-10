package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser;

import java.util.List;

import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptBlock;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.AbstractUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.UFOScriptTokenizer;

public interface IUFOSubParser extends IUFOParser {
	void parse(String id, String name, List<UFOScriptBlock> list,
			UFOScriptTokenizer t, AbstractUFOParser parentParser, int line);
}
