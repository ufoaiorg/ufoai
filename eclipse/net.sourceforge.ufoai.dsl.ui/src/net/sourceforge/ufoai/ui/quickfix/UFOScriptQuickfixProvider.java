package net.sourceforge.ufoai.ui.quickfix;

import net.sourceforge.ufoai.validation.UFOScriptJavaValidator;

import org.eclipse.jface.text.BadLocationException;
import org.eclipse.xtext.ui.editor.model.IXtextDocument;
import org.eclipse.xtext.ui.editor.model.edit.IModification;
import org.eclipse.xtext.ui.editor.model.edit.IModificationContext;
import org.eclipse.xtext.ui.editor.quickfix.DefaultQuickfixProvider;
import org.eclipse.xtext.ui.editor.quickfix.Fix;
import org.eclipse.xtext.ui.editor.quickfix.IssueResolutionAcceptor;
import org.eclipse.xtext.util.Strings;
import org.eclipse.xtext.validation.Issue;

public class UFOScriptQuickfixProvider extends DefaultQuickfixProvider {
	@Fix(UFOScriptJavaValidator.INVALID_NAME)
	public void fixName(final Issue issue, IssueResolutionAcceptor acceptor) {
		acceptor.accept(issue, "Convert to lowercase", "Convert to lowercase '" + issue.getData()[0] + "'", "upcase.png",
				new IModification() {
					public void apply(IModificationContext context) throws BadLocationException {
						IXtextDocument xtextDocument = context.getXtextDocument();
						String firstLetter = xtextDocument.get(issue.getOffset(), 1);
						xtextDocument.replace(issue.getOffset(), 1, Strings.toFirstLower(firstLetter));
					}
				});
	}
}
