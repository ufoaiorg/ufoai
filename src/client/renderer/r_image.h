/**
 * @file r_image.h
 * @brief
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef R_IMAGE_H
#define R_IMAGE_H

#include "r_gl.h"
#include "r_material.h"
/*
skins will be outline flood filled and mip mapped
pics and sprites with alpha will be outline flood filled
pic won't be mip mapped

model skin
sprite frame
wall texture
pic
*/

typedef enum {
	it_chars,
	it_effect,
	it_static,
	it_pic,
	it_wrappic,
	it_skin,
	it_normalmap,
	it_glowmap,
	it_specularmap,
	it_roughnessmap,

	/** the following are freed with every mapchange */
	it_world,
	it_lightmap,
	it_deluxemap,
	it_material,
	it_worldrelated
} imagetype_t;

typedef struct image_s {
	char name[MAX_QPATH];				/**< game path, including extension, must be first */
	imagetype_t type;
	int width, height;					/**< source image dimensions */
	int upload_width, upload_height;	/**< dimensions after power of two and picmip */
	GLuint texnum;						/**< gl texture binding */
	qboolean has_alpha;
	material_t material;
	struct image_s *normalmap;			/**< normalmap texture  */
	struct image_s *glowmap;			/**< glowmap texture  */
	struct image_s *specularmap;		/**< specularity texture (for reflection color) */
	struct image_s *roughnessmap;		/**< roughness texture (for Cook-Torrance shading) */
	struct image_s *hash_next;			/**< hash map next pointer in case of collision */
} image_t;

#define MAX_GL_TEXTURES		1024
#define MAX_GL_LIGHTMAPS	256
#define MAX_GL_DELUXEMAPS	256
#define MAX_GL_FRAMEBUFFERS	256

extern int r_numImages;

void R_UploadTexture(unsigned *data, int width, int height, image_t* image);
void R_SoftenTexture(byte *in, int width, int height, int bpp);
void R_GetScaledTextureSize(int width, int height, int *scaledWidth, int *scaledHeight);
void R_ScaleTexture(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight);
image_t* R_RenderToTexture(const char *name, int x, int y, int w, int h);

void R_ImageList_f(void);
void R_InitImages(void);
void R_ShutdownImages(void);
void R_FreeWorldImages(void);
void R_ImageClearMaterials(void);
void R_UploadAlpha(const image_t *image, const byte *alphaData);
void R_TextureMode(const char *string);
void R_TextureAlphaMode(const char *string);
void R_TextureSolidMode(const char *string);

void R_FreeImage(image_t *image);

image_t *R_LoadImageData(const char *name, byte * pic, int width, int height, imagetype_t type);
image_t *R_GetImage(const char *name);
image_t *R_FindImage(const char *pname, imagetype_t type);
const image_t *R_FindPics(const char *name);

qboolean R_ImageExists(const char *pname, ...) __attribute__((format(printf, 1, 2)));
image_t *R_GetImageAtIndex(int i);
int R_GetImageIndex(image_t *image);

#define MAX_ENVMAPTEXTURES 2
extern image_t *r_envmaptextures[MAX_ENVMAPTEXTURES];

#define NUM_FLARETEXTURES 10
extern image_t *r_flaretextures[NUM_FLARETEXTURES];

extern image_t *shadow;	/**< draw this when actor is alive */
extern image_t *r_noTexture;
extern image_t *r_warpTexture;

#endif
