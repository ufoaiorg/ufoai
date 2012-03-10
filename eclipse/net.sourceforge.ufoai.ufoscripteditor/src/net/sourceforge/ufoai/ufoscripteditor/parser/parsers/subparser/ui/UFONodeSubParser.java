package net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

public abstract class UFONodeSubParser extends AbstractUFOSubParser {

	/**
	 * Provide a set of available event property parsers
	 *
	 * @return An array of event property parsers
	 */
	public IUFOSubParserFactory[] getEventPropertyParsers() {
		return new IUFOSubParserFactory[] {};
	}

	public abstract IUFOSubParserFactory getNodeSubParserFactory();

	public static void registerSubNodeParser(IUFOSubParserFactory factory) {

	}

	public UFONodeSubParser(IParserContext ctx) {
		super(ctx);
		IUFOSubParserFactory factory = getNodeSubParserFactory();
		registerSubParser(AbstractBaseSubParser.FACTORY, factory);
		registerSubParser(AbstractNodeSubParser.FACTORY, factory);
		registerSubParser(AbstractOptionSubParser.FACTORY, factory);
		registerSubParser(AbstractScrollableSubParser.FACTORY, factory);
		registerSubParser(AbstractScrollbarSubParser.FACTORY, factory);
		registerSubParser(AbstractValueSubParser.FACTORY, factory);
		registerSubParser(BarSubParser.FACTORY, factory);
		registerSubParser(BaseLayoutSubParser.FACTORY, factory);
		registerSubParser(BaseMapSubParser.FACTORY, factory);
		registerSubParser(ButtonSubParser.FACTORY, factory);
		registerSubParser(CheckboxSubParser.FACTORY, factory);
		registerSubParser(CinematicSubParser.FACTORY, factory);
		registerSubParser(ConfuncSubParser.FACTORY, factory);
		registerSubParser(ContainerSubParser.FACTORY, factory);
		registerSubParser(ControlsSubParser.FACTORY, factory);
		registerSubParser(CustomButtonSubParser.FACTORY, factory);
		registerSubParser(CvarfuncSubParser.FACTORY, factory);
		registerSubParser(EditorSubParser.FACTORY, factory);
		registerSubParser(EkgSubParser.FACTORY, factory);
		registerSubParser(FuncSubParser.FACTORY, factory);
		registerSubParser(ItemSubParser.FACTORY, factory);
		registerSubParser(KeyBindingSubParser.FACTORY, factory);
		registerSubParser(LineChartSubParser.FACTORY, factory);
		registerSubParser(MapSubParser.FACTORY, factory);
		registerSubParser(MaterialEditorSubParser.FACTORY, factory);
		registerSubParser(MessagelistSubParser.FACTORY, factory);
		registerSubParser(ModelSubParser.FACTORY, factory);
		registerSubParser(OptionListSubParser.FACTORY, factory);
		registerSubParser(OptionSubParser.FACTORY, factory);
		registerSubParser(OptionTreeSubParser.FACTORY, factory);
		registerSubParser(PanelSubParser.FACTORY, factory);
		registerSubParser(PicSubParser.FACTORY, factory);
		registerSubParser(RadarSubParser.FACTORY, factory);
		registerSubParser(RadioButtonSubParser.FACTORY, factory);
		registerSubParser(RowsSubParser.FACTORY, factory);
		registerSubParser(SelectBoxSubParser.FACTORY, factory);
		registerSubParser(SpecialSubParser.FACTORY, factory);
		registerSubParser(SpinnerSubParser.FACTORY, factory);
		registerSubParser(StringSubParser.FACTORY, factory);
		registerSubParser(TabSubParser.FACTORY, factory);
		registerSubParser(TBarSubParser.FACTORY, factory);
		registerSubParser(TextSubParser.FACTORY, factory);
		registerSubParser(TextEntrySubParser.FACTORY, factory);
		registerSubParser(TextListSubParser.FACTORY, factory);
		registerSubParser(TodoSubParser.FACTORY, factory);
		registerSubParser(VScrollBarSubParser.FACTORY, factory);
		registerSubParser(ZoneSubParser.FACTORY, factory);

		// Special properties
		registerSubParser(ExcludeRectSubParser.FACTORY, factory);
	}

}
