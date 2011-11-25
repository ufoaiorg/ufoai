/**
 * Token exporter for a given face
 */
class UFOFaceTokenExporter {
private:
	/** the face to export */
	const Face& m_face;

	/**
	 * Write the plane points
	 * @param facePlane
	 * @param writer
	 */
	void exportPlane(const FacePlane& facePlane, TokenWriter& writer) const;

	/**
	 * Write shift, rotate and scale texture definition values
	 * @param faceTexdef
	 * @param writer
	 */
	void exportTextureDefinition(const FaceTexdef& faceTexdef,
			TokenWriter& writer) const;

	/**
	 * Write surface and content flags
	 * @param faceShader
	 * @param writer
	 */
	void exportContentAndSurfaceFlags(const FaceShader& faceShader,
			TokenWriter& writer) const;

	/**
	 * Write the texture for the plane
	 * @param faceShader
	 * @param writer
	 */
	void exportTexture(const FaceShader& faceShader, TokenWriter& writer) const;

public:
	/**
	 * @param face The face to export
	 */
	UFOFaceTokenExporter(const Face& face);
	void exportTokens(TokenWriter& writer) const;
};
