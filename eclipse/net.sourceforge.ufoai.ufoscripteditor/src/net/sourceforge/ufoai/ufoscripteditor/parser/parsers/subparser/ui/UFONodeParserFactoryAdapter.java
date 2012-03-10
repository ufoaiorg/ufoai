package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui;

import java.io.File;

import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.UFOParserFactoryAdapter;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

public abstract class UFONodeParserFactoryAdapter extends
		UFOParserFactoryAdapter implements IUFOSubParserFactory {

	private IUFOParserFactory parentFactory;

	@Override
	public String getIcon() {
		return "icons" + File.separatorChar + "ui" + File.separatorChar
				+ getIconID() + ".png";
	}

	@Override
	public void setParentFactory(IUFOParserFactory parentFactory) {
		this.parentFactory = parentFactory;
		if (parentFactory == null)
			throw new IllegalArgumentException("parentFactory may not be null");
	}

	public IUFOParserFactory getParentFactory() {
		return parentFactory;
	}

}
