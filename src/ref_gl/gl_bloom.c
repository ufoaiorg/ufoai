/**
 * @file gl_bloom.c
 * @brief light bloom effect, shamelessly robbed from kmquake2
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

#include "gl_local.h"

static const float diamond[4][4] = {
	{0.3f, 0.4f, 0.4f, 0.3f},
	{0.4f, 0.9f, 0.9f, 0.4f},
	{0.4f, 0.9f, 0.9f, 0.4f},
	{0.3f, 0.4f, 0.4f, 0.3f}
};

static int bloom_size = 0;

static cvar_t *r_bloom;
static cvar_t *r_bloomalpha;
static cvar_t *r_bloomintensity;
static cvar_t *r_bloomsize;

static image_t *gl_bloomscreentexture = NULL;
static image_t *gl_bloomeffecttexture = NULL;
static image_t *gl_bloomoriginaltexture = NULL;
static image_t *gl_bloomdowntexture = NULL;

static int original_tex_size;

/* dimensions of the screen data */
static int screen_tex_width, screen_tex_height;
/* texture coordinates of screen data */
static float screen_texc_width, screen_texc_height;

/* dimensions of the downsampled data */
static int	sample_tex_width, sample_tex_height;
/* texture coordinates of downsampled data */
static float sample_texc_width, sample_texc_height;

/*this macro is in sample size workspace coordinates */
#define R_Bloom_SamplePass(x, y)									\
	qglBegin(GL_QUADS);												\
	qglTexCoord2f(	0,						sample_texc_height);	\
	qglVertex2f(	x,						y);						\
	qglTexCoord2f(	0,						0);						\
	qglVertex2f(	x,						y + sample_tex_height);	\
	qglTexCoord2f(	sample_texc_width,		0);						\
	qglVertex2f(	x + sample_tex_width,	y + sample_tex_height);	\
	qglTexCoord2f(	sample_texc_width,		sample_texc_height);	\
	qglVertex2f(	x + sample_tex_width,	y);						\
	qglEnd();

#define R_Bloom_Quad(x, y, width, height, tex_width, tex_height)	\
	qglBegin(GL_QUADS);												\
	qglTexCoord2f(	0,			tex_height);						\
	qglVertex2f(	x,			y);									\
	qglTexCoord2f(	0,			0);									\
	qglVertex2f(	x,			y + height);						\
	qglTexCoord2f(	tex_width,	0);									\
	qglVertex2f(	x + width,	y + height);						\
	qglTexCoord2f(	tex_width,	tex_height);						\
	qglVertex2f(	x + width,	y);									\
	qglEnd();


/**
 * @brief
 * @sa R_Bloom_InitTextures
 */
static void R_Bloom_InitOriginalTexture (int width, int height)
{
	byte *data;

	data = ri.TagMalloc(ri.imagePool, width * height * 4, 0);
	memset(data, 0, width * height * 4);

	original_tex_size = width;

	gl_bloomoriginaltexture = R_LoadPic("***gl_bloomoriginaltexture***",
			(byte *)data, width, height, it_pic, 32);

	ri.TagFree(data);
}


/**
 * @brief
 * @sa R_Bloom_InitTextures
 */
static void R_Bloom_InitEffectTexture (void)
{
	byte *data;
	float b;

	bloom_size = r_bloomsize->integer;

	/* enforce a reasonable minimum sample size */
	if (bloom_size < 16)
		bloom_size = 16;

	/* and maximum */
	if (bloom_size > screen_tex_width / 2 || bloom_size > screen_tex_height / 2)
		bloom_size = min(screen_tex_width / 2, screen_tex_height / 2);

	/* enforce that bloom size is a power of 2 */
	b = (float)bloom_size;
	while (b > 1.0f)
		b /= 2.0f;

	if (b != 1.0f) {
		bloom_size = 16;
		while (bloom_size < r_bloomsize->integer)
			bloom_size *= 2;
	}

	data = ri.TagMalloc(ri.imagePool, bloom_size * bloom_size * 4, 0);
	memset(data, 0, bloom_size * bloom_size * 4);
	gl_bloomeffecttexture = R_LoadPic("***gl_bloomeffecttexture***",
			(byte *)data, bloom_size, bloom_size, it_pic, 32);
	ri.TagFree(data);
}


/**
 * @brief
 * @sa R_Bloom_InitEffectTexture
 * @sa R_Bloom_InitOriginalTexture
 */
static void R_Bloom_InitTextures (void)
{
	byte *data;
	int size;

	if (gl_bloomscreentexture) {
		qglDeleteTextures(1, &gl_bloomscreentexture->texnum);
		memset(gl_bloomscreentexture, 0, sizeof(image_t));
	}
	if (gl_bloomeffecttexture) {
		qglDeleteTextures(1, &gl_bloomeffecttexture->texnum);
		memset(gl_bloomeffecttexture, 0, sizeof(image_t));
	}
	if (gl_bloomoriginaltexture) {
		qglDeleteTextures(1, &gl_bloomoriginaltexture->texnum);
		memset(gl_bloomoriginaltexture, 0, sizeof(image_t));
	}
	if (gl_bloomdowntexture) {
		qglDeleteTextures(1, &gl_bloomdowntexture->texnum);
		memset(gl_bloomdowntexture, 0, sizeof(image_t));
	}

	/* find closer power of 2 to screen size */
	for (screen_tex_width = 1; screen_tex_width < vid.width; screen_tex_width *= 2);
	for (screen_tex_height = 1; screen_tex_height < vid.height; screen_tex_height *= 2);

	/* init the screen texture */
	size = screen_tex_width * screen_tex_height * 4;
	data = ri.TagMalloc(ri.imagePool, size, 0);
	memset(data, 0, size);
	gl_bloomscreentexture = R_LoadPic("***gl_bloomscreentexture***",
			(byte *)data, screen_tex_width, screen_tex_height, it_pic, 32);
	ri.TagFree(data);

	/* validate bloom size and init the bloom effect texture */
	R_Bloom_InitEffectTexture();

	/* init the downsample texture */
	data = ri.TagMalloc(ri.imagePool, bloom_size * bloom_size * 4, 0);
	memset(data, 0, bloom_size * bloom_size * 4);
	gl_bloomdowntexture = R_LoadPic("***gl_bloomdowntexture***",
			(byte *)data, bloom_size, bloom_size, it_pic, 32);
	ri.TagFree(data);

	/* init the screen original texture */
	R_Bloom_InitOriginalTexture(bloom_size, bloom_size);
}


/**
 * @brief
 * @sa R_Bloom_InitTextures
 */
void R_InitBloom (void)
{
	r_bloom = ri.Cvar_Get("r_bloom", "0", CVAR_ARCHIVE, "Activate light blooms");
	r_bloomalpha = ri.Cvar_Get("r_bloomalpha", "0.4", CVAR_ARCHIVE, NULL);
	r_bloomintensity = ri.Cvar_Get("r_bloomintensity", "0.2", CVAR_ARCHIVE, NULL);
	r_bloomsize = ri.Cvar_Get("r_bloomsize", "256", CVAR_ARCHIVE, NULL);

	bloom_size = 0;

	if (!r_bloom->integer)
		return;

	R_Bloom_InitTextures();
}


/**
 * @brief
 */
static void R_Bloom_DrawEffect (void)
{
	float a;

	a = r_bloomalpha->value;

	R_Bind(gl_bloomeffecttexture->texnum);
	RSTATE_ENABLE_BLEND
	qglBlendFunc(GL_ONE, GL_ONE);
	qglColor4f(a, a, a, 1.0f);
	R_TexEnv(GL_MODULATE);

	qglBegin(GL_QUADS);
	qglTexCoord2f(0, sample_texc_height);
	qglVertex2f(0, 0);
	qglTexCoord2f(0, 0);
	qglVertex2f(0, vid.height);
	qglTexCoord2f(sample_texc_width, 0);
	qglVertex2f(vid.width, vid.height);
	qglTexCoord2f(sample_texc_width, sample_texc_height);
	qglVertex2f(vid.width, 0);
	qglEnd();

	RSTATE_DISABLE_BLEND
}


/**
 * @brief
 */
static void R_Bloom_GenerateBlooms (void)
{
	int i, j;
	float intensity;

	/* set up sample size workspace */
	qglViewport(0, 0, sample_tex_width, sample_tex_height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, sample_tex_width, sample_tex_height, 0, -10, 100);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	/* copy small scene into effect texture */
	R_Bind(gl_bloomeffecttexture->texnum);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_tex_width, sample_tex_height);

	RSTATE_ENABLE_BLEND

	/* one darkening pass */
	qglBlendFunc(GL_DST_COLOR, GL_ZERO);
	R_TexEnv(GL_MODULATE);
	R_Bloom_SamplePass(0, 0);

	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_tex_width, sample_tex_height);

	/* 4 bluring passes */
	qglBlendFunc(GL_ONE, GL_ONE);

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			intensity = r_bloomintensity->value * 0.8f * diamond[i][j];
			qglColor4f(intensity, intensity, intensity, 1.0);
			R_Bloom_SamplePass(i - 2, j - 2);
		}
	}

	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_tex_width, sample_tex_height);

	/* restore screen workspace */
	qglViewport(0, 0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, -10, 100);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
}


/**
 * @brief
 * @sa R_BloomBlend
 */
static void R_Bloom_DownsampleView (void)
{
	int midsample_tex_width = bloom_size * sample_texc_width;
	int midsample_tex_height = bloom_size * sample_texc_height;

	/* copy the screen texture into bloom space and blur it */
	R_Bind(gl_bloomscreentexture->texnum);

	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, vid.width, vid.height);

	R_Bloom_Quad(0, vid.height - midsample_tex_height, midsample_tex_width,
			midsample_tex_height, screen_texc_width, screen_texc_height);

	R_Bind(gl_bloomdowntexture->texnum);

	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
			midsample_tex_width, midsample_tex_height);

	R_Bloom_Quad(0, vid.height - sample_tex_height, sample_tex_width,
			sample_tex_height, sample_texc_width, sample_texc_height);

	/* blend screen texture into bloom space for final rendering */
	RSTATE_ENABLE_BLEND
	qglBlendFunc(GL_ONE, GL_ONE);

	R_Bind(gl_bloomscreentexture->texnum);

	R_Bloom_Quad(0, vid.height - sample_tex_height, sample_tex_width, sample_tex_height,
			screen_texc_width, screen_texc_height);

	RSTATE_DISABLE_BLEND
}


/**
 * @brief
 */
void R_BloomBlend (void)
{
	/* if params have changed, update texture sizes */
	if (r_bloom->modified || r_bloomsize->modified) {
		R_Bloom_InitTextures();
		r_bloom->modified = r_bloomsize->modified = qfalse;
	}

	if (!r_bloom->integer)
		return;

	if (screen_tex_width < bloom_size || screen_tex_height < bloom_size)
		return;

	/* set up full screen workspace */
	qglViewport(0, 0, vid.width, vid.height);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);
	RSTATE_DISABLE_BLEND
	qglEnable(GL_TEXTURE_2D);

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, -10, 100);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	/* set up sizes and texture coordinates for screen dumps and resampling */
	screen_texc_width = ((float)vid.width / (float)screen_tex_width);
	screen_texc_height = ((float)vid.height / (float)screen_tex_height);
	sample_texc_width = 1.0f;
	sample_texc_height = ((float)vid.height / (float)vid.width);
	sample_tex_width = bloom_size * sample_texc_width;
	sample_tex_height = bloom_size * sample_texc_height;

	/* copy the screen into the original texture */
	R_Bind(gl_bloomoriginaltexture->texnum);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, original_tex_size * sample_texc_width,
			original_tex_size * sample_texc_height);

	/* downsample the screen image to a workable size */
	R_Bloom_DownsampleView();

	/* create the blooms */
	R_Bloom_GenerateBlooms();

	/* restore the screen */
	RSTATE_DISABLE_BLEND
	R_Bind(gl_bloomoriginaltexture->texnum);
	qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	R_Bloom_Quad(0, vid.height - (original_tex_size * sample_texc_height),
			original_tex_size * sample_texc_width,
			original_tex_size * sample_texc_height,
			sample_texc_width, sample_texc_height);

	/* blend in the blooms */
	R_Bloom_DrawEffect();
}

