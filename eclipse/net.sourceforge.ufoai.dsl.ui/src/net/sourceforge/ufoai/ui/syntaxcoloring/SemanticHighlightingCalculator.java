package net.sourceforge.ufoai.ui.syntaxcoloring;

import java.util.List;

import net.sourceforge.ufoai.ufoScript.UFONode;
import net.sourceforge.ufoai.ufoScript.UFOScript;
import net.sourceforge.ufoai.ufoScript.UfoScriptPackage;
import net.sourceforge.ufoai.ui.tasks.ITaskElementChecker;

import org.eclipse.emf.common.util.EList;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.emf.ecore.EStructuralFeature;
import org.eclipse.xtext.nodemodel.ILeafNode;
import org.eclipse.xtext.nodemodel.INode;
import org.eclipse.xtext.nodemodel.util.NodeModelUtils;
import org.eclipse.xtext.resource.XtextResource;
import org.eclipse.xtext.ui.editor.syntaxcoloring.IHighlightedPositionAcceptor;
import org.eclipse.xtext.ui.editor.syntaxcoloring.ISemanticHighlightingCalculator;

import com.google.inject.Inject;

public class SemanticHighlightingCalculator implements ISemanticHighlightingCalculator {
	@Inject
	ITaskElementChecker objElementChecker;

	public void provideHighlightingFor(XtextResource resource, IHighlightedPositionAcceptor acceptor) {
		if (resource == null) {
			return;
		}

		for (EObject e : resource.getContents()) {
			if (e instanceof UFOScript) {
				UFOScript ufoScript = (UFOScript) e;
				EList<UFONode> roots = ufoScript.getRoots();
				for (UFONode root : roots) {
					highlightUFONode(root, acceptor);
				}
			}
		}
	}

	private void highlightUFONode(UFONode node, IHighlightedPositionAcceptor acceptor) {
		highlightNode(getFirstFeatureNode(node, UfoScriptPackage.Literals.UFO_NODE__TYPE), HighlightingConfiguration.RULE_PROPERTY_TYPE,
				acceptor);
		if (node.getNodes() != null) {
			for (UFONode child : node.getNodes()) {
				highlightUFONode(child, acceptor);
			}
		}
		if (node instanceof ILeafNode) {
			highlightLeafNode((ILeafNode) node, acceptor);
		}
	}

	private void highlightLeafNode(ILeafNode node, IHighlightedPositionAcceptor acceptor) {
		String varIgnorePrefix = objElementChecker.getPrefixToIgnore(node);
		if (varIgnorePrefix != null) {
			int start = node.getOffset() + node.getText().indexOf(varIgnorePrefix);
			acceptor.addPosition(start, varIgnorePrefix.length(), HighlightingConfiguration.TODO_STYLE);
		}
	}

	private void highlightNode(INode node, String id, IHighlightedPositionAcceptor acceptor) {
		if (node == null) {
			return;
		}
		if (node instanceof ILeafNode) {
			acceptor.addPosition(node.getOffset(), node.getLength(), id);
		} else {
			for (ILeafNode leaf : node.getLeafNodes()) {
				if (!leaf.isHidden()) {
					acceptor.addPosition(leaf.getOffset(), leaf.getLength(), id);
				}
			}
		}
	}

	public INode getFirstFeatureNode(EObject semantic, EStructuralFeature feature) {
		if (semantic == null) {
			return null;
		}
		if (feature == null) {
			return NodeModelUtils.findActualNodeFor(semantic);
		}
		List<INode> nodes = NodeModelUtils.findNodesForFeature(semantic, feature);
		if (!nodes.isEmpty()) {
			return nodes.get(0);
		}
		return null;
	}
}
