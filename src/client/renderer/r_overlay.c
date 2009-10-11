/**
 * @file r_overlay.c
 * @brief Functions to generate and render overlay for geoscape
 * @todo update the alpha values for radar like the ones for the xvi map - it's much faster like this
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
#include "r_image.h"
#include "r_overlay.h"

image_t *r_radarTexture;				/**< radar texture */
image_t *r_xviTexture;					/**< XVI alpha mask texture */

/** Max alpha level - don't set this to 255 or nothing else will be visible below the mask */
const int MAX_ALPHA_VALUE = 200;
const int INITIAL_ALPHA_VALUE = 60;

#define XVI_WIDTH		512
#define XVI_HEIGHT		256
#define RADAR_WIDTH		512
#define RADAR_HEIGHT	256

/** this is the data that is used with r_xviTexture */
static byte r_xviAlpha[XVI_WIDTH * XVI_HEIGHT];

/** this is the data that is used with r_radarTexture */
static byte r_radarPic[RADAR_WIDTH * RADAR_HEIGHT];

/** this is the data that is used with r_radarTexture */
static byte r_radarSourcePic[RADAR_WIDTH * RADAR_HEIGHT];

byte* R_GetXVIMap (int *width, int *height)
{
	*width = XVI_WIDTH;
	*height = XVI_HEIGHT;
	return r_xviAlpha;
}

static void R_UploadAlpha (const image_t *image, const byte *alphaData)
{
	R_BindTexture(image->texnum);

	/* upload alpha map into the r_dayandnighttexture */
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, image->width, image->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, alphaData);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static inline void R_SetXVILevel (int x, int y, int value)
{
	assert(x >= 0);
	assert(x < XVI_WIDTH);
	assert(y >= 0);
	assert(y < XVI_HEIGHT);

	if (!value)
		r_xviAlpha[y * XVI_WIDTH + x] = 0;
	else
		r_xviAlpha[y * XVI_WIDTH + x] = min(MAX_ALPHA_VALUE, value + INITIAL_ALPHA_VALUE);
}

static inline int R_GetXVILevel (int x, int y)
{
	assert(x >= 0);
	assert(x < XVI_WIDTH);
	assert(y >= 0);
	assert(y < XVI_HEIGHT);

	return max(0, r_xviAlpha[y * XVI_WIDTH + x] - INITIAL_ALPHA_VALUE);
}

/**
 * @brief Applies spreading on xvi transparency channel centered on a given pos
 * @param[in] xCenter X Position of the center of XVI spreading
 * @param[in] yCenter Y Position of the center of XVI spreading
 * @sa R_DecreaseXVILevel
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 * XVI spreads in circle, and the alpha value of one pixel indicates the XVI level of infection.
 * This is necessary to take into account a new event that would spread in the zone where XVI is already spread.
 * @return the radius of the new XVI level
 */
static float R_IncreaseXVILevel (const vec2_t pos, int xCenter, int yCenter, float factor)
{
	int xviLevel;								/**< XVI level rate at position pos */
	vec2_t currentPos;							/**< current position (in latitude / longitude) */
	int x, y;									/**< current position (in pixel) */
	float radius;								/**< radius of the new XVI circle */

	/* Get xvi Level infection at pos */
	xviLevel = R_GetXVILevel(xCenter, yCenter);
	/* Calculate radius of new spreading */
	if (xviLevel < MAX_ALPHA_VALUE - INITIAL_ALPHA_VALUE)
		xviLevel++;
	radius = sqrt(factor * xviLevel);

	for (y = 0; y < XVI_HEIGHT; y++) {
		for (x = 0; x < XVI_WIDTH; x++) {
			float distance;
			Vector2Set(currentPos,
				180.0f - 360.0f * x / ((float) XVI_WIDTH),
				90.0f - 180.0f * y / ((float) XVI_HEIGHT));

			distance = GetDistanceOnGlobe(pos, currentPos);
			if (distance <= 1. * radius) {
				int newValue;
				if (xCenter == x && yCenter == y)
					/* Make sure that XVI level increases, even if
					 * rounding problems makes radius constant */
					newValue = xviLevel;
				else
					newValue = round((xviLevel * (radius - distance)) / radius);

				R_SetXVILevel(x, y, max(xviLevel, newValue));
			}
		}
	}

	R_UploadAlpha(r_xviTexture, r_xviAlpha);

	return radius;
}

/**
 * @sa R_IncreaseXVILevel
 * @param[in] xCenter X Position of the center of XVI spreading
 * @param[in] yCenter Y Position of the center of XVI spreading
 */
static float R_DecreaseXVILevel (const vec2_t pos, int xCenter, int yCenter, float factor)
{
	return 0.0f;
}

/**
 * @sa R_IncreaseXVILevel
 */
void R_DecreaseXVILevelEverywhere (void)
{
	int x, y;									/**< current position (in pixel) */

	for (y = 0; y < XVI_HEIGHT; y++) {
		for (x = 0; x < XVI_WIDTH; x++) {
			int xviLevel = R_GetXVILevel(x, y);
			if (xviLevel > 0)
				R_SetXVILevel(x, y, xviLevel - 1);
		}
	}

	R_UploadAlpha(r_xviTexture, r_xviAlpha);
}

float R_ChangeXVILevel (const vec2_t pos, const int xviLevel, float factor)
{
	const int xCenter = round((180 - pos[0]) * XVI_WIDTH / 360.0f);
	const int yCenter = round((90 - pos[1]) * XVI_HEIGHT / 180.0f);

	const int currentXVILevel = R_GetXVILevel(xCenter, yCenter);

	if (currentXVILevel < xviLevel)
		return R_IncreaseXVILevel(pos, xCenter, yCenter, factor);
	else if (currentXVILevel > xviLevel)
		return R_DecreaseXVILevel(pos, xCenter, yCenter, factor);

	return 0.0f;
}

/**
 * @brief Initialize XVI overlay on geoscape.
 * @param[in] data Pointer to the data containing values to store in XVI map. Can be NULL for new games.
 * This is only the alpha channel of the xvi map
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 */
void R_InitializeXVIOverlay (const byte *data)
{
	assert(r_xviTexture);

	if (!data)
		memset(r_xviAlpha, 0, sizeof(r_xviAlpha));
	else
		memcpy(r_xviAlpha, data, sizeof(r_xviAlpha));

	R_UploadAlpha(r_xviTexture, r_xviAlpha);
}

/**
 * @brief Radar overlay code description
 * The radar overlay is handled in 2 times : bases radar range and aircraft radar range.
 * Bases radar range needs to be updated only when a radar facilities is built or destroyed.
 * The base radar overlay is stored in r_radarSourcePic.
 * Aircraft radar overlay needs to be updated every time an aircraft moves, because the position of the radar moves.
 * this overlay is created by duplicating r_radarSourcePic, and then adding any aircraft radar coverage. It is stored in r_radarTexture
 * @sa RADAR_UpdateWholeRadarOverlay
 */

/**
 * @brief Initialize radar overlay on geoscape.
 * @param[in] source Initialize the source texture if qtrue: if you are updating base radar overlay.
 * false if you are updating aircraft radar overlay (base radar overlay will be copied to aircraft radar overlay)
 */
void R_InitializeRadarOverlay (qboolean source)
{
	/* Initialize Radar */
	if (source)
		memset(r_radarSourcePic, INITIAL_ALPHA_VALUE, sizeof(r_radarSourcePic));
	else
		memcpy(r_radarPic, r_radarSourcePic, sizeof(r_radarPic));
}

/**
 * @brief Draw radar overlay for a given latitude between 2 longitudes.
 * @param[in] latMin Minimum latitude.
 * @param[in] latMax Maximum latitude.
 * @param[in] y current row in radar overlay.
 * @param[in] source True if we must update the source of the radar coverage, false if the copy must be updated.
 * @pre We assume latMax - latMin <= 360 degrees.
 */
static void R_DrawRadarOverlayRow (int latMin, int latMax, int y, byte alpha, qboolean source)
{
	const float radarWidthPerDegree = RADAR_WIDTH / 360.0f;
	int xMin, xMax, x;

	assert(latMax - latMin <= 360);

	if (latMin < -180.0f) {
		xMin = 0;
		xMax = ceil((latMax + 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x++) {
			byte *dest = source ? &r_radarSourcePic[y * RADAR_WIDTH + x] : &r_radarPic[y * RADAR_WIDTH + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
		xMin = floor((latMin + 540.0f) * radarWidthPerDegree);
		xMax = RADAR_WIDTH;
		for (x = xMin; x < xMax; x++) {
			byte *dest = source ? &r_radarSourcePic[y * RADAR_WIDTH + x] : &r_radarPic[y * RADAR_WIDTH + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
	} else if (latMax > 180.0f) {
		xMin = 0;
		xMax = ceil((latMax - 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x++) {
			byte *dest = source ? &r_radarSourcePic[y * RADAR_WIDTH + x] : &r_radarPic[y * RADAR_WIDTH + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
		xMin = floor((latMin + 180.0f) * radarWidthPerDegree);
		xMax = RADAR_WIDTH;
		for (x = xMin; x < xMax; x++) {
			byte *dest = source ? &r_radarSourcePic[y * RADAR_WIDTH + x] : &r_radarPic[y * RADAR_WIDTH + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
	} else {
		xMin = floor((latMin + 180.0f) * radarWidthPerDegree);
		xMax = ceil((latMax + 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x++) {
			byte *dest = source ? &r_radarSourcePic[y * RADAR_WIDTH + x] : &r_radarPic[y * RADAR_WIDTH + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
	}
}

/**
 * @brief Return the half longitude affected by radar at a given latitude.
 * @param[in] radarPos center of the radar.
 * @param[in] radius radius of the radar.
 * @param[in] y latitude of current point (in radians).
 * @note This is an implementation of the following facts:
 * - the distance (on a sphere) between radarPos and current position is equal to radius
 * - the border of the radar coverage on a row has the same latitude than current latitude.
 */
static inline float R_GetRadarDeltaLongitude (const vec2_t radarPos, float radius, const float yLat)
{
	const float angle = (cos(radius * torad) - sin(radarPos[1] * torad) * sin(yLat)) / (cos(radarPos[1] * torad) * cos(yLat));
	return fabs(angle) > 1.0f ? 180.0f : todeg * acos(angle);
}

/**
 * @brief Set lower and upper value of the radar's overlay row that can be modified by current radar.
 * @param[in] pos Position of the center of radar.
 * @param[in] radius Radius of the radar coverage.
 * @param[out] yMin Pointer to the lower row of radar overlay that should be changed.
 * @param[out] yMax Pointer to the higher row of radar overlay that should be changed.
 */
static void R_SetMinMaxRadarRows (const vec2_t pos, float radius, int *yMin, int *yMax)
{
	const float radarHeightPerDegree = RADAR_HEIGHT / 180.0f;

	if (pos[1] + radius > 90) {
		*yMin = 0;
		*yMax = round((90 - pos[1] + radius) * radarHeightPerDegree);
	} else if (pos[1] - radius < -90) {
		*yMin = ceil((90 - pos[1] - radius) * radarHeightPerDegree);
		*yMax = RADAR_HEIGHT;
	} else {
		*yMin = ceil((90 - pos[1] - radius) * radarHeightPerDegree);
		*yMax = round((90 - pos[1] + radius) * radarHeightPerDegree);
	}

	/* a few assert to avoid buffer overflow */
	assert(*yMin >= 0);
	assert(*yMin <= *yMax);
	assert(*yMax <= RADAR_HEIGHT);			/* the loop will stop just BEFORE yMax, so use <= rather than < */
}

/**
 * @brief Add a radar coverage (base or aircraft) to radar overlay
 * @param[in] pos Position of the center of radar
 * @param[in] innerRadius Radius of the radar coverage
 * @param[in] outerRadius Radius of the outer radar coverage
 * @param[in] source True if we must update the source of the radar coverage, false if the copy must be updated.
 * @pre We assume outerRadius is smaller than 180 degrees
 */
void R_AddRadarCoverage (const vec2_t pos, float innerRadius, float outerRadius, qboolean source)
{
	const byte innerAlpha = 0;					/**< Alpha of the inner radar range */
	const byte outerAlpha = 60;					/**< Alpha of the outer radar range */
	const float radarHeightPerDegree = RADAR_HEIGHT / 180.0f;
	int y;										/**< current position (in pixel) */
	int yMax, yMin;								/**< Bounding box of the inner radar zone */
	int outeryMax, outeryMin;					/**< Bounding box of the outer radar zone */

	assert(outerRadius < 180.0f);
	assert(outerRadius > innerRadius);

	/* Set minimum and maximum rows value we'll have to change */
	R_SetMinMaxRadarRows(pos, innerRadius, &yMin, &yMax);
	R_SetMinMaxRadarRows(pos, outerRadius, &outeryMin, &outeryMax);

	/* Draw upper part of the radar coverage */
	for (y = outeryMin; y < yMin; y++) {
		/* latitude of current point, in radian */
		const float yLat = torad * (90.0f - y / radarHeightPerDegree);
		float outerDeltaLong = R_GetRadarDeltaLongitude(pos, outerRadius, yLat);

		/* Only the outer radar coverage is drawn at this latitude */
		R_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}

	/* Draw middle part of the radar coverage */
	for (y = yMin; y < yMax; y++) {
		/* latitude of current point, in radian */
		const float yLat = torad * (90.0f - y / radarHeightPerDegree);
		const float deltaLong = R_GetRadarDeltaLongitude(pos, innerRadius, yLat);
		const float outerDeltaLong = R_GetRadarDeltaLongitude(pos, outerRadius, yLat);

		/* At this latitude, there are 3 parts to draw: left outer radar, inner radar, and right outer radar */
		R_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] - deltaLong, y, outerAlpha, source);
		R_DrawRadarOverlayRow(-pos[0] - deltaLong, -pos[0] + deltaLong, y, innerAlpha, source);
		R_DrawRadarOverlayRow(-pos[0] + deltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}

	/* Draw lower part of the radar coverage */
	for (y = yMax; y < outeryMax; y++) {
		/* latitude of current point, in radian */
		const float yLat = torad * (90.0f - y / radarHeightPerDegree);
		const float outerDeltaLong = R_GetRadarDeltaLongitude(pos, outerRadius, yLat);

		/* Only the outer radar coverage is drawn at this latitude */
		R_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}
}

/**
 * @brief Smooth radar coverage
 * @note allows to make texture pixels less visible.
 */
void R_UploadRadarCoverage (void)
{
	R_SoftenTexture(r_radarPic, RADAR_WIDTH, RADAR_HEIGHT, 1);

	R_UploadAlpha(r_radarTexture, r_radarPic);
}

void R_ShutdownOverlay (void)
{
	r_radarTexture = NULL;
	r_xviTexture = NULL;
}

void R_InitOverlay (void)
{
	r_radarTexture = R_LoadImageData("***r_radarTexture***", NULL, RADAR_WIDTH, RADAR_HEIGHT, it_wrappic);
	r_xviTexture = R_LoadImageData("***r_xvitexture***", NULL, XVI_WIDTH, XVI_HEIGHT, it_effect);
}
