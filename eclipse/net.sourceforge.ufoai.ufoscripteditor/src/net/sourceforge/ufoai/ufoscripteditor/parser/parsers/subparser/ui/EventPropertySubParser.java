package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;

public class EventPropertySubParser extends AbstractUFOSubParser {
	private static Map<String, IUFOSubParserFactory> factories = new HashMap<String, IUFOSubParserFactory>();;
	public static final IUFOSubParserFactory ONCLICK = getFactory("onClick");
	public static final IUFOSubParserFactory ONCHANGE = getFactory("onChange");
	public static final IUFOSubParserFactory ONMOUSEENTER = getFactory("onMouseEnter");
	public static final IUFOSubParserFactory ONMOUSELEAVE = getFactory("onMouseLeave");
	public static final IUFOSubParserFactory ONMCLICK = getFactory("onMClick");
	public static final IUFOSubParserFactory ONRCLICK = getFactory("onRClick");
	public static final IUFOSubParserFactory ONWHEEL = getFactory("onWheel");
	public static final IUFOSubParserFactory ONWHEELDOWN = getFactory("onWheelDown");
	public static final IUFOSubParserFactory ONWHEELUP = getFactory("onWheelUp");
	public static final IUFOSubParserFactory ONVIEWCHANGE = getFactory("onViewChange");
	public static final IUFOSubParserFactory ONSELECT = getFactory("onSelect");
	public static final IUFOSubParserFactory ONABORT = getFactory("onAbort");
	public static final IUFOSubParserFactory ONCLOSE = getFactory("onClose");
	public static final IUFOSubParserFactory ONINIT = getFactory("onInit");
	public static final IUFOSubParserFactory ONEVENT = getFactory("onEvent");

	static private IUFOSubParserFactory createFactory(String eventName) {
		final String name = eventName;
		IUFOSubParserFactory factory = new UFOSubParserFactoryAdapter() {
			@Override
			public String getID() {
				return name;
			}

			@Override
			public IUFOSubParser create(IParserContext ctx) {
				return new EventPropertySubParser(ctx);
			}

			@Override
			public String getIcon() {
				return "icons" + File.separatorChar + "ui" + File.separatorChar
						+ "property-event.png";
			}

			@Override
			public boolean isIDName() {
				return true;
			}
		};
		return factory;
	}

	public EventPropertySubParser(IParserContext ctx) {
		super(ctx);
	}

	static public IUFOSubParserFactory getFactory(String eventName) {
		IUFOSubParserFactory factory = factories.get(eventName.toLowerCase());
		if (factory == null) {
			factory = createFactory(eventName);
			factories.put(eventName.toLowerCase(), factory);
		}
		return factory;
	}

}
