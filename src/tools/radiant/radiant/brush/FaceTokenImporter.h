class UFOFaceTokenImporter {
private:
	Face& m_face;

	/**
	 * Parse the optional contents/flags/value
	 * @param faceShader
	 * @param tokeniser
	 * @return
	 */
	void importContentAndSurfaceFlags(ContentsFlagsValue& flags,
			Tokeniser& tokeniser);

	/**
	 * Parse texture definition
	 * @param texdef
	 * @param tokeniser
	 * @return
	 */
	bool importTextureDefinition(FaceTexdef& texdef, Tokeniser& tokeniser);

	/**
	 * Parse plane points
	 * @param facePlane
	 * @param tokeniser
	 * @return
	 */
	bool importPlane(FacePlane& facePlane, Tokeniser& tokeniser);

	bool importTextureName(FaceShader& faceShader, Tokeniser& tokeniser);
public:
	UFOFaceTokenImporter(Face& face);
	bool importTokens(Tokeniser& tokeniser);
};
