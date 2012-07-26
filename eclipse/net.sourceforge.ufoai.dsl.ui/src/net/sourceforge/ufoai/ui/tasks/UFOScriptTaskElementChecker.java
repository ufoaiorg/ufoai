package net.sourceforge.ufoai.ui.tasks;

import net.sourceforge.ufoai.services.UFOScriptGrammarAccess;

import org.eclipse.xtext.nodemodel.INode;
import org.eclipse.xtext.nodemodel.impl.LeafNode;

import com.google.inject.Inject;

public class UFOScriptTaskElementChecker implements ITaskElementChecker {
	@Inject
	private UFOScriptGrammarAccess objGrammarAccess;

	public String getPrefixToIgnore(INode argNode) {
		if (objGrammarAccess.getSL_COMMENTRule().equals(argNode.getGrammarElement())) {
			// if it's a single line comment, its a leaf node
			if (((LeafNode) argNode).getText().contains(TaskConstants.TODO_PREFIX)) {
				return TaskConstants.TODO_PREFIX;
			}
		}
		return null;
	}
}
