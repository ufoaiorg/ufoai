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
#include "imap.h"
#include "shaderlib.h"

class UFOFaceTokenImporter
{
	private:
		Face& m_face;

		/**
		 * Parse the optional contents/flags/value
		 * @param faceShader
		 * @param tokeniser
		 * @return
		 */
		bool importContentAndSurfaceFlags (FaceShader& faceShader, Tokeniser& tokeniser)
		{
			return Tokeniser_getInteger(tokeniser, faceShader.m_flags.m_contentFlags) && Tokeniser_getInteger(
					tokeniser, faceShader.m_flags.m_surfaceFlags) && Tokeniser_getInteger(tokeniser,
					faceShader.m_flags.m_value);
		}

		/**
		 * Parse texture definition
		 * @param texdef
		 * @param tokeniser
		 * @return
		 */
		bool importTextureDefinition (FaceTexdef& texdef, Tokeniser& tokeniser)
		{
			ASSERT_MESSAGE(texdef.m_projection.m_texdef.isSane(), "FaceTexdef_importTokens: bad texdef");
			return Tokeniser_getFloat(tokeniser, texdef.m_projection.m_texdef._shift[0]) && Tokeniser_getFloat(
					tokeniser, texdef.m_projection.m_texdef._shift[1]) && Tokeniser_getFloat(tokeniser,
					texdef.m_projection.m_texdef._rotate) && Tokeniser_getFloat(tokeniser,
					texdef.m_projection.m_texdef._scale[0]) && Tokeniser_getFloat(tokeniser,
					texdef.m_projection.m_texdef._scale[1]);

		}

		/**
		 * Parse plane points
		 * @param facePlane
		 * @param tokeniser
		 * @return
		 */
		bool importPlane (FacePlane& facePlane, Tokeniser& tokeniser)
		{
			for (std::size_t i = 0; i < 3; i++) {
				if (!Tokeniser_parseToken(tokeniser, "("))
					return false;
				for (std::size_t j = 0; j < 3; ++j) {
					if (!Tokeniser_getDouble(tokeniser, facePlane.planePoints()[i][j]))
						return false;
				}
				if (!Tokeniser_parseToken(tokeniser, ")"))
					return false;
			}
			facePlane.MakePlane();
			return true;
		}

		bool importTextureName (FaceShader& faceShader, Tokeniser& tokeniser)
		{
			const std::string texture = tokeniser.getToken();
			if (texture.empty()) {
				Tokeniser_unexpectedError(tokeniser, texture, "#texture-name");
				return false;
			}
			if (texture == "NULL") {
				faceShader.setShader(GlobalTexturePrefix_get());
			} else {
				faceShader.setShader(GlobalTexturePrefix_get() + texture);
			}
			return true;
		}
	public:
		UFOFaceTokenImporter (Face& face) :
			m_face(face)
		{
		}
		bool importTokens (Tokeniser& tokeniser)
		{
			if (!importPlane(m_face.getPlane(), tokeniser))
				return false;
			if (!importTextureName(m_face.getShader(), tokeniser))
				return false;
			if (!importTextureDefinition(m_face.getTexdef(), tokeniser))
				return false;
			if (Tokeniser_nextTokenIsDigit(tokeniser)) {
				m_face.getShader().m_flags.m_specified = true;
				if (!importContentAndSurfaceFlags(m_face.getShader(), tokeniser))
					return false;
			}
			return true;
		}
};

/**
 * Token exporter for a given face
 */
class UFOFaceTokenExporter
{
	private:
		/** the face to export */
		const Face& m_face;

		/**
		 * Write the plane points
		 * @param facePlane
		 * @param writer
		 */
		void exportPlane (const FacePlane& facePlane, TokenWriter& writer) const
		{
			for (std::size_t i = 0; i < 3; i++) {
				writer.writeToken("(");
				for (std::size_t j = 0; j < 3; j++) {
					writer.writeFloat(Face::m_quantise(facePlane.planePoints()[i][j]));
				}
				writer.writeToken(")");
			}
		}

		/**
		 * Write shift, rotate and scale texture definition values
		 * @param faceTexdef
		 * @param writer
		 */
		void exportTextureDefinition (const FaceTexdef& faceTexdef, TokenWriter& writer) const
		{
			ASSERT_MESSAGE(faceTexdef.m_projection.m_texdef.isSane(), "FaceTexdef_exportTokens: bad texdef");
			// write texdef
			writer.writeFloat(faceTexdef.m_projection.m_texdef._shift[0]);
			writer.writeFloat(faceTexdef.m_projection.m_texdef._shift[1]);
			writer.writeFloat(faceTexdef.m_projection.m_texdef._rotate);
			writer.writeFloat(faceTexdef.m_projection.m_texdef._scale[0]);
			writer.writeFloat(faceTexdef.m_projection.m_texdef._scale[1]);
		}

		/**
		 * Write surface and content flags
		 * @param faceShader
		 * @param writer
		 */
		void exportContentAndSurfaceFlags (const FaceShader& faceShader, TokenWriter& writer) const
		{
			writer.writeInteger(faceShader.m_flags.m_contentFlags);
			writer.writeInteger(faceShader.m_flags.m_surfaceFlags);
			writer.writeInteger(faceShader.m_flags.m_value);
		}

		/**
		 * Write the texture for the plane
		 * @param faceShader
		 * @param writer
		 */
		void exportTexture (const FaceShader& faceShader, TokenWriter& writer) const
		{
			// write shader name
			const std::string shaderName = shader_get_textureName(faceShader.getShader());
			if (shaderName.empty()) {
				writer.writeToken("tex_common/nodraw");
			} else {
				writer.writeToken(shaderName.c_str());
			}
		}

	public:
		/**
		 * @param face The face to export
		 */
		UFOFaceTokenExporter (const Face& face) :
			m_face(face)
		{
		}
		void exportTokens (TokenWriter& writer) const
		{
			exportPlane(m_face.getPlane(), writer);
			exportTexture(m_face.getShader(), writer);
			exportTextureDefinition(m_face.getTexdef(), writer);
			if (m_face.getShader().m_flags.m_specified || m_face.isDetail()) {
				exportContentAndSurfaceFlags(m_face.getShader(), writer);
			}
			writer.nextLine();
		}
};

class BrushTokenImporter: public MapImporter
{
		Brush& m_brush;

	public:
		BrushTokenImporter (Brush& brush) :
			m_brush(brush)
		{
		}
		bool importTokens (Tokeniser& tokeniser)
		{
			while (1) {
				// check for end of brush
				const std::string token = tokeniser.getToken();
				if (token == "}")
					break;

				tokeniser.ungetToken();

				m_brush.push_back(FaceSmartPointer(new Face(&m_brush)));

				Face& face = *m_brush.back();

				UFOFaceTokenImporter importer(face);
				if (!importer.importTokens(tokeniser))
					return false;
				face.planeChanged();
			}

			m_brush.planeChanged();
			m_brush.shaderChanged();

			return true;
		}
};

class BrushTokenExporter: public MapExporter
{
		const Brush& m_brush;

	public:
		BrushTokenExporter (const Brush& brush) :
			m_brush(brush)
		{
		}
		void exportTokens (TokenWriter& writer) const
		{
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
