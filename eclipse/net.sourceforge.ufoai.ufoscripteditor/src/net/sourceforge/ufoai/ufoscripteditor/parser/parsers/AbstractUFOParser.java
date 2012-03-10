package net.sourceforge.ufoai.ufoscripteditor.parser.parsers;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import net.sourceforge.ufoai.ufoscripteditor.parser.IParserContext;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.IUFOParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptBlock;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParser;
import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.subparser.IUFOSubParserFactory;
import net.sourceforge.ufoai.ufoscripteditor.parser.util.ParserUtil;
import net.sourceforge.ufoai.ufoscripteditor.parser.validators.IUFOScriptValidator;
import net.sourceforge.ufoai.ufoscripteditor.parser.validators.UFOScriptValidateException;

public abstract class AbstractUFOParser implements IUFOParser {
	private final HashMap<String, IUFOSubParserFactory> subParsers = new HashMap<String, IUFOSubParserFactory>();
	private final HashMap<String, IUFOScriptValidator> validators = new HashMap<String, IUFOScriptValidator>();
	private final IParserContext ctx;
	private final List<UFOScriptBlock> blocks = new ArrayList<UFOScriptBlock>();

	public AbstractUFOParser(final IParserContext ctx) {
		this.ctx = ctx;
	}

	public IParserContext getParserContext() {
		return ctx;
	}

	@Override
	public List<UFOScriptBlock> getInnerScriptBlocks(final String string,
			final int startLine) {
		final UFOScriptTokenizer t = new UFOScriptTokenizer(string);
		boolean firstRealToken = false;
		while (t.hasMoreElements()) {
			final String token = t.nextToken();
			if (t.isSeperator(token))
				continue;
			if (!firstRealToken && token.equals("{")) {
				final int lineNumber = t.getLine() + startLine;
				ParserUtil.addProblem(getParserContext(), lineNumber,
						"Unknown block");
				t.skipBlock();
				continue;
			}
			firstRealToken = true;
			final IUFOSubParserFactory subParser = getSubParser(token);
			if (subParser != null) {
				final IUFOSubParser create = subParser
						.create(getParserContext());
				create.parse(token, subParser.isIDName() ? token : null,
						blocks, t, this, startLine);
				create.validate();
				continue;
			}
			String value = null;
			final String key = token;
			if (hasOnlyValue()) {
				value = key;
			} else if (hasKeyValuePairs()) {
				if (getTokenCountPerEntry(key) < 2)
					throw new IllegalStateException(
							"key value parser with less than two tokens");
				value = null;
				while (t.hasMoreElements()) {
					final String innerToken = t.nextToken();
					if (t.isSeperator(innerToken))
						continue;
					if (innerToken.equals("}") || innerToken.equals("{")) {
						final int lineNumber = t.getLine() + startLine;
						ParserUtil.addProblem(getParserContext(), lineNumber,
								"Unknown block with " + key);
						if (innerToken.equals("{"))
							t.skipBlock();
						break;
					}
					value = innerToken;
					break;
				}
				// key and value are now already parsed
				int tokenCount = getTokenCountPerEntry(key) - 2;
				while (tokenCount > 0 && t.hasMoreElements()) {
					final String innerToken = t.nextToken();
					if (t.isSeperator(innerToken))
						continue;
					if (innerToken.equals("}") || innerToken.equals("{")) {
						final int lineNumber = t.getLine() + startLine;
						ParserUtil.addProblem(getParserContext(), lineNumber,
								"Unknown block with " + key);
						if (innerToken.equals("{"))
							t.skipBlock();
						break;
					}
					tokenCount--;
				}
			}
			if (value != null)
				blocks.add(new UFOScriptBlock(key, key, null, value, t
						.getLine(), t.getLine() + 1));
		}
		validate();
		return blocks;
	}

	/**
	 * Only one of {@link #hasOnlyValue()} or {@link #hasKeyValuePairs()} can
	 * return <code>true</code>. Or better, if this function returns
	 * <code>true</code> the return value of {@link #hasKeyValuePairs()} is
	 * ignored.
	 *
	 * @return <code>true</code> if the block is only defining a list of values.
	 *         Thus the key will be handled as value
	 */
	public boolean hasOnlyValue() {
		return false;
	}

	public IUFOSubParserFactory getSubParser(final String key) {
		return subParsers.get(key.toLowerCase());
	}

	protected void registerValidator(final IUFOScriptValidator validator) {
		final String key = validator.getID();
		if (validators.containsKey(key.toLowerCase()))
			throw new RuntimeException("Validator for key " + key
					+ " is already registered");

		validators.put(key.toLowerCase(), validator);
	}

	@Override
	public void validate() {
		for (final UFOScriptBlock block : blocks) {
			final String id = block.getId().toLowerCase();
			final IUFOScriptValidator validator = validators.get(id);
			if (validator != null) {
				try {
					validator.validate(block);
				} catch (final UFOScriptValidateException e) {
					ParserUtil.addProblem(getParserContext(), e.getLine(), e
							.getMessage());
				}
			}
		}
	}

	public void registerSubParser(final IUFOSubParserFactory parser,
			final IUFOParserFactory parentFactory) {
		if (subParsers.containsKey(parser.getID().toLowerCase()))
			return;

		parser.setParentFactory(parentFactory);
		subParsers.put(parser.getID().toLowerCase(), parser);
	}

	protected HashMap<String, IUFOSubParserFactory> getSubParsers() {
		return subParsers;
	}

	/**
	 * Override this if you need customized ids - e.g. for the menu components.
	 *
	 * @param id
	 *            The new id to register a parser for
	 */
	public IUFOSubParserFactory createSubParserFactory(final String id) {
		return null;
	}

	@Override
	public boolean hasKeyValuePairs() {
		return true;
	}

	/**
	 * Not called when {@link #hasOnlyValue()} returned true - but if an entry
	 * is built from several tokens, this is used to determine the end of an
	 * entry. {@link #hasKeyValuePairs()} must have returned <code>true</code>
	 * to get here.
	 *
	 * @return The amount of tokens per entry
	 */
	public int getTokenCountPerEntry(String key) {
		if (hasOnlyValue())
			return 1;
		if (hasKeyValuePairs())
			return 2;
		throw new IllegalStateException();
	}
}
