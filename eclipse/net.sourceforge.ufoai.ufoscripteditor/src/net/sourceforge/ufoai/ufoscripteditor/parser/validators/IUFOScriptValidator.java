package net.sourceforge.ufoai.ufoscripteditor.parser.validators;

import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptBlock;

public interface IUFOScriptValidator {
	void validate(UFOScriptBlock block) throws UFOScriptValidateException;

	String getID();
}
