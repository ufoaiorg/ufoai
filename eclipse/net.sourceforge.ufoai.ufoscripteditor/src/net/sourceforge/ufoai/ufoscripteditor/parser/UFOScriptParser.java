package net.sourceforge.ufoai.ufoscripteditor.parser;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;

import net.sourceforge.ufoai.ufoscripteditor.parser.parsers.UFOScriptTokenizer;
import net.sourceforge.ufoai.ufoscripteditor.parser.util.ParserUtil;

import org.eclipse.jface.text.BadLocationException;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.source.IAnnotationModel;

public class UFOScriptParser {
	private boolean inComment;
	private final List<UFOScriptBlock> blocks = new ArrayList<UFOScriptBlock>();
	private final IAnnotationModel annotations;
	private final IDocument doc;

	private final IParserContext context = new IParserContext() {
		@Override
		public IAnnotationModel getAnnotations() {
			return annotations;
		}

		@Override
		public IDocument getDocument() {
			return doc;
		}
	};

	public UFOScriptParser(final IDocument doc,
			final IAnnotationModel annotations, final String script)
			throws IOException {
		this.doc = doc;
		this.annotations = annotations;
		final BufferedReader reader = new BufferedReader(new StringReader(
				script));
		final StringBuilder b = new StringBuilder();
		for (;;) {
			final String s = reader.readLine();
			if (s == null)
				break;

			if (s.trim().isEmpty()) {
				b.append('\n');
				continue;
			}

			if (isComment(s)) {
				b.append('\n');
				continue;
			}

			final String cleanString = cleanString(s);
			if (cleanString.isEmpty()) {
				b.append('\n');
				continue;
			}
			b.append(cleanString).append('\n');
		}

		try {
			parse(b.toString());
		} catch (BadLocationException e) {
			throw new IOException(e);
		}
	}

	private String cleanString(String s) {
		if (inComment && s.contains("*/")) {
			inComment = false;
			final String replaceAll = s.replaceAll(".*\\*/", "");
			return replaceAll.trim();
		}
		if (s.contains("/*") && !s.contains("*/")) {
			inComment = true;
			final String replaceAll = s.replaceAll("/\\*.*", "");
			return replaceAll.trim();
		}

		if (inComment)
			return "";

		if (s.contains("//"))
			s = s.replaceAll("//.*", "");
		final String replaceAll = s.replaceAll("/\\*.*\\*/", "");
		return replaceAll.trim();
	}

	private void parse(final String script) throws BadLocationException {
		final UFOScriptTokenizer tokenizer = new UFOScriptTokenizer(script);
		while (tokenizer.hasMoreElements()) {
			final String token = tokenizer.nextToken();
			if (!tokenizer.isSeperator(token) && isValidScriptID(token))
				parseBlock(token, tokenizer);
		}
	}

	private void parseBlock(final String id, final UFOScriptTokenizer tokenizer)
			throws BadLocationException {
		if (!tokenizer.hasMoreElements())
			return;
		final int startLine = tokenizer.getLine();
		String token = tokenizer.nextToken();
		final IUFOParserFactory parser = getParser(id);
		String name = null;
		if (parser.isIDName()) {
			name = id;
		} else if (parser.isNameAfterID()) {
			name = tokenizer.nextToken();
			int i = 0;
			while (!token.equals("{")) {
				i++;
				token = tokenizer.nextToken();
				/*
				 * this is no longer valid - there is no script syntax where the
				 * opening block is more than 5 tokens away
				 */
				if (i > 5) {
					ParserUtil.addProblem(getParserContext(), tokenizer
							.getLine(),
							"Could not parse the name for script id " + id);
					tokenizer.skipBlock();
					break;
				}
			}
		} else {
			int i = 0;
			while (!token.equals("{")) {
				i++;
				if (!token.trim().isEmpty())
					name = token;
				token = tokenizer.nextToken();
				/*
				 * this is no longer valid - there is no script syntax where the
				 * opening block is more than 5 tokens away
				 */
				if (i > 5) {
					ParserUtil.addProblem(getParserContext(), tokenizer
							.getLine(),
							"Could not parse the name for script id " + id);
					tokenizer.skipBlock();
					break;
				}
			}
		}

		final StringBuilder b = new StringBuilder();
		int depth = 0;
		while (tokenizer.hasMoreElements()) {
			token = tokenizer.nextToken();
			if (token.equals("{")) {
				depth++;
			} else if (token.equals("}")) {
				if (depth == 0)
					break;
				else {
					depth--;
				}
			}

			b.append(token);
			if (!tokenizer.isSeperator(token))
				b.append(' ');
		}
		blocks.add(new UFOScriptBlock(id, name, parser, b.toString(),
				startLine, tokenizer.getLine() - startLine));
	}

	private boolean isValidScriptID(final String token) {
		final IUFOParserFactory factory = getParser(token);
		return factory != null;
	}

	private IUFOParserFactory getParser(final String token) {
		final IUFOParserFactory factory = UFOParserRegistry.get().getFactory(
				token);
		return factory;
	}

	private boolean isComment(final String s) {
		if (s.startsWith("//"))
			return true;
		if (inComment && s.contains("*/")) {
			inComment = false;
			return true;
		}
		if (s.startsWith("/*") && !s.contains("*/")) {
			inComment = true;
			return true;
		}
		return inComment;
	}

	public List<UFOScriptBlock> getBlocks() {
		return blocks;
	}

	public IParserContext getParserContext() {
		return context;
	}
}
