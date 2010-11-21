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
#include "imap.h"
#include "Face.h"
#include "ContentsFlagsValue.h"

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
		void importContentAndSurfaceFlags (ContentsFlagsValue& flags, Tokeniser& tokeniser);

		/**
		 * Parse texture definition
		 * @param texdef
		 * @param tokeniser
		 * @return
		 */
		bool importTextureDefinition (FaceTexdef& texdef, Tokeniser& tokeniser);

		/**
		 * Parse plane points
		 * @param facePlane
		 * @param tokeniser
		 * @return
		 */
		bool importPlane (FacePlane& facePlane, Tokeniser& tokeniser);

		bool importTextureName (FaceShader& faceShader, Tokeniser& tokeniser);
	public:
		UFOFaceTokenImporter (Face& face);
		bool importTokens (Tokeniser& tokeniser);
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
		void exportPlane (const FacePlane& facePlane, TokenWriter& writer) const;

		/**
		 * Write shift, rotate and scale texture definition values
		 * @param faceTexdef
		 * @param writer
		 */
		void exportTextureDefinition (const FaceTexdef& faceTexdef, TokenWriter& writer) const;

		/**
		 * Write surface and content flags
		 * @param faceShader
		 * @param writer
		 */
		void exportContentAndSurfaceFlags (const FaceShader& faceShader, TokenWriter& writer) const;

		/**
		 * Write the texture for the plane
		 * @param faceShader
		 * @param writer
		 */
		void exportTexture (const FaceShader& faceShader, TokenWriter& writer) const;

	public:
		/**
		 * @param face The face to export
		 */
		UFOFaceTokenExporter (const Face& face);
		void exportTokens (TokenWriter& writer) const;
};

class BrushTokenImporter: public MapImporter
{
		Brush& m_brush;

	public:
		BrushTokenImporter (Brush& brush);
		bool importTokens (Tokeniser& tokeniser);
};

class BrushTokenExporter: public MapExporter
{
		const Brush& m_brush;

	public:
		BrushTokenExporter (const Brush& brush);
		void exportTokens (TokenWriter& writer) const;
};

#endif
