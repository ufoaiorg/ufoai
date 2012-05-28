package net.sourceforge.ufoai.ui.syntaxcoloring;

import java.util.List;

import net.sourceforge.ufoai.ufoScript.NamedBlock;
import net.sourceforge.ufoai.ufoScript.Property;
import net.sourceforge.ufoai.ufoScript.PropertyValue;
import net.sourceforge.ufoai.ufoScript.UFOScript;
import net.sourceforge.ufoai.ufoScript.UfoScriptPackage;

import org.eclipse.emf.common.util.EList;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.emf.ecore.EStructuralFeature;
import org.eclipse.xtext.nodemodel.ILeafNode;
import org.eclipse.xtext.nodemodel.INode;
import org.eclipse.xtext.nodemodel.util.NodeModelUtils;
import org.eclipse.xtext.resource.XtextResource;
import org.eclipse.xtext.ui.editor.syntaxcoloring.IHighlightedPositionAcceptor;
import org.eclipse.xtext.ui.editor.syntaxcoloring.ISemanticHighlightingCalculator;

public class SemanticHighlightingCalculator implements ISemanticHighlightingCalculator {
	@Override
	public void provideHighlightingFor(XtextResource resource, IHighlightedPositionAcceptor acceptor) {
		if (resource == null) {
			return;
		}

		for (EObject e : resource.getContents()) {
			if (e instanceof UFOScript) {
				UFOScript ufoScript = (UFOScript) e;
				EList<NamedBlock> blocks = ufoScript.getBlocks();
				for (NamedBlock block : blocks) {
					highlightNode(getFirstFeatureNode(block, UfoScriptPackage.Literals.NAMED_BLOCK__TYPE),
							HighlightingConfiguration.RULE_NAMED_BLOCK, acceptor);
					EList<Property> properties = block.getProperties();
					for (Property property : properties) {
						highlightNode(getFirstFeatureNode(property, UfoScriptPackage.Literals.PROPERTY__TYPE),
								HighlightingConfiguration.RULE_PROPERTY_TYPE, acceptor);
						PropertyValue value = property.getValue();
						highlightNode(getFirstFeatureNode(value, UfoScriptPackage.Literals.PROPERTY__VALUE),
								HighlightingConfiguration.RULE_PROPERTY_VALUE, acceptor);
					}
				}
			}
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
