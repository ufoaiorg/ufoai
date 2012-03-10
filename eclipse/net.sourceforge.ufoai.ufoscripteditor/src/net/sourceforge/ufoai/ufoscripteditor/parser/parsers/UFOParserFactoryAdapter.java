package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import java.io.File;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;

public abstract class UFOParserFactoryAdapter implements IUFOParserFactory {
	@Override
	public abstract IUFOParser create(IParserContext ctx);

	@Override
	public abstract String getID();

	@Override
	public String getIcon() {
		return "icons" + File.separatorChar + getID() + ".png";
	}

	@Override
	public boolean isNameAfterID() {
		return true;
	}

	@Override
	public boolean isIDName() {
		return false;
	}

	protected String getIconID() {
		return getID();
	}
}
