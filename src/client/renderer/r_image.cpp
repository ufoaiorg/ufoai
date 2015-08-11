/**
 * @file
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"
#include "r_error.h"
#include "r_geoscape.h"
#include "../../shared/images.h"
#include "../cl_screen.h"

#define MAX_IMAGEHASH 256
static image_t* imageHash[MAX_IMAGEHASH];

#define IMAGE_ARRAY_SIZE 128
typedef struct imageArray_s {
	image_t images[IMAGE_ARRAY_SIZE];
	struct imageArray_s* next;
} imageArray_t;

imageArray_t r_images;
int r_numImages;

/* Wicked for()-loop to go through all images in the r_images linked list. Parameters are: (int i, image_t* image, imageArray_t* imageArray) */
#define FOR_EACH_IMAGE(i, image, imageArray) \
	for (i = 0, imageArray = &r_images, image = &imageArray->images[0]; i < r_numImages; i++, image++, \
			(i % IMAGE_ARRAY_SIZE) ? 0 : (image = (imageArray = imageArray->next) ? &imageArray->images[0] : nullptr))


/* generic environment map */
image_t* r_envmaptextures[MAX_ENVMAPTEXTURES];

/* lense flares */
image_t* r_flaretextures[NUM_FLARETEXTURES];

#define MAX_TEXTURE_SIZE 8192

/**
 * @brief Free previously loaded materials and their stages
 * @sa R_LoadMaterials
 */
void R_ImageClearMaterials (void)
{
	int i;
	image_t* image;
	imageArray_t* images;

	/* clear previously loaded materials */
	FOR_EACH_IMAGE(i, image, images) {
		material_t* m = &image->material;
		materialStage_t* s = m->stages;

		while (s) {  /* free the stages chain */
			materialStage_t* ss = s->next;
			Mem_Free(s);
			s = ss;
		}

		*m = defaultMaterial;
	}
}

/**
 * @brief Shows all loaded images
 */
void R_ImageList_f (void)
{
	int i, cnt;
	image_t* image;
	imageArray_t* images;
	int texels;

	Com_Printf("------------------\n");
	texels = 0;
	cnt = 0;

	FOR_EACH_IMAGE(i, image, images) {
		const char* type;
		if (!image->texnum)
			continue;
		cnt++;
		texels += image->upload_width * image->upload_height;
		switch (image->type) {
		case it_effect:
			type = "EF";
			break;
		case it_skin:
			type = "SK";
			break;
		case it_wrappic:
			type = "WR";
			break;
		case it_chars:
			type = "CH";
			break;
		case it_static:
			type = "ST";
			break;
		case it_normalmap:
			type = "NM";
			break;
		case it_material:
			type = "MA";
			break;
		case it_lightmap:
			type = "LM";
			break;
		case it_world:
			type = "WO";
			break;
		case it_pic:
			type = "PI";
			break;
		default:
			type = "  ";
			break;
		}

		Com_Printf("%s %4i %4i RGB: %5i idx: %s\n", type, image->upload_width, image->upload_height, image->texnum, image->name);
	}
	Com_Printf("Total textures: %i/%i (max textures: %i)\n", cnt, r_numImages, MAX_GL_TEXTURES);
	Com_Printf("Total texel count (not counting mipmaps): %i\n", texels);
}

/**
 * @brief Generic image-data loading fucntion.
 * @param[in] name (Full) pathname to the image to load. Extension (if given) will be ignored.
 * @param[out] pic Image data.
 * @param[out] width Width of the loaded image.
 * @param[out] height Height of the loaded image.
 * @sa R_FindImage
 */
void R_LoadImage (const char* name, byte** pic, int* width, int* height)
{
	char filenameTemp[MAX_QPATH];
	SDL_Surface* surf;

	if (Q_strnull(name))
		Com_Error(ERR_FATAL, "R_LoadImage: nullptr name");

	Com_StripExtension(name, filenameTemp, sizeof(filenameTemp));

	if ((surf = Img_LoadImage(filenameTemp))) {
		const size_t size = (surf->w * surf->h) * 4;
		*width = surf->w;
		*height = surf->h;
		*pic = Mem_PoolAllocTypeN(byte, size, vid_imagePool);
		memcpy(*pic, surf->pixels, size);
		SDL_FreeSurface(surf);
	}
}

void R_ScaleTexture (const unsigned* in, int inwidth, int inheight, unsigned* out, int outwidth, int outheight)
{
	int i;
	unsigned p1[MAX_TEXTURE_SIZE], p2[MAX_TEXTURE_SIZE];
	const unsigned fracstep = inwidth * 0x10000 / outwidth;

	assert(outwidth <= MAX_TEXTURE_SIZE);

	unsigned frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++) {
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++) {
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth) {
		const int index = inwidth * (int) ((i + 0.25) * inheight / outheight);
		const unsigned* inrow = in + index;
		const int index2 = inwidth * (int) ((i + 0.75) * inheight / outheight);
		const unsigned* inrow2 = in + index2;

		assert(index < inwidth * inheight);
		assert(index2 < inwidth * inheight);

		for (int j = 0; j < outwidth; j++) {
			const byte* pix1 = (const byte*) inrow + p1[j];
			const byte* pix2 = (const byte*) inrow + p2[j];
			const byte* pix3 = (const byte*) inrow2 + p1[j];
			const byte* pix4 = (const byte*) inrow2 + p2[j];
			((byte*) (out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte*) (out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte*) (out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte*) (out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/**
 * @brief Calculates the texture size that should be used to upload the texture data
 * @param[in] width The width of the source texture data
 * @param[in] height The heigt of the source texture data
 * @param[out] scaledWidth The resulting width - can be the same as the given @c width
 * @param[out] scaledHeight The resulting height - can be the same as the given @c height
 */
void R_GetScaledTextureSize (int width, int height, int* scaledWidth, int* scaledHeight)
{
	for (*scaledWidth = 1; *scaledWidth < width; *scaledWidth <<= 1) {}
	for (*scaledHeight = 1; *scaledHeight < height; *scaledHeight <<= 1) {}

	while (*scaledWidth > r_config.maxTextureSize || *scaledHeight > r_config.maxTextureSize) {
		*scaledWidth >>= 1;
		*scaledHeight >>= 1;
	}

	if (*scaledWidth > MAX_TEXTURE_SIZE)
		*scaledWidth = MAX_TEXTURE_SIZE;
	else if (*scaledWidth < 1)
		*scaledWidth = 1;

	if (*scaledHeight > MAX_TEXTURE_SIZE)
		*scaledHeight = MAX_TEXTURE_SIZE;
	else if (*scaledHeight < 1)
		*scaledHeight = 1;
}

inline static bool R_IsClampedImageType (imagetype_t type)
{
	return type == it_pic || type == it_worldrelated;
}

/**
 * @brief Uploads the opengl texture to the server
 * @param[in] data Must be in RGBA format
 * @param width Width of the image
 * @param height Height of the image
 * @param[in,out] image Pointer to the image structure to initialize
 */
void R_UploadTexture (const unsigned* data, int width, int height, image_t* image)
{
	unsigned* scaled = nullptr;
	int scaledWidth, scaledHeight;
	const bool mipmap = (image->type != it_pic && image->type != it_worldrelated && image->type != it_chars);
	const bool clamp = R_IsClampedImageType(image->type);
#ifdef GL_VERSION_ES_CM_1_0
	/*
	 * According to https://www.khronos.org/opengles/sdk/1.1/docs/man/glTexImage2D.xml
	 * this *must* match the data format, which *always* is RGBA.
	 */
	GLint texFormat = GL_RGBA;
	/** @todo Should we strip the alpha from data if not needed and upload as RGB? Memory might be a concern here. */
#else
	GLint texFormat = r_config.gl_compressed_solid_format ? r_config.gl_compressed_solid_format : r_config.gl_solid_format;
	int i;
	const byte* scan;

	/* scan the texture for any non-255 alpha */
	int c = width * height;
	/* set scan to the first alpha byte */
	for (i = 0, scan = ((const byte*) data) + 3; i < c; i++, scan += 4) {
		if (*scan != 255) {
			texFormat = r_config.gl_compressed_alpha_format ? r_config.gl_compressed_alpha_format : r_config.gl_alpha_format;
			image->has_alpha = true;
			break;
		}
	}
#endif

	R_GetScaledTextureSize(width, height, &scaledWidth, &scaledHeight);

	image->upload_width = scaledWidth;	/* after power of 2 and scales */
	image->upload_height = scaledHeight;

	/* some images need very little attention (pics, fonts, etc..) */
	if (!mipmap && scaledWidth == width && scaledHeight == height) {
		/* no mipmapping for these images to save memory */
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (clamp) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			R_CheckError();
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			R_CheckError();
		}
		glTexImage2D(GL_TEXTURE_2D, 0, texFormat, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		return;
	}

	if (scaledWidth != width || scaledHeight != height) {  /* whereas others need to be scaled */
		scaled = Mem_PoolAllocTypeN(unsigned, scaledWidth * scaledHeight, vid_imagePool);
		R_ScaleTexture(data, width, height, scaled, scaledWidth, scaledHeight);
	}

	/* and mipmapped */
	if (mipmap) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		if (r_config.anisotropic) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_config.maxAnisotropic);
			R_CheckError();
		}
#ifndef GL_VERSION_ES_CM_1_0
		if (r_texture_lod->integer && r_config.lod_bias) {
			glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, r_texture_lod->value);
			R_CheckError();
		}
#endif
	} else {
		if (r_config.anisotropic) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
			R_CheckError();
		}
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
	}
	R_CheckError();

	if (clamp) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		R_CheckError();
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		R_CheckError();
	}

	glTexImage2D(GL_TEXTURE_2D, 0, texFormat, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled ? scaled : data);
	R_CheckError();

	if (scaled)
		Mem_Free(scaled);
}

/**
 * @brief Applies blurring to a texture
 */
void R_SoftenTexture (byte* in, int width, int height, int bpp)
{
	const int size = width * height * bpp;

	/* soften into a copy of the original image, as in-place would be incorrect */
	byte* const out = Mem_PoolAllocTypeN(byte, size, vid_imagePool);
	if (!out)
		Com_Error(ERR_FATAL, "R_SoftenTexture: failed on allocation of %i bytes", width * height * bpp);

	memcpy(out, in, size);

	for (int i = 1; i < height - 1; i++) {
		for (int j = 1; j < width - 1; j++) {
			const byte* src = in + ((i * width) + j) * bpp;  /* current input pixel */

			const byte* u = (src - (width * bpp));  /* and it's neighbors */
			const byte* d = (src + (width * bpp));
			const byte* l = (src - (1 * bpp));
			const byte* r = (src + (1 * bpp));

			byte* dest = out + ((i * width) + j) * bpp;  /* current output pixel */

			for (int k = 0; k < bpp; k++)
				dest[k] = (u[k] + d[k] + l[k] + r[k]) / 4;
		}
	}

	/* copy the softened image over the input image, and free it */
	memcpy(in, out, size);
	Mem_Free(out);
}

void R_UploadAlpha (const image_t* image, const byte* alphaData)
{
	R_BindTexture(image->texnum);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, image->width, image->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, alphaData);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (image->type == it_wrappic)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

static inline void R_DeleteImage (image_t* image)
{
	const unsigned int hash = Com_HashKey(image->name, MAX_IMAGEHASH);
	for (image_t** anchor = &imageHash[hash]; *anchor; anchor = &(*anchor)->hash_next) {
		if (Q_streq((*anchor)->name, image->name)) {
			HASH_Delete(anchor);
			break;
		}
	}

	/* free it */
	glDeleteTextures(1, &image->texnum);
	R_CheckError();

	OBJZERO(*image);
}

image_t* R_GetImage (const char* name)
{
	image_t* image;
	const unsigned hash = Com_HashKey(name, MAX_IMAGEHASH);

	/* look for it */
	for (image = imageHash[hash]; image; image = image->hash_next)
		if (Q_streq(name, image->name))
			return image;

	return nullptr;
}

/**
 * @brief Creates a new image from RGBA data. Stores it in the gltextures array
 * and also uploads it.
 * @note This is also used as an entry point for the generated r_noTexture
 * @param[in] name The name of the newly created image
 * @param[in] pic The RGBA data of the image
 * @param[in] width The width of the image (power of two, please)
 * @param[in] height The height of the image (power of two, please)
 * @param[in] type The image type @sa imagetype_t
 */
image_t* R_LoadImageData (const char* name, const byte* pic, int width, int height, imagetype_t type)
{
	image_t* image;
	imageArray_t* images;
	int i;
	size_t len;
	unsigned hash;

	len = strlen(name);
	if (len >= sizeof(image->name))
		Com_Error(ERR_DROP, "R_LoadImageData: \"%s\" is too long", name);
	if (len == 0)
		Com_Error(ERR_DROP, "R_LoadImageData: name is empty");

	/* look for it */
	image = R_GetImage(name);
	if (image) {
		assert(image->texnum);
		Com_Printf("R_LoadImageData: image '%s' is already uploaded\n", name);
		return image;
	}

	/* find a free image_t, using a wicked for()-loop */
	FOR_EACH_IMAGE(i, image, images) {
		if (!image->texnum)
			break;
	}

	if (i == r_numImages) {
		/* Did we run out of space in the current array? Add a new array chunk */
		if (r_numImages % IMAGE_ARRAY_SIZE == 0) {
			for (images = &r_images; images->next;)
				images = images->next;
			images->next = Mem_AllocType(imageArray_t);
			image = &images->next->images[0];
		}
		r_numImages++;
	}
	OBJZERO(*image);
	image->material = defaultMaterial;
	image->has_alpha = false;
	image->type = type;
	image->width = width;
	image->height = height;

	/** @todo Instead of this hack, unit tests' build should link to the dummy GL driver */
#ifdef COMPILE_UNITTESTS
	{
		static int texnum = 0;
		image->texnum = ++texnum;
	}
#else
	glGenTextures(1, &image->texnum);
#endif

	Q_strncpyz(image->name, name, sizeof(image->name));
	/* drop extension */
	if (len >= 4 && image->name[len - 4] == '.') {
		image->name[len - 4] = '\0';
		Com_Printf("Image with extension: '%s'\n", name);
	}

	hash = Com_HashKey(image->name, MAX_IMAGEHASH);
	HASH_Add(imageHash, image, hash);

	if (pic) {
		R_BindTexture(image->texnum);
		R_UploadTexture((const unsigned* ) pic, width, height, image);
	}
	return image;
}

image_t* R_RenderToTexture (const char* name, int x, int y, int w, int h)
{
	image_t* img = R_GetImage(name);
	const bool dimensionDiffer = img != nullptr && img->width != w && img->height != h;
	if (img == nullptr || dimensionDiffer) {
		if (dimensionDiffer) {
			R_DeleteImage(img);
		}
		byte* const buf = Mem_PoolAllocTypeN(byte, w * h * 4, vid_imagePool);
		img = R_LoadImageData(name, buf, w, h, it_effect);
		Mem_Free(buf);
	}

	glFlush();
#ifndef GL_VERSION_ES_CM_1_0
	glReadBuffer(GL_BACK);
#endif
	R_SelectTexture(&texunit_diffuse);
	R_BindTexture(img->texnum);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, w, h, 0);

	return img;
}

/**
 * @brief Set up new image type and change texturemapping paramenters accordingly
 * @note Mipmapping mode is not updated
 */
static void R_ChangeImageType(image_t* img, imagetype_t type)
{
	if (!img)
		return;

	img->type = type;
	R_BindTexture(img->texnum);

	if (R_IsClampedImageType(type)) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
}

/**
 * @brief Finds or loads the given image
 * @sa R_RegisterImage
 * @param[in] pname Image name Path relative to the game dir (e.g. textures/tex_common/nodraw)
 * @param[in] type The type of the image. This has influence on image filters and texture
 * parameters when uploading the image data
 * @note the image name has to be at least 5 chars long
 * @sa R_LoadTGA
 * @sa R_LoadJPG
 * @sa R_LoadPNG
 */
image_t* R_FindImage (const char* pname, imagetype_t type)
{
	char lname[MAX_QPATH];
	image_t* image;
	SDL_Surface* surf;

	if (!pname || !pname[0])
		Com_Error(ERR_FATAL, "R_FindImage: nullptr name");

	/* drop extension */
	Com_StripExtension(pname, lname, sizeof(lname));

	image = R_GetImage(lname);
	if (image) {
		/* Warn if game tries to use same image with different texture mapping modes */
		if (R_IsClampedImageType(image->type) != R_IsClampedImageType(type)) { /** @todo should also check the mipmapping */
			Com_Printf("Warning: inconsistent usage of image %s (%i,%i)\n", image->name, image->type, type);
			R_ChangeImageType(image, type);
			R_ChangeImageType(image->normalmap, type);
			R_ChangeImageType(image->roughnessmap, type);
			R_ChangeImageType(image->specularmap, type);
			R_ChangeImageType(image->glowmap, type);
		}
		return image;
	}

	if ((surf = Img_LoadImage(lname))) {
		image = R_LoadImageData(lname, (byte*)surf->pixels, surf->w, surf->h, type);
		SDL_FreeSurface(surf);
		if (image->type == it_world) {
			image->normalmap = R_FindImage(va("%s_nm", image->name), it_normalmap);
			if (image->normalmap == r_noTexture)
				image->normalmap = nullptr;
		}
		if (image->type != it_glowmap) {
			image->glowmap = R_FindImage(va("%s_gm", image->name), it_glowmap);
			if (image->glowmap == r_noTexture)
				image->glowmap = nullptr;
		}
		if (image->type != it_normalmap) {
			image->normalmap = R_FindImage(va("%s_nm", image->name), it_normalmap);
			if (image->normalmap == r_noTexture)
				image->normalmap = nullptr;
		}
		if (image->type != it_specularmap) {
			image->specularmap = R_FindImage(va("%s_sm", image->name), it_specularmap);
			if (image->specularmap == r_noTexture)
				image->specularmap = nullptr;
		}
		if (image->type != it_roughnessmap) {
			image->roughnessmap = R_FindImage(va("%s_rm", image->name), it_roughnessmap);
			if (image->roughnessmap == r_noTexture)
				image->roughnessmap = nullptr;
		}
	}

	/* no fitting texture found */
	if (!image)
		image = r_noTexture;

	return image;
}

/**
 * @brief Searches for an image in the image array
 * @param[in] name The name of the image relative to pics/
 * @note name may not be null and has to be longer than 4 chars
 * @return nullptr on error or image_t pointer on success
 * @sa R_FindImage
 */
const image_t* R_FindPics (const char* name)
{
	const image_t* image = R_FindImage(va("pics/%s", name), it_pic);
	if (image == r_noTexture)
		return nullptr;
	return image;
}

bool R_ImageExists (const char* pname, ...)
{
	char const* const* const types = Img_GetImageTypes();
	char filename[MAX_QPATH];
	va_list ap;

	va_start(ap, pname);
	Q_vsnprintf(filename, sizeof(filename), pname, ap);
	va_end(ap);

	for (int i = 0; types[i]; i++) {
		if (FS_CheckFile("%s.%s", filename, types[i]) != -1)
			return true;
	}
	return false;
}

/**
 * @brief Returns an index of the image pointer in the r_images linked list, as if r_images would be a plain contiguous array
 * @param imagePtr The image pointer
 */
int R_GetImageIndex (image_t* imagePtr)
{
	imageArray_t* images;

	for (images = &r_images; images; images = images->next) {
		if (imagePtr >= &images->images[0] && imagePtr <= &images->images[IMAGE_ARRAY_SIZE - 1])
			return imagePtr - &images->images[0];
	}

	return -1;
}

/**
 * @brief Returns an image pointer from the r_images linked list, as if r_images would be a plain contiguous array
 * @param i The image index inside r_images
 */
image_t* R_GetImageAtIndex (int i)
{
	imageArray_t* images;

	if (i >= r_numImages || i < 0)
		return nullptr;

	for (images = &r_images; i >= IMAGE_ARRAY_SIZE; i -= IMAGE_ARRAY_SIZE)
		images = images->next;

	return &images->images[i];
}

/**
 * @brief Free the image and its assigned maps (roughness, normal, specular, glow - if there are any)
 * @param image The image that should be freed
 */
void R_FreeImage (image_t* image)
{
	/* free image slot */
	if (!image || !image->texnum)
		return;

	/* also free the several maps if they are loaded */
	if (image->normalmap)
		R_DeleteImage(image->normalmap);
	if (image->glowmap)
		R_DeleteImage(image->glowmap);
	if (image->roughnessmap)
		R_DeleteImage(image->roughnessmap);
	if (image->specularmap)
		R_DeleteImage(image->specularmap);
	R_DeleteImage(image);
}

/**
 * @brief Any image that is a mesh or world texture will be removed here
 * @sa R_ShutdownImages
 */
void R_FreeWorldImages (void)
{
	int i;
	image_t* image;
	imageArray_t* images;

	R_CheckError();
	/* Wicked for()-loop (tm) */
	FOR_EACH_IMAGE(i, image, images) {
		if (image->type < it_world)
			continue;			/* keep them */

		/* free it */
		R_FreeImage(image);
	}
}

void R_InitImages (void)
{
	int i;

	r_numImages = 0;

	for (i = 0; i < MAX_ENVMAPTEXTURES; i++) {
		r_envmaptextures[i] = R_FindImage(va("pics/envmaps/envmap_%i", i), it_effect);
		if (r_envmaptextures[i] == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load environment map %i", i);
	}

	for (i = 0; i < NUM_FLARETEXTURES; i++) {
		r_flaretextures[i] = R_FindImage(va("pics/flares/flare_%i", i), it_effect);
		if (r_flaretextures[i] == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load lens flare %i", i);
	}
}

/**
 * @sa R_FreeWorldImages
 */
void R_ShutdownImages (void)
{
	int i;
	image_t* image;
	imageArray_t* images;

	R_CheckError();
	/* Wicked for()-loop (tm) */
	FOR_EACH_IMAGE(i, image, images) {
		if (!image->texnum)
			continue;			/* free image_t slot */
		R_DeleteImage(image);
	}
	OBJZERO(imageHash);
	OBJZERO(r_images);
	r_numImages = 0;
}

static void R_ReloadImageData (image_t* image)
{
	SDL_Surface* surf;
	if (image == r_noTexture || !image || !image->texnum)
		return;

	surf = Img_LoadImage(image->name);
	if (!surf) {
		Com_Printf("R_ReloadImageData: unable to load image %s\n", image->name);
		surf = SDL_CreateRGBSurface(0, image->width, image->height, 32,
					0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
		SDL_FillRect(surf, nullptr, 0x99ff33ff); /* A random color */
	}
	glGenTextures(1, &image->texnum);
	R_BindTexture(image->texnum);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	R_UploadTexture((unsigned* )surf->pixels, surf->w, surf->h, image);
	SDL_FreeSurface(surf);
}

void R_ReloadImages (void)
{
	int i;
	image_t* image;
	imageArray_t* images;

	R_CheckError();
	glEnable(GL_TEXTURE_2D);
	FOR_EACH_IMAGE(i, image, images) {
		if (i % 5 == 0) {
			SCR_DrawLoadingScreen(false, i * 100 / r_numImages);
		}
		R_ReloadImageData(image);
		R_ReloadImageData(image->normalmap);
		R_ReloadImageData(image->glowmap);
		R_ReloadImageData(image->specularmap);
		R_ReloadImageData(image->roughnessmap);
	}
}

typedef struct {
	const char* name;
	int minimize, maximize;
} glTextureMode_t;

static const glTextureMode_t gl_texture_modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST}, /* no filtering, no mipmaps */
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR}, /* bilinear filtering, no mipmaps */
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST}, /* no filtering, mipmaps */
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR}, /* bilinear filtering, mipmaps */
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST}, /* trilinear filtering, mipmaps */
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR} /* bilinear filtering, trilinear filtering, mipmaps */
};

void R_TextureMode (const char* string)
{
	int i;
	const size_t size = lengthof(gl_texture_modes);
	const glTextureMode_t* mode = nullptr;
	for (i = 0; i < size; i++) {
		mode = &gl_texture_modes[i];
		if (!Q_strcasecmp(mode->name, string))
			break;
	}

	if (mode == nullptr) {
		Com_Printf("bad filter name\n");
		return;
	}

	r_config.gl_filter_min = mode->minimize;
	r_config.gl_filter_max = mode->maximize;

	/* Wicked for()-loop (tm) */
	image_t* image;
	imageArray_t* images;
	FOR_EACH_IMAGE(i, image, images) {
		if (image->type == it_pic || image->type == it_worldrelated || image->type == it_chars)
			continue; /* no mipmaps */

		R_BindTexture(image->texnum);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mode->minimize);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode->maximize);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_config.maxAnisotropic);
		R_CheckError();
	}
}

#ifdef GL_VERSION_ES_CM_1_0
void R_TextureSolidMode (const char* string)
{
}
void R_TextureAlphaMode (const char* string)
{
}
#else
typedef struct {
	const char* name;
	int mode;
} gltmode_t;

static const gltmode_t gl_alpha_modes[] = {
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
	{"GL_LUMINANCE4_ALPHA4", GL_LUMINANCE4_ALPHA4},
	{"GL_LUMINANCE6_ALPHA2", GL_LUMINANCE6_ALPHA2},
	{"GL_LUMINANCE8_ALPHA8", GL_LUMINANCE8_ALPHA8},
	{"GL_LUMINANCE12_ALPHA4", GL_LUMINANCE12_ALPHA4},
	{"GL_LUMINANCE12_ALPHA12", GL_LUMINANCE12_ALPHA12},
	{"GL_LUMINANCE16_ALPHA16", GL_LUMINANCE16_ALPHA16}
};


void R_TextureAlphaMode (const char* string)
{
	const size_t size = lengthof(gl_alpha_modes);

	for (int i = 0; i < size; i++) {
		const gltmode_t* mode = &gl_alpha_modes[i];
		if (!Q_strcasecmp(mode->name, string)) {
			r_config.gl_alpha_format = mode->mode;
			return;
		}
	}

	Com_Printf("bad alpha texture mode name (%s)\n", string);
}

static const gltmode_t gl_solid_modes[] = {
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
	{"GL_RGB2", GL_RGB2_EXT},
	{"GL_RGB4", GL_RGB4_EXT},
	{"GL_RGB5", GL_RGB5_EXT},
	{"GL_RGB8", GL_RGB8_EXT},
	{"GL_RGB10", GL_RGB10_EXT},
	{"GL_RGB12", GL_RGB12_EXT},
	{"GL_RGB16", GL_RGB16_EXT},
	{"GL_LUMINANCE", GL_LUMINANCE},
	{"GL_LUMINANCE4", GL_LUMINANCE4},
	{"GL_LUMINANCE8", GL_LUMINANCE8},
	{"GL_LUMINANCE12", GL_LUMINANCE12},
	{"GL_LUMINANCE16", GL_LUMINANCE16}
};

void R_TextureSolidMode (const char* string)
{
	const size_t size = lengthof(gl_solid_modes);

	for (int i = 0; i < size; i++) {
		const gltmode_t* mode = &gl_solid_modes[i];
		if (!Q_strcasecmp(mode->name, string)) {
			r_config.gl_solid_format = mode->mode;
			return;
		}
	}

	Com_Printf("bad solid texture mode name (%s)\n", string);
}
#endif
