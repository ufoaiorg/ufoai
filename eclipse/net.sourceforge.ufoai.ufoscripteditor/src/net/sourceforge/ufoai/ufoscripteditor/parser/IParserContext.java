package net.sourceforge.ufoai.ufoscripteditor.parser;

import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.source.IAnnotationModel;

public interface IParserContext {
	IDocument getDocument();

	IAnnotationModel getAnnotations();
}
