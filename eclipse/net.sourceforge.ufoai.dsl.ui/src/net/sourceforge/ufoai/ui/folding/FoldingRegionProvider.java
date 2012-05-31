package net.sourceforge.ufoai.ui.folding;

import net.sourceforge.ufoai.ufoScript.UFONode;

import org.eclipse.emf.ecore.EObject;
import org.eclipse.xtext.resource.ILocationInFileProvider;
import org.eclipse.xtext.ui.editor.folding.DefaultFoldingRegionProvider;
import org.eclipse.xtext.ui.editor.folding.IFoldingRegionAcceptor;
import org.eclipse.xtext.util.ITextRegion;

import com.google.inject.Inject;

public class FoldingRegionProvider extends DefaultFoldingRegionProvider {
	@Inject
	private ILocationInFileProvider locationInFileProvider;

	@Override
	protected boolean isHandled(EObject eObject) {
		if (eObject instanceof UFONode) {
			return true;
		}
		return super.isHandled(eObject);
	}

	@Override
	protected void computeObjectFolding(EObject eObject, IFoldingRegionAcceptor<ITextRegion> foldingRegionAcceptor) {
		// copied from "DefaultFoldingRegionProvider
		ITextRegion region = locationInFileProvider.getFullTextRegion(eObject);
		if (region != null) {
			ITextRegion significant = locationInFileProvider.getSignificantTextRegion(eObject);
			int offset = region.getOffset();
			int length = region.getLength();
			foldingRegionAcceptor.accept(offset, length, significant);
		}
	}
}
