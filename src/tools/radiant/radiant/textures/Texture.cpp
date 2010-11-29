#include "texturelib.h"
#include "iimage.h"
#include "itextures.h"
#include "igl.h"
#include "AutoPtr.h"

#include "TextureManipulator.h"

/**
 * @brief This function does the actual processing of raw RGBA data into a GL texture.
 * @note It will also resample to power-of-two dimensions, generate the mipmaps and adjust gamma.
 */
void GLTexture::LoadTextureRGBA (Image* image)
{
	surfaceFlags = image->getSurfaceFlags();
	contentFlags = image->getContentFlags();
	value = image->getValue();
	width = image->getWidth();
	height = image->getHeight();

	Image* processed = shaders::TextureManipulator::Instance().getProcessedImage(image);

	hasAlpha = processed->hasAlpha();
	color = shaders::TextureManipulator::Instance().getFlatshadeColour(processed);

	glGenTextures(1, &texture_number);

	glBindTexture(GL_TEXTURE_2D, texture_number);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

	// Tell OpenGL how to use the mip maps we will be creating here
	shaders::TextureManipulator::Instance().setTextureParameters();

	glTexImage2D(GL_TEXTURE_2D, 0, hasAlpha ? GL_RGBA : GL_RGB, processed->getWidth(), processed->getHeight(), 0,
			GL_RGBA, GL_UNSIGNED_BYTE, processed->getRGBAPixels());

	glBindTexture(GL_TEXTURE_2D, 0);

	// if we had to resize it
	if (image != processed)
		delete processed;
}

GLTexture::GLTexture (const LoadImageCallback& load, const std::string& name) :
	name(name), load(load), width(0), height(0), texture_number(0), color(0.0, 0.0, 0.0), surfaceFlags(0),
			contentFlags(0), value(0), hasAlpha(false)
{
}

void GLTexture::realise ()
{
	texture_number = 0;
	/* skip empty names and normalmaps */
	if (!name.empty() && !strstr(name.c_str(), "_nm")) {
		AutoPtr<Image> image(load.loadImage(name));
		if (image) {
			LoadTextureRGBA(image);
		} else {
			globalWarningStream() << "Texture load failed: '" << name << "'\n";
		}
	}
}

const std::string& GLTexture::getName () const
{
	return name;
}

void GLTexture::unrealise ()
{
	if (GlobalOpenGL().contextValid && texture_number != 0) {
		glDeleteTextures(1, &texture_number);
	}
}
