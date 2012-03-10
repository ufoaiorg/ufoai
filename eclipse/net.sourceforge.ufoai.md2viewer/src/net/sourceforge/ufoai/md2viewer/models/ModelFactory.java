package net.sourceforge.ufoai.md2viewer.models;

import java.util.HashMap;

import net.sourceforge.ufoai.md2viewer.models.md2.MD2Model;
import net.sourceforge.ufoai.md2viewer.models.md2.MD2Tag;

public class ModelFactory {
	private final HashMap<String, IModelFactory> map = new HashMap<String, IModelFactory>();
	private static ModelFactory fac;

	private ModelFactory() {
		register(MD2Model.FACTORY);
		register(MD2Tag.FACTORY);
	}

	public void register(IModelFactory factory) {
		String extension = factory.getExtension();
		if (map.get(extension) != null)
			throw new RuntimeException("factory for " + extension
					+ " is already registered");
		map.put(extension, factory);
	}

	public IModel get(String extension) {
		IModelFactory modelFactory = map.get(extension);
		if (modelFactory == null)
			throw new RuntimeException("unknown factory type requested");

		return modelFactory.create();
	}

	public static synchronized ModelFactory get() {
		if (fac == null)
			fac = new ModelFactory();

		return fac;
	}
}
