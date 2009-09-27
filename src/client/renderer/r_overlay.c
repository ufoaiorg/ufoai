/**
 * @file r_overlay.c
 * @brief Functions to generate and render overlay for geoscape
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

static const int maxAlpha = 256;		/**< Number of alpha level */
image_t *r_xviTexture;					/**< XVI texture */
static byte *r_xviPic;					/**< XVI picture */

float MAP_GetDistance(const vec2_t pos1, const vec2_t pos2);

/**
 * @brief Applies spreading on xvi transparency channel centered on a given pos
 * @param[in] pos Position of the center of XVI spreading
 * @sa R_DecreaseXVILevel
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 * XVI spreads in circle, and the alpha value of one pixel indicates the XVI level of infection.
 * This is necessary to take into account a new event that would spread in the zone where XVI is already spread.
 */
void R_IncreaseXVILevel (const vec2_t pos)
{
	const int bpp = 4;							/**< byte per pixel */
	const int minAlpha = 100;					/**< minimum value of alpha when spreading XVI
													255 - minAlpha is the maximum XVI level */
	int xviLevel;								/**< XVI level rate at position pos */
	vec2_t currentPos;							/**< current position (in latitude / longitude) */
	int x, y;									/**< current position (in pixel) */
	const int xviWidth = r_xviTexture->width;
	const int xviHeight = r_xviTexture->height;
	const int xCenter = bpp * round((180 - pos[0]) * r_xviTexture->width / 360.0f);
	const int yCenter = bpp * round((90 - pos[1]) * r_xviTexture->height / 180.0f);
	float radius;								/**< radius of the new XVI circle */

	assert(xCenter >= 0);
	assert(xCenter < bpp * xviWidth);
	assert(yCenter >= 0);
	assert(yCenter < bpp * xviHeight);

	if (r_xviPic[3 + yCenter * xviWidth + xCenter] < minAlpha) {
		/* This zone wasn't infected */
		r_xviPic[3 + yCenter * xviWidth + xCenter] = minAlpha;
		return;
	}

	/* Get xvi Level infection at pos */
	xviLevel = r_xviPic[3 + yCenter * xviWidth + xCenter] - minAlpha;
	/* Calculate radius of new spreading */
	xviLevel++;
	radius = sqrt(15.0f * xviLevel);

	for (y = 0; y < bpp * xviHeight; y += bpp) {
		for (x = 0; x < bpp * xviWidth; x += bpp) {
			float distance;
			Vector2Set(currentPos,
				180.0f - 360.0f * x / ((float) xviWidth * bpp),
				90.0f - 180.0f * y / ((float) xviHeight * bpp));
			distance = MAP_GetDistance(pos, currentPos);
			if (distance <= 1.1 * radius) {
				int newValue;
				if ((xCenter == x) && (yCenter == y))
					/* Make sure that XVI level increases, even if rounding problems makes radius constant */
					newValue = minAlpha + xviLevel;
				else if (distance > radius)
					newValue = round(minAlpha * (1.1 * radius - distance) / (0.1 * radius));
				else
					newValue = round(minAlpha + (xviLevel * (radius - distance)) / radius);
				if (newValue > 255) {
					Com_DPrintf(DEBUG_CLIENT, "Maximum alpha value reached\n");
					newValue = 255;
				}
				if (r_xviPic[3 + y * xviWidth + x] < newValue) {
					r_xviPic[3 + y * xviWidth + x] = newValue;
				}
			}
		}
	}

	R_BindTexture(r_xviTexture->texnum);
	R_UploadTexture((unsigned *) r_xviPic, r_xviTexture->upload_width, r_xviTexture->upload_height, r_xviTexture);
}

/**
 * @sa R_IncreaseXVILevel
 * @param pos
 */
void R_DecreaseXVILevel (const vec2_t pos)
{
}

/**
 * @brief Initialize XVI overlay on geoscape.
 * @param[in] data Pointer to the data containing values to store in XVI map. Can be NULL for new games.
 * This is only the alpha channel of the xvi map
 * @param[in] width Width of data (used only if data is not NULL)
 * @param[in] height Height of data (used only if data is not NULL)
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 */
void R_InitializeXVIOverlay (const char *mapname, byte *data, int width, int height)
{
	int xviWidth, xviHeight;
	int x, y;
	byte *start;
	qboolean setToZero = qtrue;

	/* reload for every new game */
	if (r_xviTexture)
		R_FreeImage(r_xviTexture);
	if (r_xviPic) {
		Mem_Free(r_xviPic);
		r_xviPic = NULL;
	}

	/* we should always have a campaign loaded here */
	assert(mapname);

	/* Load the XVI texture */
	R_LoadImage(va("pics/geoscape/%s_xvi_overlay", mapname), &r_xviPic, &xviWidth, &xviHeight);
	if (!r_xviPic || !xviWidth || !xviHeight)
		Com_Error(ERR_FATAL, "Couldn't load map mask %s_xvi_overlay in pics/geoscape", mapname);

	if (data && (width == xviWidth) && (height == xviHeight))
		setToZero = qfalse;

	/* Initialize XVI rate */
	start = r_xviPic + 3;	/* to get the first alpha value */
	for (y = 0; y < xviHeight; y++)
		for (x = 0; x < xviWidth; x++, start += 4) {
			start[0] = setToZero ? 0 : data[y * xviWidth + x];
		}

	/* Set an image */
	r_xviTexture = R_LoadImageData(va("pics/geoscape/%s_xvi_overlay", mapname), r_xviPic, xviWidth, xviHeight, it_wrappic);
}

/**
 * @brief Copy XVI transparency map for saving purpose (only the alpha value of the map)
 * @sa CP_XVIMapSave
 * @note Make sure to free the returned buffer
 */
byte* R_XVIMapCopy (int *width, int *height)
{
	int x, y;
	const int bpp = 4;
	byte *buf = Mem_PoolAllocExt(r_xviTexture->height * r_xviTexture->width * bpp, qfalse, vid_imagePool, 0);
	if (!buf)
		return NULL;

	*width = r_xviTexture->width;
	*height = r_xviTexture->height;
	for (y = 0; y < r_xviTexture->height; y++) {
		for (x = 0; x < r_xviTexture->width; x++)
			buf[y * r_xviTexture->width + x] = r_xviPic[3 + (y * r_xviTexture->width + x) * bpp];
	}
	return buf;
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

image_t *r_radarTexture;					/**< radar texture */
static byte *r_radarPic;					/**< radar picture (base and aircraft radar) */
static byte *r_radarSourcePic;				/**< radar picture (only base radar) */

/**
 * @brief Create radar overlay on geoscape.
 * @note This function allocate memory for radar images and texture.
 * It should be called only once per game.
 */
void R_CreateRadarOverlay (void)
{
	const int radarWidth = 512;
	const int radarHeight = 256;
	const int bpp = 4;
	const int size = bpp * radarHeight * radarWidth;

	/* create new texture only once per life time, but reset it for every
	 * new game (campaign start or game load) */
	if (r_radarTexture && r_radarTexture->texnum > 0) {
		memset(r_radarSourcePic, 0, size);
		memset(r_radarPic, 0, size);
		R_UploadRadarCoverage(qfalse);
		return;
	}

	r_radarPic = Mem_PoolAlloc(size, vid_imagePool, 0);
	r_radarSourcePic = Mem_PoolAlloc(size, vid_imagePool, 0);

	/* Set an image */
	r_radarTexture = R_LoadImageData("pics/geoscape/map_earth_radar_overlay", r_radarPic, radarWidth, radarHeight, it_wrappic);
}

/**
 * @brief Initialize radar overlay on geoscape.
 * @param[in] source Initialize the source texture if qtrue: if you are updating base radar overlay.
 * false if you are updating aircraft radar overlay (base radar overlay will be copied to aircraft radar overlay)
 */
void R_InitializeRadarOverlay (qboolean source)
{
	/* Initialize Radar */
	if (source) {
		int x, y;
		const byte unexploredColor[4] = {180, 180, 180, 100};	/**< Color of the overlay outside radar range */

		for (y = 0; y < r_radarTexture->height; y++) {
			for (x = 0; x < r_radarTexture->width; x++) {
				memcpy(&r_radarSourcePic[4 * (y * r_radarTexture->width + x)], unexploredColor, 4);
			}
		}
	} else
		memcpy(r_radarPic, r_radarSourcePic, 4 * r_radarTexture->height * r_radarTexture->width);
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
	const int bpp = 4;							/**< byte per pixel */
	const int radarWidth = r_radarTexture->width;
	const float radarWidthPerDegree = radarWidth / 360.0f;
	int xMin, xMax, x;

	assert(latMax - latMin <= 360);

	if (latMin < -180.0f) {
		xMin = 0;
		xMax = bpp * ceil((latMax + 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x += bpp) {
			byte *dest = source ? &r_radarSourcePic[y * radarWidth + x] : &r_radarPic[y * radarWidth + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
		xMin = bpp * floor((latMin + 540.0f) * radarWidthPerDegree);
		xMax = bpp * radarWidth;
		for (x = xMin; x < xMax; x += bpp) {
			byte *dest = source ? &r_radarSourcePic[y * radarWidth + x] : &r_radarPic[y * radarWidth + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
	} else if (latMax > 180.0f) {
		xMin = 0;
		xMax = bpp * ceil((latMax - 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x += bpp) {
			byte *dest = source ? &r_radarSourcePic[y * radarWidth + x] : &r_radarPic[y * radarWidth + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
		xMin = bpp * floor((latMin + 180.0f) * radarWidthPerDegree);
		xMax = bpp * radarWidth;
		for (x = xMin; x < xMax; x += bpp) {
			byte *dest = source ? &r_radarSourcePic[y * radarWidth + x] : &r_radarPic[y * radarWidth + x];
			if (alpha < dest[3])
				dest[3] = alpha;
		}
	} else {
		xMin = bpp * floor((latMin + 180.0f) * radarWidthPerDegree);
		xMax = bpp * ceil((latMax + 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x += bpp) {
			byte *dest = source ? &r_radarSourcePic[y * radarWidth + x] : &r_radarPic[y * radarWidth + x];
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
static inline float R_GetRadarDeltaLongitude (const vec2_t radarPos, float radius, const float  yLat)
{
	const float angle = (cos(radius * torad) - sin(radarPos[1] * torad) * sin(yLat)) / (cos(radarPos[1] * torad) * cos(yLat));

	return fabs(angle) > 1.0f ? 180.0f : todeg * acos(angle);
}

/**
 * @brief Set lower and upper value of the radar's overlay row that can be modified by current radar.
 * @param[in] pos Position of the center of radar.
 * @param[in] radius Radius of the radar coverage.
 * @param[in] radarHeight Number of pixels of the texture in y direction.
 * @param[out] yMin Pointer to the lower row of radar overlay that should be changed.
 * @param[out] yMax Pointer to the higher row of radar overlay that should be changed.
 */
static void R_SetMinMaxRadarRows (const vec2_t pos, float radius, const float radarHeight, int *yMin, int *yMax)
{
	const int bpp = 4;							/**< byte per pixel */
	const float radarHeightPerDegree = radarHeight / 180.0f;

	if (pos[1] + radius > 90) {
		*yMin = 0;
		*yMax = bpp * round((90 - pos[1] + radius) * radarHeightPerDegree);
	} else if (pos[1] - radius < -90) {
		*yMin = bpp * ceil((90 - pos[1] - radius) * radarHeightPerDegree);
		*yMax = bpp * radarHeight;
	} else {
		*yMin = bpp * ceil((90 - pos[1] - radius) * radarHeightPerDegree);
		*yMax = bpp * round((90 - pos[1] + radius) * radarHeightPerDegree);
	}

	/* a few assert to avoid buffer overflow */
	assert(*yMin >= 0);
	assert(*yMin <= *yMax);
	assert(*yMax <= bpp * radarHeight);			/* the loop will stop just BEFORE yMax, so use <= rather than < */
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
	const int bpp = 4;							/**< byte per pixel */
	const byte innerAlpha = 0;					/**< Alpha of the inner radar range */
	const byte outerAlpha = 60;					/**< Alpha of the outer radar range */
	const int radarHeight = r_radarTexture->height;
	const float radarHeightPerDegree = radarHeight / 180.0f;
	int y;										/**< current position (in pixel) */
	int yMax, yMin;								/**< Bounding box of the inner radar zone */
	int outeryMax, outeryMin;					/**< Bounding box of the outer radar zone */

	assert(outerRadius < 180.0f);
	assert(outerRadius > innerRadius);

	/* Set minimum and maximum rows value we'll have to change */
	R_SetMinMaxRadarRows(pos, innerRadius, radarHeight, &yMin, &yMax);
	R_SetMinMaxRadarRows(pos, outerRadius, radarHeight, &outeryMin, &outeryMax);

	/* Draw upper part of the radar coverage */
	for (y = outeryMin; y < yMin; y += bpp) {
		const float yLat = torad * (90.0f - y / (radarHeightPerDegree * bpp));	/**< latitude of current point, in radian */
		float outerDeltaLong = R_GetRadarDeltaLongitude(pos, outerRadius, yLat);

		/* Only the outer radar coverage is drawn at this latitude */
		R_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}

	/* Draw middle part of the radar coverage */
	for (y = yMin; y < yMax; y += bpp) {
		const float yLat = torad * (90.0f - y / (radarHeightPerDegree * bpp));	/**< latitude of current point, in radian */
		const float deltaLong = R_GetRadarDeltaLongitude(pos, innerRadius, yLat);
		const float outerDeltaLong = R_GetRadarDeltaLongitude(pos, outerRadius, yLat);

		/* At this latitude, there are 3 parts to draw: left outer radar, inner radar, and right outer radar */
		R_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] - deltaLong, y, outerAlpha, source);
		R_DrawRadarOverlayRow(-pos[0] - deltaLong, -pos[0] + deltaLong, y, innerAlpha, source);
		R_DrawRadarOverlayRow(-pos[0] + deltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}

	/* Draw lower part of the radar coverage */
	for (y = yMax; y < outeryMax; y += bpp) {
		const float yLat = torad * (90.0f - y / (radarHeightPerDegree * bpp));	/**< latitude of current point, in radian */
		const float outerDeltaLong = R_GetRadarDeltaLongitude(pos, outerRadius, yLat);

		/* Only the outer radar coverage is drawn at this latitude */
		R_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}
}

/**
 * @brief Smooth radar coverage
 * @param[in] smooth Smoothes the picture if set to True (should be used only when all radars have been added)
 * @note allows to make texture pixels less visible.
 */
void R_UploadRadarCoverage (qboolean smooth)
{
	/** @todo This is no realtime function */
	if (smooth)
		R_SoftenTexture(r_radarPic, r_radarTexture->width, r_radarTexture->height, 4);

	R_BindTexture(r_radarTexture->texnum);
	R_UploadTexture((unsigned *) r_radarPic, r_radarTexture->upload_width, r_radarTexture->upload_height, r_radarTexture);
}

void R_ShutdownOverlay (void)
{
	r_radarTexture = NULL;
	Mem_Free(r_radarPic);
	Mem_Free(r_radarSourcePic);
	r_radarPic = NULL;
	r_radarSourcePic = NULL;

	r_xviTexture = NULL;
	Mem_Free(r_xviPic);
	r_xviPic = NULL;
}
