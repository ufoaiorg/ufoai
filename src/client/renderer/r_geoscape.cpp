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
#include "r_sphere.h"
#include "r_geoscape.h"

#include "r_mesh.h"
#include "r_draw.h"

#define MARKER_SIZE 60.0

/**
 * @brief Draw the day and night images of a flat geoscape
 * multitexture feature is used to blend the images
 * @sa R_Draw3DGlobe
 * @param[in] nodePos The position, normalized (1024x768)
 * @param[in] nodeSize The dimensions, normalized (1024x768)
 * @param[in] p The horizontal shift of the night map
 * @param[in] cx,cy The x/y texture coordinates
 * @param[in] iz The zoomlevel of the geoscape - see ccs.zoom
 * @param[in] map The geoscape map to draw (can be changed in the campaign definition)
 * @param[in] overlayNation,overlayXVI,overlayRadar Whether these overlays should be drawn or not
 * @param[in] r_dayandnightTexture,r_xviTexture,r_radarTexture The textures used for the corresponding overlays
 */
void R_DrawFlatGeoscape (const vec2_t nodePos, const vec2_t nodeSize, float p, float cx, float cy, float iz, const char* map, bool overlayNation, bool overlayXVI, bool overlayRadar, image_t* r_dayandnightTexture, image_t* r_xviTexture, image_t* r_radarTexture)
{
	image_t* gl;
	float geoscape_texcoords[4 * 2];
	short geoscape_verts[4 * 2];

	/* normalize */
	const float nx = nodePos[0] * viddef.rx;
	const float ny = nodePos[1] * viddef.ry;
	const float nw = nodeSize[0] * viddef.rx;
	const float nh = nodeSize[1] * viddef.ry;

	/* load day image */
	gl = R_FindImage(va("pics/geoscape/%s_day", map), it_wrappic);
	if (gl == r_noTexture)
		Com_Error(ERR_FATAL, "Could not load geoscape day image");

	/* alter the array pointers */
	glVertexPointer(2, GL_SHORT, 0, geoscape_verts);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, geoscape_texcoords);

	geoscape_texcoords[0] = cx - iz;
	geoscape_texcoords[1] = cy - iz;
	geoscape_texcoords[2] = cx + iz;
	geoscape_texcoords[3] = cy - iz;
	geoscape_texcoords[4] = cx + iz;
	geoscape_texcoords[5] = cy + iz;
	geoscape_texcoords[6] = cx - iz;
	geoscape_texcoords[7] = cy + iz;

	geoscape_verts[0] = nx;
	geoscape_verts[1] = ny;
	geoscape_verts[2] = nx + nw;
	geoscape_verts[3] = ny;
	geoscape_verts[4] = nx + nw;
	geoscape_verts[5] = ny + nh;
	geoscape_verts[6] = nx;
	geoscape_verts[7] = ny + nh;

	/* draw day image */
	R_BindTexture(gl->texnum);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	refdef.batchCount++;

	/* draw night map */
	gl = R_FindImage(va("pics/geoscape/%s_night", map), it_wrappic);
	/* maybe the campaign map doesn't have a night image */
	if (gl != r_noTexture) {
		float geoscape_nighttexcoords[4 * 2];

		R_BindTexture(gl->texnum);
		R_EnableTexture(&texunit_lightmap, true);
		R_SelectTexture(&texunit_lightmap);

		geoscape_nighttexcoords[0] = geoscape_texcoords[0] + p;
		geoscape_nighttexcoords[1] = geoscape_texcoords[1];
		geoscape_nighttexcoords[2] = geoscape_texcoords[2] + p;
		geoscape_nighttexcoords[3] = geoscape_texcoords[3];
		geoscape_nighttexcoords[4] = geoscape_texcoords[4] + p;
		geoscape_nighttexcoords[5] = geoscape_texcoords[5];
		geoscape_nighttexcoords[6] = geoscape_texcoords[6] + p;
		geoscape_nighttexcoords[7] = geoscape_texcoords[7];

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, geoscape_nighttexcoords);

		R_BindTexture(r_dayandnightTexture->texnum);

		R_SelectTexture(&texunit_diffuse);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		refdef.batchCount++;

		R_SelectTexture(&texunit_lightmap);
		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, geoscape_texcoords);

		R_EnableTexture(&texunit_lightmap, false);
	}

	/* draw nation overlay */
	if (overlayNation) {
		gl = R_FindImage(va("pics/geoscape/%s_nations_overlay", map), it_wrappic);
		if (gl == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load geoscape nation overlay image");

		/* draw day image */
		R_BindTexture(gl->texnum);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		refdef.batchCount++;
	}

	/* draw XVI image */
	if (overlayXVI) {
		gl = R_FindImage(va("pics/geoscape/%s_xvi_overlay", map), it_wrappic);
		if (gl == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load xvi overlay image");

		R_BindTexture(gl->texnum);

		R_EnableTexture(&texunit_lightmap, true);
		R_BindLightmapTexture(r_xviTexture->texnum);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		refdef.batchCount++;

		R_EnableTexture(&texunit_lightmap, false);
	}

	/* draw radar image */
	if (overlayRadar) {
		R_BindTexture(r_radarTexture->texnum);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		refdef.batchCount++;
	}

	/* and restore them */
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
}

/**
 * @brief Draw 3D Marker on the 2D geoscape.
 * @param[in] screenPos Position on screenlongitude and latitude of the model to draw.
 * @param[in] direction angle giving the direction the model is heading toward.
 * @param[in] model The name of the model of the marker.
 * @param[in] skin Number of modelskin to draw on marker
 */
void R_Draw2DMapMarkers (const vec2_t screenPos, float direction, const char* model, int skin)
{
	modelInfo_t mi;
	vec2_t size;
	vec3_t scale, center, position, angles;
	float zoom = 0.4f;

	OBJZERO(mi);
	VectorCopy(vec3_origin, position);
	VectorCopy(vec3_origin, angles);

	mi.model = R_FindModel(model);
	if (!mi.model) {
		Com_Printf("Could not find model '%s'\n", model);
		return;
	}

	mi.name = model;
	mi.origin = position;
	mi.angles = angles;
	mi.skin = skin;

	size[0] = size[1] = MARKER_SIZE * zoom;
	R_ModelAutoScale(size, &mi, scale, center);
	/* reset the center, as we want to place the models onto the surface of the earth */
	mi.center = nullptr;

	/* go to a new matrix */
	glPushMatrix();

	/* Apply all transformation to model. Note that the transformations are applied starting
	 * from the last one and ending with the first one */

	/* move model to its location */
	glTranslatef(screenPos[0]* viddef.rx, screenPos[1]* viddef.ry, 0);
	/* scale model to proper resolution */
	glScalef(viddef.rx, viddef.ry, 1.0f);
	/* rotate model to proper direction. */
	glRotatef(-90.f + direction, 0, 0, 1);

	R_DrawModelDirect(&mi, nullptr, nullptr);

	/* restore previous matrix */
	glPopMatrix();
}

/**
 * @brief Draw 3D Marker on the 3D geoscape.
 * @param[in] nodePos The position, normalized (1024x768)
 * @param[in] nodeSize The dimensions, normalized (1024x768)
 * @param[in] rotate vector giving the angles of earth rotation due to player view.
 * @param[in] pos longitude and latitude of the model to draw.
 * @param[in] direction angle giving the direction the model is heading toward.
 * @param[in] earthRadius Radius of earth on screen (this include zoom).
 * @param[in] model The name of the model of the marker.
 * @param[in] skin Number of modelskin to draw on marker
 */
void R_Draw3DMapMarkers (const vec2_t nodePos, const vec2_t nodeSize, const vec3_t rotate, const vec2_t pos, float direction, float earthRadius, const char* model, int skin)
{
	/* normalize */
	const float nx = nodePos[0] * viddef.rx;
	const float ny = nodePos[1] * viddef.ry;
	const float nw = nodeSize[0] * viddef.rx;
	const float nh = nodeSize[1] * viddef.ry;

	/* Earth center is in the middle of node.
	 * Due to Orthographic view, this is also camera position */
	const vec3_t earthPos = {nx + nw / 2.0f, ny + nh / 2.0f, 0.0f};

	modelInfo_t mi;
	vec2_t size;
	vec3_t scale, center, position, angles;
	float zoom = 0.4f;

	OBJZERO(mi);
	VectorCopy(vec3_origin, position);
	VectorCopy(vec3_origin, angles);

	mi.model = R_FindModel(model);
	if (!mi.model) {
		Com_Printf("Could not find model '%s'\n", model);
		return;
	}

	mi.name = model;
	mi.origin = position;
	mi.angles = angles;
	mi.skin = skin;

	size[0] = size[1] = MARKER_SIZE * zoom;
	R_ModelAutoScale(size, &mi, scale, center);
	/* reset the center, as we want to place the models onto the surface of the earth */
	mi.center = nullptr;

	/* go to a new matrix */
	glPushMatrix();

	/* Apply all transformation to model. Note that the transformations are applied starting
	 * from the last one and ending with the first one */

	/* center model on earth. Translate also along z to avoid seeing
	 * bottom part of the model through earth (only half of earth is drawn) */
	glTranslatef(earthPos[0], earthPos[1], 10.0f);
	/* scale model to proper resolution */
	glScalef(viddef.rx, viddef.ry, 1.0f);
	/* place model on earth: make it tangent to earth surface, heading toward it if direction is used. */
	glRotatef(-rotate[1], 1, 0, 0);
	glRotatef(rotate[2], 0, 1, 0);
	glRotatef(rotate[0] - pos[0], 0, 0, 1);
	glRotatef(90.0f - pos[1], 1, 0, 0);
	glTranslatef(0, 0, earthRadius);
	glRotatef(90.0f + direction, 0, 0, 1);

	R_DrawModelDirect(&mi, nullptr, nullptr);

	/* restore previous matrix */
	glPopMatrix();
}

/**
 * @brief Half size of Skybox.
 * @note The bigger, the less perspective default you'll have, but the more you'll
 * zoom on the texture (and see it's default).
 * @sa R_DrawStarfield
 * @sa R_Setup2D
 */
#define SKYBOX_HALFSIZE 800.0f

static const float starFieldVerts[] = {
	/* face 1 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 2 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,

	/* face 3 */
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 4 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 5 */
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,

	/* face 6 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE
};

static const float starFieldTexCoords[] = {
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
};

/**
 * @brief Bind and draw starfield.
 * @param[in] texnum The texture id (already uploaded of course)
 * @param[in] pos The earth position
 * @param[in] rotate the rotate angle of the globe
 * @param[in] timeOfDay The earth rotation (time of day) in arcseconds
 * @note We draw a skybox: the camera is inside a cube rotating at earth rotation speed
 * (stars seems to rotate because we see earth as idle, but in reality stars are statics
 * and earth rotate around itself)
 * @sa R_Setup2D
 * @sa R_Draw3DGlobe
 */
static void R_DrawStarfield (int texnum, const vec3_t pos, const vec3_t rotate, float timeOfDay)
{
	vec3_t angle;		/**< Angle of rotation of starfield */

	/* go to a new matrix */
	glPushMatrix();

	/* we must center the skybox on the camera border of view, and not on the earth, in order
	 * to see only the inside of the cube */
	glTranslatef(pos[0], pos[1], -SKYBOX_DEPTH);

	/* rotates starfield: only time and rotation of earth around itself causes starfield to rotate. */
	VectorSet(angle, rotate[0] - timeOfDay * todeg, rotate[1], rotate[2]);
	glRotatef(angle[YAW], 1, 0, 0);
	glRotatef(angle[ROLL], 0, 1, 0);
	glRotatef(angle[PITCH], 0, 0, 1);

	R_BindTexture(texnum);

	/* alter the array pointers */
	glVertexPointer(3, GL_FLOAT, 0, starFieldVerts);
	glTexCoordPointer(2, GL_FLOAT, 0, starFieldTexCoords);

	/* draw the cube */
#ifdef GL_VERSION_ES_CM_1_0
	for( int ii = 0; ii < 6; ii++ )
		glDrawArrays(GL_TRIANGLE_FAN, ii * 4, 4);
#else
	glDrawArrays(GL_QUADS, 0, 24);
#endif

	refdef.batchCount++;

	/* restore previous matrix */
	glPopMatrix();
}

/**
 * @brief rotate a planet (sun or moon) with respect to the earth
 */
static inline void R_RotateCelestialBody (const vec4_t v, vec4_t r, const vec3_t rotate, const vec3_t earthPos, const float celestialDist)
{
	vec4_t v1;
	vec3_t v2;
	vec3_t rotationAxis;

	VectorSet(v2, v[1], v[0], v[2]);
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v1, rotationAxis, v2, -rotate[PITCH]);
	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v2, rotationAxis, v1, -rotate[YAW]);

	Vector4Set(r, earthPos[0] + celestialDist * v2[1], earthPos[1] + celestialDist * v2[0], -celestialDist * v2[2], 0);
}

/**
 * @brief responsible for drawing the 3d globe on geoscape
 * param[in] rotate the rotate angle of the globe
 * param[in] zoom the current globe zoon
 * param[in] map the prefix of the map to use (image must be at base/pics/menu/\<map\>_[day|night])
 * @sa R_DrawFlatGeoscape
 * @sa R_SphereGenerate
 */
void R_Draw3DGlobe (const vec2_t pos, const vec2_t size, int day, int second, const vec3_t rotate, float zoom, const char* map,
					bool disableSolarRender, float ambient, bool overlayNation, bool overlayXVI, bool overlayRadar, image_t* r_xviTexture,
					image_t* r_radarTexture, bool renderNationGlow)
{
	/* globe scaling */
	const float fullscale = zoom / STANDARD_3D_ZOOM;

	/* lighting colors */
	static const vec4_t diffuseLightColor = { 1.75f, 1.75f, 1.75f, 1.0f };
	static const vec4_t specularLightColor = { 2.0f, 1.9f, 1.7f, 1.0f };
	static const vec4_t darknessLightColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	static const vec4_t brightDiffuseLightColor = { 5.0f, 5.0f, 5.0f, 1.0f };
	const vec4_t ambientLightColor = { ambient + 0.2f, ambient + 0.2f, ambient + 0.2f, ambient + 0.2f };
	/* billboard textures */
	image_t* starfield;
	image_t* sun;
	image_t* sunOverlay;

	/* set distance of the sun and moon to make them static on starfield when
	 * time is stoped.  this distance should be used for any celestial body
	 * considered at infinite location (sun, moon) */
	static const float celestialDist = 1.37f * SKYBOX_HALFSIZE;
	static const float moonSize = 0.025f;
	vec4_t sunPos;
	vec4_t antiSunPos;
	vec4_t moonLoc;
	vec4_t sunLoc;

	/* normalize */
	const float nx = pos[0] * viddef.rx;
	const float ny = pos[1] * viddef.ry;
	const float nw = size[0] * viddef.rx;
	const float nh = size[1] * viddef.ry;

	/* Earth center is in the middle of node.
	 * Due to Orthographic view, this is also camera position */
	const vec3_t earthPos = { nx + nw / 2.0f, ny + nh / 2.0f, 0.0f };

	/* estimate the progress through the current season so we can do
	 * smooth transitions between textures.  Currently there are 12
	 * "seasons", because we have one image per Earth-month. */
	const float season = (float) (day % DAYS_PER_YEAR) / ((float) (DAYS_PER_YEAR) / (float) (SEASONS_PER_YEAR));
	const int currSeason = (int) floorf(season) % SEASONS_PER_YEAR;
	const int nextSeason = (int) ceilf(season) % SEASONS_PER_YEAR;
	const float seasonProgress = season - (float) currSeason;

	/* Compute sun position in absolute frame */
	const float q = (day % DAYS_PER_YEAR * SECONDS_PER_DAY + second) * (2.0f * M_PI / (SECONDS_PER_DAY * DAYS_PER_YEAR));	/* sun rotation (year) */
	const float a = cos(q) * SIN_ALPHA;	/* due to earth obliquity */
	const float sqrta = sqrt(0.5f * (1 - a * a));

	/* earth rotation (day) */
	const float p = (second - SECONDS_PER_DAY / 4) * (2.0f * M_PI / SECONDS_PER_DAY);
	/* lunar orbit */
	const float m = p + (((double)((10 * day % 249) / 10.0f) + ((double)second / (double)SECONDS_PER_DAY)) / 24.9f) * (2.0f * M_PI);

	glPushMatrix();
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_LIGHTING);
	/* draw the starfield, rotating with the planet */
	starfield = R_FindImage(va("pics/geoscape/%s_stars", map), it_wrappic);
	if (starfield != r_noTexture)
		R_DrawStarfield(starfield->texnum, earthPos, rotate, p);

	glPopMatrix();

	/* set up position vectors for celestial bodies */
	Vector4Set(sunPos, cos(p) * sqrta, -sin(p) * sqrta, a, 0);
	Vector4Set(antiSunPos, -cos(p) * sqrta, sin(p) * sqrta, -a, 0);

	/* Rotate the sun in the relative frame of player view, to get sun location */
	R_RotateCelestialBody(sunPos, sunLoc, rotate, earthPos, 1.0f);
	/* load sun texture image */
	sun = R_FindImage(va("pics/geoscape/%s_sun", map), it_wrappic);
	sunOverlay = R_FindImage(va("pics/geoscape/%s_sun_overlay", map), it_pic);
	if (sun != r_noTexture && sunOverlay != r_noTexture && sunLoc[2] > 0 && !disableSolarRender) {
		const int sunx = earthPos[0] + viddef.rx * (-128.0f + celestialDist * (sunLoc[0] - earthPos[0]));
		const int suny = earthPos[1] + viddef.ry * (-128.0f + celestialDist * (sunLoc[1] - earthPos[1]));

		R_DrawTexture(sunOverlay->texnum, sunx, suny, 256.0f * viddef.rx, 256.0f * viddef.ry);
		R_DrawBuffers(2);
		R_DrawTexture(sun->texnum, sunx, suny, 256.0 * viddef.rx, 256.0 * viddef.ry);
		R_DrawBuffers(1);
	}

	/* calculate position of the moon (it rotates around earth with a period of
	 * about 24.9 h, and we must take day into account to avoid moon to "jump"
	 * every time the day is changing) */
	VectorSet(moonLoc, cos(m) * sqrta, -sin(m) * sqrta, a);
	R_RotateCelestialBody(moonLoc, moonLoc, rotate, earthPos, celestialDist);

	/* free last month's texture image */
	if (r_globeEarth.season != currSeason) {
		r_globeEarth.season = currSeason;
		R_FreeImage(r_globeEarth.texture);
	}

	/* load diffuse texture map (with embedded night-glow map as alpha channel) */
	r_globeEarth.texture = R_FindImage(va("pics/geoscape/%s/%s_season_%02d", r_config.lodDir, map, currSeason), it_wrappic);
	if (r_globeEarth.texture == r_noTexture)
		Com_Error(ERR_FATAL, "Could not find pics/geoscape/%s/%s_season_%02d\n", r_config.lodDir, map, currSeason);

	/* set up for advanced GLSL rendering if we have the capability */
	if (r_programs->integer) {
		r_globeEarth.glslProgram = r_state.geoscape_program;
		/* load earth image for the next month so we can blend them */
		r_globeEarth.blendTexture = R_FindImage(va("pics/geoscape/%s/%s_season_%02d", r_config.lodDir, map, nextSeason), it_wrappic);
		if (r_globeEarth.blendTexture == r_noTexture)
			Com_Error(ERR_FATAL, "Could not find pics/geoscape/%s/%s_season_%02d\n", r_config.lodDir, map, nextSeason);

		/* load normal map (with embedded gloss map as alpha channel) */
		r_globeEarth.normalMap = R_FindImage(va("pics/geoscape/%s/%s_bump", r_config.lodDir, map), it_wrappic);
		if (r_globeEarth.normalMap == r_noTexture)
			r_globeEarth.normalMap = nullptr;

		/* weight the blending based on how much of the month has elapsed */
		r_globeEarth.blendScale = seasonProgress;
		/* set up lights for nighttime city glow */
		VectorCopy(antiSunPos, r_globeEarth.nightLightPos);
		glLightfv(GL_LIGHT1, GL_AMBIENT, darknessLightColor);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, brightDiffuseLightColor);
		glLightfv(GL_LIGHT1, GL_SPECULAR, darknessLightColor);

		r_globeEarth.glowScale = 0.7f;
	}

	/* load moon texture image */
	r_globeMoon.texture = R_FindImage(va("pics/geoscape/%s_moon", map), it_wrappic);

	/* globe texture scaling */
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(2.0f, 1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);

	/* enable the lighting */
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLightColor);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLightColor);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularLightColor);

	/* draw the moon */
	if (r_globeMoon.texture != r_noTexture && moonLoc[2] > 0 && !disableSolarRender)
		R_SphereRender(&r_globeMoon, moonLoc, rotate, moonSize, sunPos);

	/* activate depth to hide 3D models behind earth */
	glEnable(GL_DEPTH_TEST);

	/* draw the earth */
	R_DrawBuffers(2);
#if 0 /* old rendering code which doesn't render city lights in FFP */
	if (r_programs->integer == 0) /* ignore alpha channel, since the city-light map is stored there */
		glBlendFunc(GL_ONE, GL_ZERO);

	R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);

	if (r_programs->integer == 0) /* restore default blend function */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#else /* new which does render city lights in FFP */
	if (r_programs->integer == 0) {
		/* set up rendering of city lights map, which is stored in alpha channel; OpenGL 1.3 required */
		R_SelectTexture(&texunit_diffuse); /* select texture to edit texture environment for */
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE); /* enable color combiner */
		/* setup texture combiner to blend between daylight diffuse map stored in the RGB channels of the diffuse texture
		 * and the monochomatic emission map (which simulates city lights) stored in the alpha channel;
		 * incoming color value is the blend factor.
		 */
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE); /* set day color as blending target*/
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE); /* set night color as blending source */
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_PREVIOUS); /* set incoming color as blending factor */
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE); /* set blending mode to interpolation from src1 to src0 */
		/* copy alpha from incoming color, bypassing the value read from texture, which is not a "real" alpha anyway */
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}

	R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);

	if (r_programs->integer == 0) { /* disable combiner */
		R_SelectTexture(&texunit_diffuse);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
#endif

	r_globeEarthAtmosphere.texture = R_FindImage(va("pics/geoscape/%s_atmosphere", map), it_wrappic);

	/* Draw earth atmosphere */
	/** @todo render atmosphere glow even when postprocessing is disabled */
	if (r_programs->integer && r_postprocess->integer) {
		r_globeEarthAtmosphere.normalMap = r_globeEarth.normalMap;
		r_globeEarthAtmosphere.glowScale = 1.0;
		r_globeEarthAtmosphere.blendScale = -1.0;
		r_globeEarthAtmosphere.glslProgram = r_state.atmosphere_program;
		R_SphereRender(&r_globeEarthAtmosphere, earthPos, rotate, fullscale, sunPos);
	} else {
		image_t* halo = R_FindImage("pics/geoscape/map_earth_halo", it_pic);
		if (halo != r_noTexture) {
			/** @todo Replace this magic number with some speaking constant */
			const float earthSizeX = fullscale * 20500.0f * viddef.rx;
			const float earthSizeY = fullscale * 20500.0f * viddef.ry;
			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			glLoadIdentity();
			glDisable(GL_LIGHTING);

			R_DrawTexture(halo->texnum, earthPos[0] - earthSizeX * 0.5f, earthPos[1] - earthSizeY * 0.5f, earthSizeX, earthSizeY);
			glEnable(GL_LIGHTING);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
		}
	}

	R_DrawBuffers(1);
	glDisable(GL_DEPTH_TEST);

	/* draw nation overlay */
	if (overlayNation) {
		r_globeEarth.overlay = R_FindImage(va("pics/geoscape/%s_nations_overlay", map), it_wrappic);
		if (r_globeEarth.overlay == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load geoscape nation overlay image");

		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);

		if (renderNationGlow) {
			/* draw glowing borders */
			r_globeEarth.overlay = R_FindImage(va("pics/geoscape/%s_nations_overlay_glow", map), it_wrappic);
			if (r_globeEarth.overlay == r_noTexture)
				Com_Error(ERR_FATAL, "Could not load geoscape nation overlay glow image");

			R_DrawBuffers(2);
			glDisable(GL_LIGHTING);
			R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);
			glEnable(GL_LIGHTING);
			R_DrawBuffers(1);
		}

		r_globeEarth.overlay = nullptr;
	}
	/* draw XVI overlay */
	if (overlayXVI) {
		r_globeEarth.overlay = R_FindImage(va("pics/geoscape/%s_xvi_overlay", map), it_wrappic);
		r_globeEarth.overlayAlphaMask = r_xviTexture;
		assert(r_globeEarth.overlayAlphaMask);
		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);
		r_globeEarth.overlayAlphaMask = nullptr;
		r_globeEarth.overlay = nullptr;
	}
	/* draw radar overlay */
	if (overlayRadar) {
		r_globeEarth.overlay = r_radarTexture;
		assert(r_globeEarth.overlay);
		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, sunPos);
		r_globeEarth.overlay = nullptr;
	}

	/* disable 3d geoscape lighting */
	glDisable(GL_LIGHTING);

	/* restore the previous matrix */
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}

/**
 * @brief Draw the current texture on a quad the size of the renderbuffer
 */
static inline void R_DrawQuad (void)
{
	/** @todo use default_texcoords */
	const vec2_t texcoord[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };
	const vec2_t verts[] = { { 0.0f, 0.0f }, Vector2FromInt(fbo_render->width, 0.0f), Vector2FromInt(fbo_render->width, fbo_render->height), Vector2FromInt(0.0f, fbo_render->height) };

	glVertexPointer(2, GL_FLOAT, 0, verts);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, texcoord);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	refdef.batchCount++;

	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
}

/**
 * @brief does 1D filter convolution to blur a framebuffer texture.
 * dir=0 for horizontal, dir=1 for vertical
 */
static void R_Blur (r_framebuffer_t* source, r_framebuffer_t* dest, int tex, int dir)
{
	R_EnableBlur(r_state.convolve_program, true, source, dest, dir);

	/* draw new texture onto a flat surface */
	R_BindTextureForTexUnit(source->textures[tex], &texunit_0);
	R_UseViewport(source);
	R_DrawQuad();

	R_EnableBlur(r_state.convolve_program, false, nullptr, nullptr, 0);
}

/**
 * @brief blur from the source image pyramid into the dest image pyramid
 */
static void R_BlurStack (int levels, r_framebuffer_t** sources, r_framebuffer_t** dests)
{
	for (int i = 0; i < levels; i++) {
		const int l = levels - i - 1;

		R_UseProgram(i == 0 ? default_program : r_state.combine2_program);
		R_UseFramebuffer(dests[l]);
		R_BindTextureForTexUnit(sources[l]->textures[0], &texunit_0);
		if (i != 0)
			R_BindTextureForTexUnit(dests[l + 1]->textures[0], &texunit_1);

		R_UseViewport(sources[l]);
		R_DrawQuad();

		R_Blur(dests[l], sources[l], 0, 1);
		R_Blur(sources[l], dests[l], 0, 0);
	}
}

/**
 * @brief handle post-processing bloom
 */
void R_DrawBloom (void)
{
	int i;
	bool renderBufferState;

	if (!r_config.frameBufferObject || !r_postprocess->integer || !r_programs->integer)
		return;

	/* save state, then set up for blit-style rendering to quads */
	renderBufferState = R_RenderbufferEnabled();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
#ifndef GL_VERSION_ES_CM_1_0
	glPushAttrib(GL_ENABLE_BIT | GL_VIEWPORT_BIT | GL_LIGHTING_BIT | GL_DEPTH_BUFFER_BIT);
#endif
	glOrtho(0, viddef.context.width, viddef.context.height, 0, 9999.0f, SKYBOX_DEPTH);

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	/* downsample into image pyramid */
	R_ResolveMSAA(fbo_render);
	R_BindTexture(fbo_render->textures[1]);
	qglGenerateMipmapEXT(GL_TEXTURE_2D);

	R_Blur(fbo_render, fbo_bloom0, 1, 0);
	R_Blur(fbo_bloom0, fbo_bloom1, 0, 1);

	R_UseFramebuffer(r_state.buffers0[0]);
	R_BindTexture(fbo_bloom1->textures[0]);
	qglGenerateMipmapEXT(GL_TEXTURE_2D);
	R_UseViewport(r_state.buffers0[0]);
	R_DrawQuad();

	for (i = 1; i < DOWNSAMPLE_PASSES; i++) {
		R_Blur(r_state.buffers0[i - 1], r_state.buffers1[i - 1], 0, 0);
		R_Blur(r_state.buffers1[i - 1], r_state.buffers2[i - 1], 0, 1);
		R_UseFramebuffer(r_state.buffers0[i]);
		R_BindTexture(r_state.buffers2[i - 1]->textures[0]);
		R_UseViewport(r_state.buffers0[i]);
		R_DrawQuad();
	}

	/* blur and combine downsampled images */
	R_BlurStack(DOWNSAMPLE_PASSES, r_state.buffers0, r_state.buffers1);

	/* re-combine the blurred version with the original "glow" image */
	R_UseProgram(r_state.combine2_program);
	R_UseFramebuffer(fbo_bloom0);
	R_BindTextureForTexUnit(fbo_render->textures[1], &texunit_0);
	R_BindTextureForTexUnit(r_state.buffers1[0]->textures[0], &texunit_1);

	R_UseViewport(fbo_screen);
	R_DrawQuad();

	/* draw final result to the screenbuffer */
	R_UseFramebuffer(fbo_screen);
	R_UseProgram(r_state.combine2_program);
	R_BindTextureForTexUnit(fbo_render->textures[0], &texunit_0);
	R_BindTextureForTexUnit(fbo_bloom0->textures[0], &texunit_1);

	R_DrawQuad();

	/* cleanup before returning */
	R_UseProgram(default_program);

	R_CheckError();

#ifndef GL_VERSION_ES_CM_1_0
	glPopAttrib();
#endif
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	R_CheckError();

	/* reset renderbuffer state to what it was before */
	R_EnableRenderbuffer(renderBufferState);
}
