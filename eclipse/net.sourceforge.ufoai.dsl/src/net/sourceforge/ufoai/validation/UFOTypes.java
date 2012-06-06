package net.sourceforge.ufoai.validation;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

import net.sourceforge.ufoai.UFOAIPlugin;

public class UFOTypes {

	private static UFOTypes instance;
	private static String defaultPropertyFile = "/rules.properties";

	private final Properties properties;
	private final Map<String, UFOType> types;

	private UFOTypes(Properties properties) {
		this.properties = new Properties(properties);
		this.types = new HashMap<String, UFOType>();
	}

	public static UFOTypes getInstance() {
		if (instance == null) {
			Properties properties = new Properties();
			InputStream stream = UFOAIPlugin.class.getResourceAsStream(defaultPropertyFile);
			if (stream == null) {
				System.out.println("UFOAI Validator property file \"" + defaultPropertyFile + "\" not found");
			} else {
				try {
					properties.load(stream);
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
			instance = new UFOTypes(properties);
		}
		return instance;
	}

	private UFOType computePathType(String path) {
		String type = properties.getProperty(path);
		if (type == null)
			return null;
		String values = properties.getProperty(path + ".VALUES");
		return new UFOType(type, values);
	}

	public UFOType getPathType(String path) {
		UFOType type = types.get(path);
		if (type == null) {
			type = computePathType(path);
			types.put(path, type);
		}
		return type;
	}
}
