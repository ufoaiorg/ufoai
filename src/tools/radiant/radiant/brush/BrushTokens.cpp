#include "BrushTokens.h"
#include "FaceTokenExporter.h"
#include "FaceTokenImporter.h"
#include "stringio.h"
#include "stream/stringstream.h"
#include "imap.h"
#include "shaderlib.h"
#include "Brush.h"

/**
 * Parse the optional contents/flags/value
 * @param faceShader
 * @param tokeniser
 * @return
 */
void UFOFaceTokenImporter::importContentAndSurfaceFlags (ContentsFlagsValue& flags, Tokeniser& tokeniser)
{
	std::string token = tokeniser.getToken();
	flags.setContentFlags(string::toInt(token));

	token = tokeniser.getToken();
	flags.setSurfaceFlags(string::toInt(token));

	token = tokeniser.getToken();
	flags.setValue(string::toInt(token));
}

/**
 * Parse texture definition
 * @param texdef
 * @param tokeniser
 * @return
 */
bool UFOFaceTokenImporter::importTextureDefinition (FaceTexdef& texdef, Tokeniser& tokeniser)
{
	ASSERT_MESSAGE(texdef.m_projection.m_texdef.isSane(), "FaceTexdef_importTokens: bad texdef");
	return Tokeniser_getFloat(tokeniser, texdef.m_projection.m_texdef._shift[0]) && Tokeniser_getFloat(tokeniser,
			texdef.m_projection.m_texdef._shift[1]) && Tokeniser_getFloat(tokeniser,
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
bool UFOFaceTokenImporter::importPlane (FacePlane& facePlane, Tokeniser& tokeniser)
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

bool UFOFaceTokenImporter::importTextureName (FaceShader& faceShader, Tokeniser& tokeniser)
{
	const std::string texture = tokeniser.getToken();
	if (texture.empty()) {
		Tokeniser_unexpectedError(tokeniser, texture, "#texture-name");
		return false;
	}
	if (texture == "NULL" || texture.empty()) {
		faceShader.setShader("");
	} else {
		faceShader.setShader(GlobalTexturePrefix_get() + texture);
	}
	return true;
}

UFOFaceTokenImporter::UFOFaceTokenImporter (Face& face) :
	m_face(face)
{
}

bool UFOFaceTokenImporter::importTokens (Tokeniser& tokeniser)
{
	if (!importPlane(m_face.getPlane(), tokeniser))
		return false;
	if (!importTextureName(m_face.getShader(), tokeniser))
		return false;
	if (!importTextureDefinition(m_face.getTexdef(), tokeniser))
		return false;

	if (Tokeniser_nextTokenIsDigit(tokeniser)) {
		m_face.getShader().m_flags.setSpecified(true);
		importContentAndSurfaceFlags(m_face.getShader().m_flags, tokeniser);
	}

	return true;
}

/**
 * Write the plane points
 * @param facePlane
 * @param writer
 */
void UFOFaceTokenExporter::exportPlane (const FacePlane& facePlane, TokenWriter& writer) const
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
void UFOFaceTokenExporter::exportTextureDefinition (const FaceTexdef& faceTexdef, TokenWriter& writer) const
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
void UFOFaceTokenExporter::exportContentAndSurfaceFlags (const FaceShader& faceShader, TokenWriter& writer) const
{
	writer.writeInteger(faceShader.m_flags.getContentFlags());
	writer.writeInteger(faceShader.m_flags.getSurfaceFlags());
	writer.writeInteger(faceShader.m_flags.getValue());
}

/**
 * Write the texture for the plane
 * @param faceShader
 * @param writer
 */
void UFOFaceTokenExporter::exportTexture (const FaceShader& faceShader, TokenWriter& writer) const
{
	// write shader name
	const std::string shaderName = shader_get_textureName(faceShader.getShader());
	if (shaderName.empty()) {
		writer.writeToken("tex_common/nodraw");
	} else {
		writer.writeToken(shaderName);
	}
}

/**
 * @param face The face to export
 */
UFOFaceTokenExporter::UFOFaceTokenExporter (const Face& face) :
	m_face(face)
{
}

void UFOFaceTokenExporter::exportTokens (TokenWriter& writer) const
{
	exportPlane(m_face.getPlane(), writer);
	exportTexture(m_face.getShader(), writer);
	exportTextureDefinition(m_face.getTexdef(), writer);
	if (m_face.getShader().m_flags.isSpecified() || m_face.isDetail()) {
		exportContentAndSurfaceFlags(m_face.getShader(), writer);
	}
	writer.nextLine();
}

BrushTokenImporter::BrushTokenImporter (Brush& brush) :
	m_brush(brush)
{
}
bool BrushTokenImporter::importTokens (Tokeniser& tokeniser)
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

BrushTokenExporter::BrushTokenExporter (const Brush& brush) :
	m_brush(brush)
{
}
void BrushTokenExporter::exportTokens (TokenWriter& writer) const
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
