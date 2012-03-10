package net.sourceforge.ufoai.ufoscripteditor.views.outline;

import java.io.IOException;
import java.util.List;

import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptBlock;
import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;

import org.eclipse.jface.text.BadPositionCategoryException;
import org.eclipse.jface.text.DefaultPositionUpdater;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.IPositionUpdater;
import org.eclipse.jface.text.source.IAnnotationModel;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.ui.texteditor.IDocumentProvider;

public class UFOContentProvider implements ITreeContentProvider {
	private final IDocumentProvider documentProvider;
	private final static String TAG_POSITIONS = "__tag_positions";
	private final IPositionUpdater positionUpdater = new DefaultPositionUpdater(
			TAG_POSITIONS);
	private UFOScriptParser ufoScriptParser;

	public UFOContentProvider(final IDocumentProvider provider) {
		super();
		this.documentProvider = provider;
	}

	public Object[] getChildren(final Object parentElement) {
		final UFOScriptBlock block = (UFOScriptBlock) parentElement;
		final IUFOParserFactory parserFactory = block.getParser();
		if (parserFactory == null)
			return new Object[0];
		final IUFOParser parser = parserFactory.create(ufoScriptParser
				.getParserContext());
		IUFOSubParserFactory innerParserFactory = parser
				.createSubParserFactory(block.getName());
		if (innerParserFactory != null)
			parser.registerSubParser(innerParserFactory, parserFactory);

		final List<UFOScriptBlock> inner = parser.getInnerScriptBlocks(block
				.getData(), block.getStartLine());
		return inner.toArray();
	}

	public Object getParent(final Object element) {
		return null;
	}

	public boolean hasChildren(final Object element) {
		final UFOScriptBlock block = (UFOScriptBlock) element;
		return block.hasChildren();
	}

	public Object[] getElements(final Object inputElement) {
		try {
			final IDocument doc = documentProvider.getDocument(inputElement);
			final String string = doc.get();
			final IAnnotationModel annotationModel = documentProvider
					.getAnnotationModel(inputElement);
			ufoScriptParser = new UFOScriptParser(doc, annotationModel, string);
			final List<UFOScriptBlock> blocks = ufoScriptParser.getBlocks();
			return blocks.toArray();
		} catch (final IOException e) {
			return new Object[0];
		}
	}

	public void dispose() {
	}

	public void inputChanged(final Viewer viewer, final Object oldInput,
			final Object newInput) {
		if (oldInput != null) {
			final IDocument document = documentProvider.getDocument(oldInput);
			if (document != null) {
				try {
					document.removePositionCategory(TAG_POSITIONS);
				} catch (final BadPositionCategoryException x) {
				}
				document.removePositionUpdater(positionUpdater);
			}
		}

		if (newInput != null) {
			final IDocument document = documentProvider.getDocument(newInput);
			if (document != null) {
				document.addPositionCategory(TAG_POSITIONS);
				document.addPositionUpdater(positionUpdater);
			}
		}
	}
}
