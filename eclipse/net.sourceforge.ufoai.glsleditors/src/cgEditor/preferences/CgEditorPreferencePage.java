package cgEditor.preferences;

import org.eclipse.jface.preference.ColorSelector;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.preference.PreferenceConverter;
import org.eclipse.jface.preference.PreferencePage;
import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.Token;
import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.List;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

import cgEditor.CgEditorPlugin;
import cgEditor.editors.TokenManager;

/**
 * This class represents a preference page that is contributed to the
 * Preferences dialog. By subclassing <samp>FieldEditorPreferencePage</samp>, we
 * can use the field support built into JFace that allows us to create a page
 * that is small and knows how to save, restore and apply itself.
 * <p>
 * This page is used to modify preferences only. They are stored in the
 * preference store that belongs to the main plug-in class. That way,
 * preferences can be accessed directly via the preference store.
 */

public class CgEditorPreferencePage extends PreferencePage implements
		IWorkbenchPreferencePage {
	private Button bold;
	private Button italic;
	private Button underline;
	private Button strikethrough;
	private ColorSelector color;
	private TextAttribute currentAttribute;
	private Token token;

	public CgEditorPreferencePage() {
		super();
		setPreferenceStore(CgEditorPlugin.getDefault().getPreferenceStore());
		setDescription("Cg Editor configuration");
	}

	@Override
	protected Control createContents(Composite parent) {
		Composite composite = new Composite(parent, SWT.NONE);
		composite.setLayout(new GridLayout(2, false));
		List list = new List(composite, SWT.NONE);

		list.setLayoutData(new GridData(GridData.FILL_BOTH));

		for (int i = 0; i < PreferenceConstants.PREFERENCES.length; i++) {
			list.add(PreferenceConstants.PREFERENCES[i][0]);
		}

		list.addSelectionListener(new SelectionListener() {

			public void widgetSelected(SelectionEvent e) {
				String str = ((List) e.getSource()).getSelection()[0];
				token = TokenManager.getToken(str);
				updateValues();
			}

			public void widgetDefaultSelected(SelectionEvent e) {
				widgetSelected(e);
			}
		});

		Composite compPref = new Composite(composite, SWT.NONE);
		GridData compPrefData = new GridData(GridData.FILL_BOTH);
		compPref.setLayoutData(compPrefData);
		compPref.setLayout(new GridLayout(2, false));

		Label label = new Label(compPref, SWT.NONE);
		GridData data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_END;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		label.setLayoutData(data);
		label.setText("Color :");
		color = new ColorSelector(compPref);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_BEGINNING;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;

		color.getButton().setLayoutData(data);
		color.addListener(new IPropertyChangeListener() {
			public void propertyChange(PropertyChangeEvent event) {
				RGB rgb = (RGB) event.getNewValue();
				if (token != null) {
					TextAttribute att = (TextAttribute) token.getData();
					TextAttribute att2 = new TextAttribute(TokenManager
							.getColor(rgb), null, att.getStyle());
					token.setData(att2);
				}
			}
		});

		bold = new Button(compPref, SWT.CHECK);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_END;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		bold.setLayoutData(data);
		bold.addSelectionListener(new SelectionListener() {
			public void widgetSelected(SelectionEvent e) {
				if (bold.getSelection()) {
					// On enleve la selection
					if (token != null) {
						int style = currentAttribute.getStyle() | SWT.BOLD;
						TextAttribute att = new TextAttribute(currentAttribute
								.getForeground(), currentAttribute
								.getBackground(), style);
						token.setData(att);
						currentAttribute = att;
					}
				} else {
					if (token != null) {
						int style = currentAttribute.getStyle() & ~SWT.BOLD;
						TextAttribute att = new TextAttribute(currentAttribute
								.getForeground(), currentAttribute
								.getBackground(), style);
						token.setData(att);
						currentAttribute = att;
					}
				}
			}

			public void widgetDefaultSelected(SelectionEvent e) {
				widgetSelected(e);
			}
		});
		label = new Label(compPref, SWT.NONE);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_BEGINNING;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		label.setLayoutData(data);
		label.setText("Bold");

		italic = new Button(compPref, SWT.CHECK);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_END;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		italic.setLayoutData(data);
		label = new Label(compPref, SWT.NONE);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_BEGINNING;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		label.setLayoutData(data);
		label.setText("Italic");
		italic.addSelectionListener(new SelectionListener() {
			public void widgetSelected(SelectionEvent e) {
				if (italic.getSelection()) {
					// On enleve la selection
					if (token != null) {
						int style = currentAttribute.getStyle() | SWT.ITALIC;
						TextAttribute att = new TextAttribute(currentAttribute
								.getForeground(), currentAttribute
								.getBackground(), style);
						token.setData(att);
						currentAttribute = att;
					}
				} else {
					if (token != null) {
						int style = currentAttribute.getStyle() & ~SWT.ITALIC;
						TextAttribute att = new TextAttribute(currentAttribute
								.getForeground(), currentAttribute
								.getBackground(), style);
						token.setData(att);
						currentAttribute = att;
					}
				}
			}

			public void widgetDefaultSelected(SelectionEvent e) {
				widgetSelected(e);
			}
		});

		strikethrough = new Button(compPref, SWT.CHECK);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_END;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		strikethrough.setLayoutData(data);
		label = new Label(compPref, SWT.NONE);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_BEGINNING;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		label.setLayoutData(data);
		label.setText("Strikethrough");
		strikethrough.addSelectionListener(new SelectionListener() {
			public void widgetSelected(SelectionEvent e) {
				if (strikethrough.getSelection()) {
					// On enleve la selection

					if (token != null) {
						int style = currentAttribute.getStyle()
								| TextAttribute.STRIKETHROUGH;
						TextAttribute att = new TextAttribute(currentAttribute
								.getForeground(), currentAttribute
								.getBackground(), style);
						token.setData(att);
						currentAttribute = att;
					}
				} else {

					if (token != null) {
						int style = currentAttribute.getStyle()
								& ~TextAttribute.STRIKETHROUGH;
						TextAttribute att = new TextAttribute(currentAttribute
								.getForeground(), currentAttribute
								.getBackground(), style);
						token.setData(att);
						currentAttribute = att;
					}
				}
			}

			public void widgetDefaultSelected(SelectionEvent e) {
				widgetSelected(e);
			}
		});

		underline = new Button(compPref, SWT.CHECK);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_END;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		underline.setLayoutData(data);
		label = new Label(compPref, SWT.NONE);
		data = new GridData();
		data.horizontalAlignment = GridData.HORIZONTAL_ALIGN_BEGINNING;
		data.verticalAlignment = GridData.VERTICAL_ALIGN_BEGINNING;
		label.setLayoutData(data);
		label.setText("Underline");
		underline.addSelectionListener(new SelectionListener() {
			public void widgetSelected(SelectionEvent e) {
				if (underline.getSelection()) {
					// On enleve la selection
					if (token != null) {
						int style = currentAttribute.getStyle()
								| TextAttribute.UNDERLINE;
						TextAttribute att = new TextAttribute(currentAttribute
								.getForeground(), currentAttribute
								.getBackground(), style);
						token.setData(att);
						currentAttribute = att;
					}
				} else {
					if (token != null) {
						int style = currentAttribute.getStyle()
								& ~TextAttribute.UNDERLINE;
						TextAttribute att = new TextAttribute(currentAttribute
								.getForeground(), currentAttribute
								.getBackground(), style);
						token.setData(att);
						currentAttribute = att;
					}
				}
			}

			public void widgetDefaultSelected(SelectionEvent e) {
				widgetSelected(e);
			}
		});

		composite.pack();
		return composite;
	}

	protected void updateValues() {
		currentAttribute = (TextAttribute) token.getData();
		color.setColorValue(currentAttribute.getForeground().getRGB());
		bold.setSelection((currentAttribute.getStyle() & SWT.BOLD) != 0);
		italic.setSelection((currentAttribute.getStyle() & SWT.ITALIC) != 0);
		underline
				.setSelection((currentAttribute.getStyle() & TextAttribute.UNDERLINE) != 0);
		strikethrough
				.setSelection((currentAttribute.getStyle() & TextAttribute.STRIKETHROUGH) != 0);
	}

	public void init(IWorkbench workbench) {
	}

	@Override
	protected IPreferenceStore doGetPreferenceStore() {
		return CgEditorPlugin.getDefault().getPreferenceStore();
	}

	protected void loadFromSavedData() {
		IPreferenceStore store = getPreferenceStore();

		for (int i = 0; i < PreferenceConstants.PREFERENCES.length; i++) {
			Token token = TokenManager
					.getToken(PreferenceConstants.PREFERENCES[i][0]);
			RGB rgb = PreferenceConverter.getColor(store,
					PreferenceConstants.PREFERENCES[i][1]);
			int style = store.getInt(PreferenceConstants.PREFERENCES[i][2]);
			TextAttribute att = new TextAttribute(TokenManager.getColor(rgb),
					null, style);
			token.setData(att);
		}
	}

	protected void saveData() {
		IPreferenceStore store = getPreferenceStore();

		for (int i = 0; i < PreferenceConstants.PREFERENCES.length; i++) {
			Token token = TokenManager
					.getToken(PreferenceConstants.PREFERENCES[i][0]);
			TextAttribute att = (TextAttribute) token.getData();
			PreferenceConverter.setValue(store,
					PreferenceConstants.PREFERENCES[i][1], att.getForeground()
							.getRGB());
			store.setValue(PreferenceConstants.PREFERENCES[i][2], att
					.getStyle());
		}
	}

	@Override
	public boolean performCancel() {
		loadFromSavedData();
		return true;
	}

	@Override
	public boolean performOk() {
		saveData();
		return super.performOk();
	}
}
