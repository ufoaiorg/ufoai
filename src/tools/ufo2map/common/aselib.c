/**
 * @file aselib.c
 * @brief ASE model loading
 */

/*
Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "aselib.h"
#include "../bsp.h"
#include "shared.h"

#define MAX_ASE_MATERIALS			32
#define MAX_ASE_OBJECTS				64
#define MAX_ASE_ANIMATIONS			32
#define MAX_ASE_ANIMATION_FRAMES	512

#define VERBOSE(x) { if (ase.verbose) { Com_Printf x; } }

typedef struct {
	float x, y, z;
	float nx, ny, nz;
	float s, t;
} aseVertex_t;

typedef struct {
	float s, t;
} aseTVertex_t;

typedef int aseFace_t[3];

typedef struct {
	int numFaces;
	int numVertexes;
	int numTVertexes;

	int timeValue;

	aseVertex_t *vertexes;
	aseTVertex_t *tvertexes;
	aseFace_t *faces, *tfaces;

	int currentFace, currentVertex;
} aseMesh_t;

typedef struct {
	int numFrames;
	aseMesh_t frames[MAX_ASE_ANIMATION_FRAMES];

	int currentFrame;
} aseMeshAnimation_t;

typedef struct {
	char name[MAX_QPATH];
} aseMaterial_t;

/**
 * @brief contains the animate sequence of a single surface using a single material
 */
typedef struct {
	char name[MAX_QPATH];

	int materialRef;
	int numAnimations;

	aseMeshAnimation_t anim;
} aseGeomObject_t;

typedef struct {
	int numMaterials;
	aseMaterial_t materials[MAX_ASE_MATERIALS];
	aseGeomObject_t objects[MAX_ASE_OBJECTS];

	char *buffer;
	char *curpos;
	int len;

	int currentObject;
	qboolean verbose;
} ase_t;

static char s_token[1024];
static ase_t ase;

static void ASE_Process(void);
static void ASE_FreeGeomObject(int ndx);

void ASE_Load (const char *filename, qboolean verbose)
{
	qFILE file;

	FS_OpenFile(filename, &file, FILE_READ);
	if (!file.f && !file.z)
		Sys_Error("File not found '%s'", filename);

	OBJZERO(ase);

	ase.verbose = verbose;
	ase.len = FS_FileLength(&file);

	ase.curpos = ase.buffer = (char *)Mem_Alloc(ase.len);
	if (!ase.curpos)
		Sys_Error("Could not allocate memory for ase loading");

	Verb_Printf(VERB_EXTRA, "Processing '%s'\n", filename);

	if (FS_Read(ase.buffer, ase.len, &file) != 1) {
		FS_CloseFile(&file);
		Sys_Error("fread() != -1 for '%s'", filename);
	}

	FS_CloseFile(&file);

	ASE_Process();
}

void ASE_Free (void)
{
	int i;

	for (i = 0; i < ase.currentObject; i++)
		ASE_FreeGeomObject(i);
}

int ASE_GetNumSurfaces (void)
{
	return ase.currentObject;
}

const char *ASE_GetSurfaceName (int which)
{
	aseGeomObject_t *pObject = &ase.objects[which];

	if (!pObject->anim.numFrames)
		return 0;

	return pObject->name;
}

/**
 * @brief Returns an animation (sequence of polysets)
 */
polyset_t *ASE_GetSurfaceAnimation (int whichSurface)
{
	aseGeomObject_t *pObject = &ase.objects[whichSurface];
	polyset_t *psets;
	int numFramesInAnimation;
	int i, f;

	if (!pObject->anim.numFrames)
		return 0;

	numFramesInAnimation = pObject->anim.numFrames;

	psets = (polyset_t *)Mem_Alloc(sizeof(*psets) * numFramesInAnimation);

	for (f = 0, i = 0; i < numFramesInAnimation; i++) {
		int t;
		aseMesh_t *pMesh = &pObject->anim.frames[i];
		polyset_t *ps = &psets[f];

		strcpy(ps->name, pObject->name);
		strcpy(ps->materialname, ase.materials[pObject->materialRef].name);

		ps->triangles = (triangle_t *)Mem_Alloc(sizeof(*ps->triangles) * pObject->anim.frames[i].numFaces);
		ps->numtriangles = pObject->anim.frames[i].numFaces;

		for (t = 0; t < pObject->anim.frames[i].numFaces; t++) {
			int k;

			for (k = 0; k < 3; k++) {
				triangle_t *tri = &ps->triangles[t];
				const int vIdx = pMesh->faces[t][k];
				aseVertex_t *v = &pMesh->vertexes[vIdx];
				VectorSet(tri->verts[k], v->x, v->y, v->z);

				if (pMesh->tvertexes && pMesh->tfaces) {
					aseTVertex_t *tv = &pMesh->tvertexes[pMesh->tfaces[t][k]];
					Vector2Set(tri->texcoords[k], tv->s, tv->t);
				}
			}
		}

		f++;
	}

	return psets;
}

static void ASE_FreeGeomObject (int ndx)
{
	aseGeomObject_t *pObject;
	int i;

	pObject = &ase.objects[ndx];

	for (i = 0; i < pObject->anim.numFrames; i++) {
		if (pObject->anim.frames[i].vertexes)
			Mem_Free(pObject->anim.frames[i].vertexes);
		if (pObject->anim.frames[i].tvertexes)
			Mem_Free(pObject->anim.frames[i].tvertexes);
		if (pObject->anim.frames[i].faces)
			Mem_Free(pObject->anim.frames[i].faces);
		if (pObject->anim.frames[i].tfaces)
			Mem_Free(pObject->anim.frames[i].tfaces);
	}

	OBJZERO(*pObject);
}

static aseMesh_t *ASE_GetCurrentMesh (void)
{
	aseGeomObject_t *pObject;

	if (ase.currentObject >= MAX_ASE_OBJECTS)
		Sys_Error("Too many GEOMOBJECTs");

	pObject = &ase.objects[ase.currentObject];

	if (pObject->anim.currentFrame >= MAX_ASE_ANIMATION_FRAMES)
		Sys_Error("Too many MESHes");

	return &pObject->anim.frames[pObject->anim.currentFrame];
}

static inline int CharIsTokenDelimiter (int ch)
{
	if (ch <= ' ')
		return 1;
	return 0;
}

static int ASE_GetToken (qboolean restOfLine)
{
	int i = 0;

	if (ase.buffer == 0)
		return 0;

	if ((ase.curpos - ase.buffer) == ase.len)
		return 0;

	/* skip over crap */
	while (ase.curpos - ase.buffer < ase.len && *ase.curpos <= ' ') {
		ase.curpos++;
	}

	while ((ase.curpos - ase.buffer) < ase.len) {
		s_token[i] = *ase.curpos;

		ase.curpos++;
		i++;

		if ((CharIsTokenDelimiter(s_token[i - 1]) && !restOfLine) ||
			(s_token[i - 1] == '\n' || s_token[i - 1] == '\r')) {
			s_token[i - 1] = 0;
			break;
		}
	}

	s_token[i] = 0;

	return 1;
}

static void ASE_ParseBracedBlock (void (*parser)(const char *token))
{
	int indent = 0;

	while (ASE_GetToken(qfalse)) {
		if (Q_streq(s_token, "{")) {
			indent++;
		} else if (Q_streq(s_token, "}")) {
			--indent;
			if (indent == 0)
				break;
			else if (indent < 0)
				Sys_Error("Unexpected '}'");
		} else {
			if (parser)
				parser(s_token);
		}
	}
}

static void ASE_SkipEnclosingBraces (void)
{
	int indent = 0;

	while (ASE_GetToken(qfalse)) {
		if (Q_streq(s_token, "{")) {
			indent++;
		} else if (Q_streq(s_token, "}")) {
			indent--;
			if (indent == 0)
				break;
			else if (indent < 0)
				Sys_Error("Unexpected '}'");
		}
	}
}

static void ASE_SkipRestOfLine (void)
{
	ASE_GetToken(qtrue);
}

static void ASE_KeyMAP_DIFFUSE (const char *token)
{
	if (Q_streq(token, "*BITMAP")) {
		const char *bitmap;
		size_t len;

		ASE_GetToken(qfalse);

		/* skip the " */
		bitmap = &s_token[1];
		len = strlen(bitmap) - 1;
		s_token[len] = '\0';

		Com_StripExtension(bitmap, ase.materials[ase.numMaterials].name, MAX_QPATH);
		Verb_Printf(VERB_EXTRA, "ase material name: \'%s\'\n", ase.materials[ase.numMaterials].name);
	}
}

static void ASE_KeyMATERIAL (const char *token)
{
	if (Q_streq(token, "*MAP_DIFFUSE"))
		ASE_ParseBracedBlock(ASE_KeyMAP_DIFFUSE);
}

static void ASE_KeyMATERIAL_LIST (const char *token)
{
	if (Q_streq(token, "*MATERIAL_COUNT")) {
		ASE_GetToken(qfalse);
		VERBOSE(("..num materials: %s\n", s_token));
		if (atoi(s_token) > MAX_ASE_MATERIALS) {
			Sys_Error("Too many materials!");
		}
		ase.numMaterials = 0;
	} else if (Q_streq(token, "*MATERIAL")) {
		VERBOSE(("..material %d ", ase.numMaterials));
		ASE_ParseBracedBlock(ASE_KeyMATERIAL);
		ase.numMaterials++;
	}
}

static void ASE_KeyMESH_VERTEX_LIST (const char *token)
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if (Q_streq(token, "*MESH_VERTEX")) {
		ASE_GetToken(qfalse);		/* skip number */

		ASE_GetToken(qfalse);
		pMesh->vertexes[pMesh->currentVertex].y = atof(s_token);

		ASE_GetToken(qfalse);
		pMesh->vertexes[pMesh->currentVertex].x = -atof(s_token);

		ASE_GetToken(qfalse);
		pMesh->vertexes[pMesh->currentVertex].z = atof(s_token);

		pMesh->currentVertex++;

		if (pMesh->currentVertex > pMesh->numVertexes)
			Sys_Error("pMesh->currentVertex >= pMesh->numVertexes");
	} else
		Sys_Error("Unknown token '%s' while parsing MESH_VERTEX_LIST", token);
}

static void ASE_KeyMESH_FACE_LIST (const char *token)
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if (Q_streq(token, "*MESH_FACE")) {
		ASE_GetToken(qfalse);	/* skip face number */

		ASE_GetToken(qfalse);	/* skip label */
		ASE_GetToken(qfalse);	/* first vertex */
		pMesh->faces[pMesh->currentFace][0] = atoi(s_token);

		ASE_GetToken(qfalse);	/* skip label */
		ASE_GetToken(qfalse);	/* second vertex */
		pMesh->faces[pMesh->currentFace][2] = atoi(s_token);

		ASE_GetToken(qfalse);	/* skip label */
		ASE_GetToken(qfalse);	/* third vertex */
		pMesh->faces[pMesh->currentFace][1] = atoi(s_token);

		ASE_GetToken(qtrue);

#if 0
		if ((p = strstr(s_token, "*MESH_MTLID")) != 0) {
			p += strlen("*MESH_MTLID") + 1;
			mtlID = atoi(p);
		} else {
			Sys_Error("No *MESH_MTLID found for face!");
		}
#endif

		pMesh->currentFace++;
	} else
		Sys_Error("Unknown token '%s' while parsing MESH_FACE_LIST", token);
}

static void ASE_KeyTFACE_LIST (const char *token)
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if (Q_streq(token, "*MESH_TFACE")) {
		int a, b, c;
		aseFace_t *f;

		ASE_GetToken(qfalse);

		ASE_GetToken(qfalse);
		a = atoi(s_token);
		ASE_GetToken(qfalse);
		c = atoi(s_token);
		ASE_GetToken(qfalse);
		b = atoi(s_token);

		f = &pMesh->tfaces[pMesh->currentFace];
		*f[0] = a;
		*f[1] = b;
		*f[2] = c;

		pMesh->currentFace++;
	} else
		Sys_Error("Unknown token '%s' in MESH_TFACE", token);
}

static void ASE_KeyMESH_TVERTLIST (const char *token)
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if (Q_streq(token, "*MESH_TVERT")) {
		char u[80], v[80], w[80];

		ASE_GetToken(qfalse);

		ASE_GetToken(qfalse);
		strcpy(u, s_token);

		ASE_GetToken(qfalse);
		strcpy(v, s_token);

		ASE_GetToken(qfalse);
		strcpy(w, s_token);

		pMesh->tvertexes[pMesh->currentVertex].s = atof(u);
		pMesh->tvertexes[pMesh->currentVertex].t = 1.0f - atof(v);

		pMesh->currentVertex++;

		if (pMesh->currentVertex > pMesh->numTVertexes) {
			Sys_Error("pMesh->currentVertex > pMesh->numTVertexes");
		}
	} else
		Sys_Error("Unknown token '%s' while parsing MESH_TVERTLIST", token);
}

static void ASE_KeyMESH (const char *token)
{
	aseMesh_t *pMesh = ASE_GetCurrentMesh();

	if (Q_streq(token, "*TIMEVALUE")) {
		ASE_GetToken(qfalse);

		pMesh->timeValue = atoi(s_token);
		VERBOSE((".....timevalue: %d\n", pMesh->timeValue));
	} else if (Q_streq(token, "*MESH_NUMVERTEX")) {
		ASE_GetToken(qfalse);

		pMesh->numVertexes = atoi(s_token);
		VERBOSE((".....TIMEVALUE: %d\n", pMesh->timeValue));
		VERBOSE((".....num vertexes: %d\n", pMesh->numVertexes));
	} else if (Q_streq(token, "*MESH_NUMFACES")) {
		ASE_GetToken(qfalse);

		pMesh->numFaces = atoi(s_token);
		VERBOSE((".....num faces: %d\n", pMesh->numFaces));
	} else if (Q_streq(token, "*MESH_NUMTVFACES")) {
		ASE_GetToken(qfalse);

		if (atoi(s_token) != pMesh->numFaces)
			Sys_Error("MESH_NUMTVFACES != MESH_NUMFACES");
	} else if (Q_streq(token, "*MESH_NUMTVERTEX")) {
		ASE_GetToken(qfalse);

		pMesh->numTVertexes = atoi(s_token);
		VERBOSE((".....num tvertexes: %d\n", pMesh->numTVertexes));
	} else if (Q_streq(token, "*MESH_VERTEX_LIST")) {
		pMesh->vertexes = (aseVertex_t *)Mem_Alloc(sizeof(aseVertex_t) * pMesh->numVertexes);
		pMesh->currentVertex = 0;
		VERBOSE((".....parsing MESH_VERTEX_LIST\n"));
		ASE_ParseBracedBlock(ASE_KeyMESH_VERTEX_LIST);
	} else if (Q_streq(token, "*MESH_TVERTLIST")) {
		pMesh->currentVertex = 0;
		pMesh->tvertexes = (aseTVertex_t *)Mem_Alloc(sizeof(aseTVertex_t) * pMesh->numTVertexes);
		VERBOSE((".....parsing MESH_TVERTLIST\n"));
		ASE_ParseBracedBlock(ASE_KeyMESH_TVERTLIST);
	} else if (Q_streq(token, "*MESH_FACE_LIST")) {
		pMesh->faces = (aseFace_t *)Mem_Alloc(sizeof(aseFace_t) * pMesh->numFaces);
		pMesh->currentFace = 0;
		VERBOSE((".....parsing MESH_FACE_LIST\n"));
		ASE_ParseBracedBlock(ASE_KeyMESH_FACE_LIST);
	} else if (Q_streq(token, "*MESH_TFACELIST")) {
		pMesh->tfaces = (aseFace_t *)Mem_Alloc(sizeof(aseFace_t) * pMesh->numFaces);
		pMesh->currentFace = 0;
		VERBOSE((".....parsing MESH_TFACE_LIST\n"));
		ASE_ParseBracedBlock(ASE_KeyTFACE_LIST);
	} else if (Q_streq(token, "*MESH_NORMALS")) {
		ASE_ParseBracedBlock(0);
	}
}

static void ASE_KeyGEOMOBJECT (const char *token)
{
	if (Q_streq(token, "*NODE_NAME")) {
		char *name = ase.objects[ase.currentObject].name;

		ASE_GetToken(qtrue);
		VERBOSE((" %s\n", s_token));
		strcpy(ase.objects[ase.currentObject].name, s_token + 1);
		if (strchr(ase.objects[ase.currentObject].name, '"'))
			*strchr(ase.objects[ase.currentObject].name, '"') = 0;

		if (strstr(name, "tag") == name) {
			while (strchr(name, '_') != strrchr(name, '_')) {
				*strrchr(name, '_') = 0;
			}
			while (strrchr(name, ' ')) {
				*strrchr(name, ' ') = 0;
			}
		}
	} else if (Q_streq(token, "*NODE_PARENT")) {
		ASE_SkipRestOfLine();
	}
	/* ignore unused data blocks */
	else if (Q_streq(token, "*NODE_TM") || Q_streq(token, "*TM_ANIMATION")) {
		ASE_ParseBracedBlock(0);
	}
	/* ignore regular meshes that aren't part of animation */
	else if (Q_streq(token, "*MESH")) {
#if 0
		if (strstr(ase.objects[ase.currentObject].name, "tag_") == ase.objects[ase.currentObject].name) {
			s_forceStaticMesh = true;
			ASE_ParseBracedBlock(ASE_KeyMESH);
			s_forceStaticMesh = false;
		}
#endif
		ASE_ParseBracedBlock(ASE_KeyMESH);
		if (++ase.objects[ase.currentObject].anim.currentFrame == MAX_ASE_ANIMATION_FRAMES) {
			Sys_Error("Too many animation frames");
		}
		ase.objects[ase.currentObject].anim.numFrames = ase.objects[ase.currentObject].anim.currentFrame;
		ase.objects[ase.currentObject].numAnimations++;
#if 0
		/* ignore meshes that aren't part of animations if this object isn't a a tag */
		else {
			ASE_ParseBracedBlock(0);
		}
#endif
	}
	/* according to spec these are obsolete */
	else if (Q_streq(token, "*MATERIAL_REF")) {
		ASE_GetToken(qfalse);

		ase.objects[ase.currentObject].materialRef = atoi(s_token);
	}
	/* ignore sequences of animation frames */
	else if (Q_streq(token, "*MESH_ANIMATION")) {
		ASE_SkipEnclosingBraces();
	}
	/* skip unused info */
	else if (Q_streq(token, "*PROP_MOTIONBLUR") || Q_streq(token, "*PROP_CASTSHADOW") || Q_streq(token, "*PROP_RECVSHADOW")) {
		ASE_SkipRestOfLine();
	}
}

static void ConcatenateObjects (aseGeomObject_t *pObjA, aseGeomObject_t *pObjB)
{
}

static void CollapseObjects (void)
{
	int i;
	int numObjects = ase.currentObject;

	for (i = 0; i < numObjects; i++) {
		int j;

		/* skip tags */
		if (strstr(ase.objects[i].name, "tag") == ase.objects[i].name)
			continue;

		if (!ase.objects[i].numAnimations)
			continue;

		for (j = i + 1; j < numObjects; j++) {
			if (strstr(ase.objects[j].name, "tag") == ase.objects[j].name)
				continue;

			if (ase.objects[i].materialRef == ase.objects[j].materialRef)
				if (ase.objects[j].numAnimations)
					ConcatenateObjects(&ase.objects[i], &ase.objects[j]);
		}
	}
}

static void ASE_Process (void)
{
	while (ASE_GetToken(qfalse)) {
		if (Q_streq(s_token, "*3DSMAX_ASCIIEXPORT") || Q_streq(s_token, "*COMMENT")) {
			ASE_SkipRestOfLine();
		} else if (Q_streq(s_token, "*SCENE"))
			ASE_SkipEnclosingBraces();
		else if (Q_streq(s_token, "*MATERIAL_LIST")) {
			VERBOSE(("MATERIAL_LIST\n"));

			ASE_ParseBracedBlock(ASE_KeyMATERIAL_LIST);
		} else if (Q_streq(s_token, "*GEOMOBJECT")) {
			VERBOSE(("GEOMOBJECT"));

			ASE_ParseBracedBlock(ASE_KeyGEOMOBJECT);

			if (strstr(ase.objects[ase.currentObject].name, "Bip") ||
				strstr(ase.objects[ase.currentObject].name, "ignore_")) {
				ASE_FreeGeomObject(ase.currentObject);
				VERBOSE(("(discarding BIP/ignore object)\n"));
			} else {
				if (++ase.currentObject == MAX_ASE_OBJECTS) {
					Sys_Error("Too many GEOMOBJECTs");
				}
			}
		} else if (s_token[0]) {
			Com_Printf("Unknown token '%s'\n", s_token);
		}
	}

	if (!ase.currentObject)
		Sys_Error("No animation data!");

	CollapseObjects();
}
