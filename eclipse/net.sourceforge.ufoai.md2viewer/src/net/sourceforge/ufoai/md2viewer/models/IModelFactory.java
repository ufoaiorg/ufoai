package net.sourceforge.ufoai.md2viewer.models;

public interface IModelFactory {
	IModel create();

	String getExtension();
}
