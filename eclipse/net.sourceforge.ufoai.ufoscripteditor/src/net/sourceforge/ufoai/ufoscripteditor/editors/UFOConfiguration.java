package net.sourceforge.ufoai.ufoscripteditor.editors;

import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.presentation.IPresentationReconciler;
import org.eclipse.jface.text.presentation.PresentationReconciler;
import org.eclipse.jface.text.rules.DefaultDamagerRepairer;
import org.eclipse.jface.text.rules.Token;
import org.eclipse.jface.text.source.ISourceViewer;
import org.eclipse.jface.text.source.SourceViewerConfiguration;

public class UFOConfiguration extends SourceViewerConfiguration {
	private UFOScanner scanner;
	private ColorManager colorManager;

	public UFOConfiguration(ColorManager colorManager) {
		this.colorManager = colorManager;
	}

	public String[] getConfiguredContentTypes(ISourceViewer sourceViewer) {
		return new String[] { IDocument.DEFAULT_CONTENT_TYPE,
				UFOPartitionScanner.UFO_COMMENT, UFOPartitionScanner.UFO_SCRIPT };
	}

	protected UFOScanner getUFOScanner() {
		if (scanner == null) {
			scanner = new UFOScanner(colorManager);
			scanner.setDefaultReturnToken(new Token(new TextAttribute(
					colorManager.getColor(IUFOColorConstants.DEFAULT))));
		}
		return scanner;
	}

	public IPresentationReconciler getPresentationReconciler(
			ISourceViewer sourceViewer) {
		PresentationReconciler reconciler = new PresentationReconciler();

		DefaultDamagerRepairer dr = new DefaultDamagerRepairer(getUFOScanner());
		reconciler.setDamager(dr, IDocument.DEFAULT_CONTENT_TYPE);
		reconciler.setRepairer(dr, IDocument.DEFAULT_CONTENT_TYPE);

		NonRuleBasedDamagerRepairer ndr = new NonRuleBasedDamagerRepairer(
				new TextAttribute(colorManager
						.getColor(IUFOColorConstants.COMMENT)));
		reconciler.setDamager(ndr, UFOPartitionScanner.UFO_COMMENT);
		reconciler.setRepairer(ndr, UFOPartitionScanner.UFO_COMMENT);

		return reconciler;
	}

}
