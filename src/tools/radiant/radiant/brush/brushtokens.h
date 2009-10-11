/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if !defined(INCLUDED_BRUSHTOKENS_H)
#define INCLUDED_BRUSHTOKENS_H

#include "stringio.h"
#include "stream/stringstream.h"
#include "brush.h"

inline bool FaceShader_importContentsFlagsValue(FaceShader& faceShader, Tokeniser& tokeniser) {
	// parse the optional contents/flags/value
	RETURN_FALSE_IF_FAIL(Tokeniser_getInteger(tokeniser, faceShader.m_flags.m_contentFlags));
	RETURN_FALSE_IF_FAIL(Tokeniser_getInteger(tokeniser, faceShader.m_flags.m_surfaceFlags));
	RETURN_FALSE_IF_FAIL(Tokeniser_getInteger(tokeniser, faceShader.m_flags.m_value));
	return true;
}

inline bool FaceTexdef_importTokens(FaceTexdef& texdef, Tokeniser& tokeniser) {
	// parse texdef
	RETURN_FALSE_IF_FAIL(Tokeniser_getFloat(tokeniser, texdef.m_projection.m_texdef.shift[0]));
	RETURN_FALSE_IF_FAIL(Tokeniser_getFloat(tokeniser, texdef.m_projection.m_texdef.shift[1]));
	RETURN_FALSE_IF_FAIL(Tokeniser_getFloat(tokeniser, texdef.m_projection.m_texdef.rotate));
	RETURN_FALSE_IF_FAIL(Tokeniser_getFloat(tokeniser, texdef.m_projection.m_texdef.scale[0]));
	RETURN_FALSE_IF_FAIL(Tokeniser_getFloat(tokeniser, texdef.m_projection.m_texdef.scale[1]));

	ASSERT_MESSAGE(texdef_sane(texdef.m_projection.m_texdef), "FaceTexdef_importTokens: bad texdef");
	return true;
}

inline bool FacePlane_importTokens(FacePlane& facePlane, Tokeniser& tokeniser) {
	// parse planepts
	for (std::size_t i = 0; i < 3; i++) {
		RETURN_FALSE_IF_FAIL(Tokeniser_parseToken(tokeniser, "("));
		for (std::size_t j = 0; j < 3; ++j) {
			RETURN_FALSE_IF_FAIL(Tokeniser_getDouble(tokeniser, facePlane.planePoints()[i][j]));
		}
		RETURN_FALSE_IF_FAIL(Tokeniser_parseToken(tokeniser, ")"));
	}
	facePlane.MakePlane();
	return true;
}

inline bool FaceShader_importTokens(FaceShader& faceShader, Tokeniser& tokeniser) {
	const char* texture = tokeniser.getToken();
	if (texture == 0) {
		Tokeniser_unexpectedError(tokeniser, texture, "#texture-name");
		return false;
	}
	if (string_equal(texture, "NULL")) {
		faceShader.setShader(GlobalTexturePrefix_get());
	} else {
		faceShader.setShader(GlobalTexturePrefix_get() + texture);
	}
	return true;
}

class UFOFaceTokenImporter {
	Face& m_face;
public:
	UFOFaceTokenImporter(Face& face) : m_face(face) {
	}
	bool importTokens(Tokeniser& tokeniser) {
		RETURN_FALSE_IF_FAIL(FacePlane_importTokens(m_face.getPlane(), tokeniser));
		RETURN_FALSE_IF_FAIL(FaceShader_importTokens(m_face.getShader(), tokeniser));
		RETURN_FALSE_IF_FAIL(FaceTexdef_importTokens(m_face.getTexdef(), tokeniser));
		if (Tokeniser_nextTokenIsDigit(tokeniser)) {
			m_face.getShader().m_flags.m_specified = true;
			RETURN_FALSE_IF_FAIL(FaceShader_importContentsFlagsValue(m_face.getShader(), tokeniser));
		}
		m_face.getTexdef().m_scaleApplied = true;
		return true;
	}
};

inline void FacePlane_exportTokens(const FacePlane& facePlane, TokenWriter& writer) {
	// write planepts
	for (std::size_t i = 0; i < 3; i++) {
		writer.writeToken("(");
		for (std::size_t j = 0; j < 3; j++) {
			writer.writeFloat(Face::m_quantise(facePlane.planePoints()[i][j]));
		}
		writer.writeToken(")");
	}
}

inline void FaceTexdef_exportTokens(const FaceTexdef& faceTexdef, TokenWriter& writer) {
	ASSERT_MESSAGE(texdef_sane(faceTexdef.m_projection.m_texdef), "FaceTexdef_exportTokens: bad texdef");
	// write texdef
	writer.writeFloat(faceTexdef.m_projection.m_texdef.shift[0]);
	writer.writeFloat(faceTexdef.m_projection.m_texdef.shift[1]);
	writer.writeFloat(faceTexdef.m_projection.m_texdef.rotate);
	writer.writeFloat(faceTexdef.m_projection.m_texdef.scale[0]);
	writer.writeFloat(faceTexdef.m_projection.m_texdef.scale[1]);
}

inline void FaceShader_ContentsFlagsValue_exportTokens(const FaceShader& faceShader, TokenWriter& writer) {
	// write surface flags
	writer.writeInteger(faceShader.m_flags.m_contentFlags);
	writer.writeInteger(faceShader.m_flags.m_surfaceFlags);
	writer.writeInteger(faceShader.m_flags.m_value);
}

inline void FaceShader_exportTokens(const FaceShader& faceShader, TokenWriter& writer) {
	// write shader name
	if (string_empty(shader_get_textureName(faceShader.getShader()))) {
		writer.writeToken("tex_common/nodraw");
	} else {
		writer.writeToken(shader_get_textureName(faceShader.getShader()));
	}
}

class UFOFaceTokenExporter {
	const Face& m_face;
public:
	UFOFaceTokenExporter(const Face& face) : m_face(face) {
	}
	void exportTokens(TokenWriter& writer) const {
		FacePlane_exportTokens(m_face.getPlane(), writer);
		FaceShader_exportTokens(m_face.getShader(), writer);
		FaceTexdef_exportTokens(m_face.getTexdef(), writer);
		if (m_face.getShader().m_flags.m_specified || m_face.isDetail()) {
			FaceShader_ContentsFlagsValue_exportTokens(m_face.getShader(), writer);
		}
		writer.nextLine();
	}
};

class BrushTokenImporter : public MapImporter {
	Brush& m_brush;

public:
	BrushTokenImporter(Brush& brush) : m_brush(brush) {
	}
	bool importTokens(Tokeniser& tokeniser) {
		while (1) {
			// check for end of brush
			tokeniser.nextLine();
			const char* token = tokeniser.getToken();
			if (string_equal(token, "}")) {
				break;
			}

			tokeniser.ungetToken();

			m_brush.push_back(FaceSmartPointer(new Face(&m_brush)));

			tokeniser.nextLine();

			Face& face = *m_brush.back();

			UFOFaceTokenImporter importer(face);
			RETURN_FALSE_IF_FAIL(importer.importTokens(tokeniser));
			face.planeChanged();
		}

		m_brush.planeChanged();
		m_brush.shaderChanged();

		return true;
	}
};


class BrushTokenExporter : public MapExporter {
	const Brush& m_brush;

public:
	BrushTokenExporter(const Brush& brush) : m_brush(brush) {
	}
	void exportTokens(TokenWriter& writer) const {
		m_brush.evaluateBRep(); // ensure b-rep is up-to-date, so that non-contributing faces can be identified.

		if (!m_brush.hasContributingFaces()) {
			return;
		}

		writer.writeToken("{");
		writer.nextLine();

		for (Brush::const_iterator i = m_brush.begin(); i != m_brush.end(); ++i) {
			const Face& face = *(*i);

			if (face.contributes()) {
				UFOFaceTokenExporter exporter(face);
				exporter.exportTokens(writer);
			}
		}

		writer.writeToken("}");
		writer.nextLine();
	}
};


#endif
