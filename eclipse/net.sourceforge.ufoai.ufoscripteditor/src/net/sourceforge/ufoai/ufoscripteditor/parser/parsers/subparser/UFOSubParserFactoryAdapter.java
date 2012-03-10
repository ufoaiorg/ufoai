package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser;

import java.io.File;

import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.UFOParserFactoryAdapter;

public abstract class UFOSubParserFactoryAdapter extends
		UFOParserFactoryAdapter implements IUFOSubParserFactory {
	private IUFOParserFactory parentFactory;

	@Override
	public String getIcon() {
		return "icons" + File.separatorChar + parentFactory.getID()
				+ File.separatorChar + getID() + ".png";
	}

	@Override
	public void setParentFactory(IUFOParserFactory parentFactory) {
		if (parentFactory == null)
			throw new IllegalArgumentException("parentFactory may not be null");
		this.parentFactory = parentFactory;
	}

	public IUFOParserFactory getParentFactory() {
		return parentFactory;
	}
}
