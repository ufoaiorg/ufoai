package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.AbstractUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.UFOSubParserFactoryAdapter;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.AbstractBaseSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.AbstractNodeSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.AbstractOptionSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.AbstractScrollableSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.AbstractScrollbarSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.AbstractValueSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.BarSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.BaseLayoutSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.BaseMapSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.ButtonSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.CheckboxSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.CinematicSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.ConfuncSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.ContainerSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.ControlsSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.CustomButtonSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.CvarfuncSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.EditorSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.EkgSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.EventPropertySubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.FuncSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.ItemSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.KeyBindingSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.LineChartSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.MapSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.MaterialEditorSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.MessagelistSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.ModelSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.OptionListSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.OptionTreeSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.PanelSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.PicSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.RadarSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.RadioButtonSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.RowsSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.SelectBoxSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.SpecialSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.SpinnerSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.StringSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.TBarSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.TabSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.TextEntrySubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.TextListSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.TextSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.TodoSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.VScrollBarSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.ui.ZoneSubParser;

public class WindowParser extends AbstractUFOSubParser {
	public static final IUFOParserFactory FACTORY = new UFOParserFactoryAdapter() {
		@Override
		public IUFOParser create(IParserContext ctx) {
			return new WindowParser(ctx);
		}

		@Override
		public String getID() {
			return "window";
		}
	};

	public static final IUFOSubParserFactory SUBFACTORY = new UFOSubParserFactoryAdapter() {
		@Override
		public IUFOSubParser create(IParserContext ctx) {
			return new WindowParser(ctx);
		}

		@Override
		public String getID() {
			return "window";
		}
	};

	public WindowParser(IParserContext ctx) {
		super(ctx);
		registerSubParser(AbstractBaseSubParser.FACTORY, FACTORY);
		registerSubParser(AbstractNodeSubParser.FACTORY, FACTORY);
		registerSubParser(AbstractOptionSubParser.FACTORY, FACTORY);
		registerSubParser(AbstractScrollableSubParser.FACTORY, FACTORY);
		registerSubParser(AbstractScrollbarSubParser.FACTORY, FACTORY);
		registerSubParser(AbstractValueSubParser.FACTORY, FACTORY);
		registerSubParser(BarSubParser.FACTORY, FACTORY);
		registerSubParser(BaseLayoutSubParser.FACTORY, FACTORY);
		registerSubParser(BaseMapSubParser.FACTORY, FACTORY);
		registerSubParser(ButtonSubParser.FACTORY, FACTORY);
		registerSubParser(CheckboxSubParser.FACTORY, FACTORY);
		registerSubParser(CinematicSubParser.FACTORY, FACTORY);
		registerSubParser(ConfuncSubParser.FACTORY, FACTORY);
		registerSubParser(ContainerSubParser.FACTORY, FACTORY);
		registerSubParser(ControlsSubParser.FACTORY, FACTORY);
		registerSubParser(CustomButtonSubParser.FACTORY, FACTORY);
		registerSubParser(CvarfuncSubParser.FACTORY, FACTORY);
		registerSubParser(EditorSubParser.FACTORY, FACTORY);
		registerSubParser(EkgSubParser.FACTORY, FACTORY);
		registerSubParser(FuncSubParser.FACTORY, FACTORY);
		registerSubParser(ItemSubParser.FACTORY, FACTORY);
		registerSubParser(KeyBindingSubParser.FACTORY, FACTORY);
		registerSubParser(LineChartSubParser.FACTORY, FACTORY);
		registerSubParser(MapSubParser.FACTORY, FACTORY);
		registerSubParser(MaterialEditorSubParser.FACTORY, FACTORY);
		registerSubParser(MessagelistSubParser.FACTORY, FACTORY);
		registerSubParser(ModelSubParser.FACTORY, FACTORY);
		registerSubParser(OptionListSubParser.FACTORY, FACTORY);
		registerSubParser(OptionTreeSubParser.FACTORY, FACTORY);
		registerSubParser(PanelSubParser.FACTORY, FACTORY);
		registerSubParser(PicSubParser.FACTORY, FACTORY);
		registerSubParser(RadarSubParser.FACTORY, FACTORY);
		registerSubParser(RadioButtonSubParser.FACTORY, FACTORY);
		registerSubParser(RowsSubParser.FACTORY, FACTORY);
		registerSubParser(SelectBoxSubParser.FACTORY, FACTORY);
		registerSubParser(SpecialSubParser.FACTORY, FACTORY);
		registerSubParser(SpinnerSubParser.FACTORY, FACTORY);
		registerSubParser(StringSubParser.FACTORY, FACTORY);
		registerSubParser(TabSubParser.FACTORY, FACTORY);
		registerSubParser(TBarSubParser.FACTORY, FACTORY);
		registerSubParser(TextSubParser.FACTORY, FACTORY);
		registerSubParser(TextEntrySubParser.FACTORY, FACTORY);
		registerSubParser(TextListSubParser.FACTORY, FACTORY);
		registerSubParser(TodoSubParser.FACTORY, FACTORY);
		registerSubParser(VScrollBarSubParser.FACTORY, FACTORY);
		registerSubParser(ZoneSubParser.FACTORY, FACTORY);

		registerSubParser(EventPropertySubParser.ONCLOSE, FACTORY);
		registerSubParser(EventPropertySubParser.ONEVENT, FACTORY);
		registerSubParser(EventPropertySubParser.ONINIT, FACTORY);
		registerSubParser(EventPropertySubParser.ONCHANGE, FACTORY);
		registerSubParser(EventPropertySubParser.ONCLICK, FACTORY);
		registerSubParser(EventPropertySubParser.ONMCLICK, FACTORY);
		registerSubParser(EventPropertySubParser.ONMOUSEENTER, FACTORY);
		registerSubParser(EventPropertySubParser.ONMOUSELEAVE, FACTORY);
		registerSubParser(EventPropertySubParser.ONRCLICK, FACTORY);
		registerSubParser(EventPropertySubParser.ONWHEEL, FACTORY);
		registerSubParser(EventPropertySubParser.ONWHEELDOWN, FACTORY);
		registerSubParser(EventPropertySubParser.ONWHEELUP, FACTORY);
	}
}
