/**
 * @file l3dslib.c
 * @brief library for loading triangles from an Alias triangle file
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

#include "../../ufo2map/common/cmdlib.h"
#include "../../ufo2map/common/mathlib.h"
#include "trilib.h"
#include "l3dslib.h"

#define MAIN3DS       0x4D4D
#define EDIT3DS       0x3D3D  /* this is the start of the editor config */
#define EDIT_OBJECT   0x4000
#define OBJ_TRIMESH   0x4100
#define TRI_VERTEXL   0x4110
#define TRI_FACEL1    0x4120

#define MAXVERTS	2000

typedef struct {
	int	v[4];
} tri;

static float fverts[MAXVERTS][3];
static tri tris[MAXTRIANGLES];

static int bytesread, level, numtris, totaltris;
static int vertsfound, trisfound;

static triangle_t *ptri;


/**
 * @brief Alias stores triangles as 3 explicit vertices in .tri files, so even though we
 * start out with a vertex pool and vertex indices for triangles, we have to convert
 * to raw, explicit triangles
 */
static void StoreAliasTriangles (void)
{
	int i, j, k;

	if ((totaltris + numtris) > MAXTRIANGLES)
		Error ("Error: Too many triangles");

	for (i=0; i<numtris ; i++) {
		for (j=0 ; j<3 ; j++) {
			for (k=0 ; k<3 ; k++) {
				ptri[i+totaltris].verts[j][k] = fverts[tris[i].v[j]][k];
			}
		}
	}

	totaltris += numtris;
	numtris = 0;
	vertsfound = 0;
	trisfound = 0;
}


/**
 * @brief
 */
static int ParseVertexL (FILE *input)
{
	int i, j, startbytesread, numverts;
	unsigned short tshort;

	if (vertsfound)
		Error ("Error: Multiple vertex chunks");

	vertsfound = 1;
	startbytesread = bytesread;

	if (feof(input))
		Error ("Error: unexpected end of file");

	if (fread(&tshort, sizeof(tshort), 1, input) != 1)
		printf("ParseVertexL: read error\n");
	bytesread += sizeof(tshort);
	numverts = (int)tshort;

	if (numverts > MAXVERTS)
		Error ("Error: Too many vertices");

	for (i=0 ; i<numverts ; i++) {
		for (j=0 ; j<3 ; j++) {
			if (feof(input))
				Error ("Error: unexpected end of file");

			if (fread(&fverts[i][j], sizeof(float), 1, input) != 1)
				printf("ParseVertexL: read error\n");
			bytesread += sizeof(float);
		}
	}

	if (vertsfound && trisfound)
		StoreAliasTriangles ();

	return bytesread - startbytesread;
}


/**
 * @brief
 */
static int ParseFaceL1 (FILE *input)
{

	int i, j, startbytesread;
	unsigned short	tshort;

	if (trisfound)
		Error ("Error: Multiple face chunks");

	trisfound = 1;
	startbytesread = bytesread;

	if (feof(input))
		Error ("Error: unexpected end of file");

	if (fread(&tshort, sizeof(tshort), 1, input) != 1)
		printf("ParseFaceL1: read error\n");
	bytesread += sizeof(tshort);
	numtris = (int)tshort;

	if (numtris > MAXTRIANGLES)
		Error ("Error: Too many triangles");

	for (i=0 ; i<numtris ; i++) {
		for (j=0 ; j<4 ; j++) {
			if (feof(input))
				Error ("Error: unexpected end of file");

			if (fread(&tshort, sizeof(tshort), 1, input) != 1)
				printf("ParseFaceL1: read error\n");
			bytesread += sizeof(tshort);
			tris[i].v[j] = (int)tshort;
		}
	}

	if (vertsfound && trisfound)
		StoreAliasTriangles ();

	return bytesread - startbytesread;
}


#define BLOCK_SIZE	4096
/**
 * @brief
 */
static int ParseChunk (FILE *input)
{
	char temp[BLOCK_SIZE];
	unsigned short type;
	int i, length, w, t, retval;

	level++;
	retval = 0;

	/* chunk type */
	if (feof(input))
		Error ("Error: unexpected end of file");

	if (fread(&type, sizeof(type), 1, input) != 1)
		printf("ParseChunk: read error\n");
	bytesread += sizeof(type);

	/* chunk length */
	if (feof(input))
		Error ("Error: unexpected end of file");

	if (fread (&length, sizeof(length), 1, input) != 1)
		printf("ParseChunk: read error\n");
	bytesread += sizeof(length);
	w = length - 6;

	/* process chunk if we care about it, otherwise skip it */
	switch (type) {
	case TRI_VERTEXL:
		w -= ParseVertexL (input);
		goto ParseSubchunk;

	case TRI_FACEL1:
		w -= ParseFaceL1 (input);
		goto ParseSubchunk;

	case EDIT_OBJECT:
		/* read the name */
		i = 0;

		do {
			if (feof(input))
				Error ("Error: unexpected end of file");

			if (fread (&temp[i], 1, 1, input) != 1)
				printf("ParseChunk: read error\n");
			i++;
			w--;
			bytesread++;
		} while (temp[i-1]);

	case MAIN3DS:
	case OBJ_TRIMESH:
	case EDIT3DS:
	/* parse through subchunks */
ParseSubchunk:
		while (w > 0) {
			w -= ParseChunk (input);
		}

		retval = length;
		goto Done;

	default:
	/* skip other chunks */
		while (w > 0) {
			t = w;

			if (t > BLOCK_SIZE)
				t = BLOCK_SIZE;

			if (feof(input))
				Error ("Error: unexpected end of file");

			if (fread (&temp, t, 1, input) != 1)
				printf("ParseChunk: read error\n");
			bytesread += t;
			w -= t;
		}

		retval = length;
		goto Done;
	}

Done:
	level--;
	return retval;
}

/**
 * @brief
 */
extern void Load3DSTriangleList (char *filename, triangle_t **pptri, int *numtriangles)
{
	FILE *input;
	short int tshort;

	bytesread = 0;
	level = 0;
	numtris = 0;
	totaltris = 0;
	vertsfound = 0;
	trisfound = 0;

	if ((input = fopen(filename, "rb")) == 0) {
		fprintf(stderr,"reader: could not open file '%s'\n", filename);
		exit(0);
	}

	if (fread(&tshort, sizeof(tshort), 1, input) != 1)
		printf("Load3DSTriangleList: read error\n");

	/* should only be MAIN3DS, but some files seem to start with EDIT3DS, with */
	/* no MAIN3DS */
	if ((tshort != MAIN3DS) && (tshort != EDIT3DS)) {
		fprintf(stderr,"File is not a 3DS file.\n");
		exit(0);
	}

	/* back to top of file so we can parse the first chunk descriptor */
	fseek(input, 0, SEEK_SET);

	ptri = malloc (MAXTRIANGLES * sizeof(triangle_t));
	*pptri = ptri;

	/* parse through looking for the relevant chunk tree (MAIN3DS | EDIT3DS | EDIT_OBJECT | */
	/* OBJ_TRIMESH | {TRI_VERTEXL, TRI_FACEL1}) and skipping other chunks */
	ParseChunk (input);

	if (vertsfound || trisfound)
		Error ("Incomplete triangle set");

	*numtriangles = totaltris;

	fclose (input);
}

