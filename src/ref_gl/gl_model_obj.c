/**
 * @file gl_obj.c
 * @brief Wavefront (.obj) model loader & display code.
 *
 * ======= Original file information =======
 * obj.c -- obj model loader
 * last modification: dec. 15, 2005
 *
 * Copyright (c) 2005 David HENRY
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * gcc -Wall -ansi -L/usr/X11R6/lib -lGL -lGLU -lglut obj.c -o obj
 * ==========================================
 *
 * Modifications to integrate this code into ufoai added by Werner Hoehrer and possibly other ufoai members in the future.
 */

/**
 * @todo Rename load/display functions to match the ones for md2/md3.
 * @todo transfer this to model_t - see gl_model.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "gl_local.h"

/** @brief obj Vertex */
typedef struct {
	vec4_t xyzw;
} obj_vertex_t;


/** @brief obj Texture coordinates */
typedef struct obj_texCoord_s {
	vec3_t uvw;
} obj_texCoord_t;


/** @brief obj Polygon/Face */
typedef struct obj_face_s {
	GLenum type;		/**< primitive type */
	int num_elems;		/**< number of vertices */

	int *vert_indices;	/**< vertex indices */
	int *uvw_indices;	/**< texture coordinate indices */
	int *norm_indices;	/**< normal vector indices */
} obj_face_t;


/** @brief obj model structure */
typedef struct obj_model_s {
	char name[MAX_VAR];			/**< the model name */
	int num_texCoords;			/**< number of texture coords. */
	int num_faces;				/**< number of polygons */

	int has_texCoords;			/**< has texture coordinates? */
	int has_normals;			/**< has normal vectors? */

	obj_texCoord_t *texCoords;	/**< tex. coord. list */
	obj_face_t *faces;			/**< model's polygons */
} obj_model_t;

/**
 * @brief
 * @sa R_OBJ_SecondPass
 */
static int R_OBJ_FirstPass (model_t *mod, void* buffer)
{
#if 0
	int v, t, n;
	char buf[256];

	while (!feof (buffer)) {
		/* read whole line */
		fgets(buf, sizeof (buf), buffer);

		switch (buf[0]) {
		case 'v': {
			if (buf[1] == ' ') {
				/* Vertex */
				mod->numvertexes++;
			} else if (buf[1] == 't') {
				/* Texture coordinates */
				mod->num_texCoords++;
			} else if (buf[1] == 'n') {
				/* Normal vector */
				mod->numnormals++;
			} else {
				ri.Con_Printf(PRINT_ALL, "warning: unknown token \"%s\"! (ignoring)\n", buf);
			}
			break;
		} /* case 'v' */

		case 'f': {
			/* Face */
			if (sscanf(buf + 2, "%d/%d/%d", &v, &n, &t) == 3) {
				mod->num_faces++;
				mod->has_texCoords = 1;
				mod->has_normals = 1;
			} else if (sscanf(buf + 2, "%d//%d", &v, &n) == 2) {
				mod->num_faces++;
				mod->has_texCoords = 0;
				mod->has_normals = 1;
			} else if (sscanf(buf + 2, "%d/%d", &v, &t) == 2) {
				mod->num_faces++;
				mod->has_texCoords = 1;
				mod->has_normals = 0;
			} else if (sscanf(buf + 2, "%d", &v) == 1) {
				mod->num_faces++;
				mod->has_texCoords = 0;
				mod->has_normals = 0;
			} else {
				/* Should never be there or the model is very crappy. */
				ri.Con_Printf(PRINT_ALL, "Error: Found face with no vertex!\n");
			}

			break;
		} /* case 'f' */

		case 'g': {
			/* Group */
			/*	fscanf(fp, "%s", buf); */
			break;
		}

		default:
			break;
		} /* switch */
	} /* while */

	/* Check if informations are valid. */
	if ((mod->has_texCoords && !mod->num_texCoords) ||
	    (mod->has_normals && !mod->numnormals)) {
		ri.Con_Printf(PRINT_ALL, "error: contradiction between collected info!\n");
		return 0;
	}

	if (!mod->numvertexes) {
		ri.Con_Printf(PRINT_ALL, "error: no vertex found!\n");
		return 0;
	}

#if 1
	/* Verbose info */
	ri.Con_Printf(PRINT_ALL, "first pass results: found\n");
	ri.Con_Printf(PRINT_ALL, "   * %i vertices\n", mod->numvertexes);
	ri.Con_Printf(PRINT_ALL, "   * %i texture coords.\n", mod->num_texCoords);
	ri.Con_Printf(PRINT_ALL, "   * %i normal vectors\n", mod->numnormals);
	ri.Con_Printf(PRINT_ALL, "   * %i faces\n", mod->num_faces);
	ri.Con_Printf(PRINT_ALL, "   * has texture coords.: %s\n", mod->has_texCoords ? "yes" : "no");
	ri.Con_Printf(PRINT_ALL, "   * has normals: %s\n", mod->has_normals ? "yes" : "no");
#endif

#endif /* 0 */
	return 1;
}

/**
 * @brief
 * @sa R_OBJ_FirstPass
 */
static int R_OBJ_SecondPass (model_t *mod, void *buffer)
{
#if 0
	obj_vertex_t *pvert = mod->vertexes;
	obj_texCoord_t *puvw = mod->texCoords;
	obj_normal_t *pnorm = mod->normals;
	obj_face_t *pface = mod->faces;
	char buf[128], *pbuf;
	int i;

	while (!feof (buffer)) {
		/* Read whole line. */
		fgets(buf, sizeof(buf), buffer);

		switch (buf[0]) {
		case 'v': {
			if (buf[1] == ' ') {
				/* Vertex */
				if (sscanf(buf + 2, "%f %f %f %f",
					&pvert->xyzw[0], &pvert->xyzw[1],
					&pvert->xyzw[2], &pvert->xyzw[3]) != 4) {
					if (sscanf(buf + 2, "%f %f %f",
						&pvert->xyzw[0],
						&pvert->xyzw[1], &pvert->xyzw[2] ) != 3) {
						ri.Con_Printf(PRINT_ALL, "error reading vertex data!\n");
						return 0;
					} else {
						pvert->xyzw[3] = 1.0;
					}
				}

				pvert++;
			} else if (buf[1] == 't') {
				/* Texture coordinates */
				if (sscanf(buf + 2, "%f %f %f",
					&puvw->uvw[0],
					&puvw->uvw[1], &puvw->uvw[2]) != 3) {
					if (sscanf(buf + 2, "%f %f",
						&puvw->uvw[0],
						&puvw->uvw[1]) != 2) {
						if (sscanf(buf + 2, "%f", &puvw->uvw[0]) != 1) {
							ri.Con_Printf(PRINT_ALL, "error reading texture coordinates!\n");
							return 0;
						} else {
							puvw->uvw[1] = 0.0;
							puvw->uvw[2] = 0.0;
						}
					} else {
						puvw->uvw[2] = 0.0;
					}
				}
				puvw++;
			} else if (buf[1] == 'n') {
				/* Normal vector */
				if (sscanf(buf + 2, "%f %f %f",
					&pnorm->ijk[0],
					&pnorm->ijk[1],
					&pnorm->ijk[2]) != 3) {
					ri.Con_Printf(PRINT_ALL, "error reading normal vectors!\n");
					return 0;
				}
				pnorm++;
			}

			break;
		} /* case 'v' */

		case 'f': {
			pbuf = buf;
			pface->num_elems = 0;

			/* Count number of vertexes for this face. */
			while (*pbuf) {
				if (*pbuf == ' ')
					pface->num_elems++;
				pbuf++;
			}

			/* select primitive type */
			if (pface->num_elems < 3) {
				ri.Con_Printf(PRINT_ALL, "error: a face must have at least 3 vertexes!\n");
				return 0;
			} else if (pface->num_elems == 3) {
				pface->type = GL_TRIANGLES;
			} else if (pface->num_elems == 4) {
				pface->type = GL_QUADS;
			} else {
				pface->type = GL_POLYGON;
			}

			/* Memory allocation for vertexes. */
			pface->vert_indices = (int *)ri.TagMalloc(ri.modelPool, sizeof(int) * pface->num_elems, 0);

			if (mod->has_texCoords)
				pface->uvw_indices = (int *)ri.TagMalloc(ri.modelPool, sizeof(int) * pface->num_elems, 0);

			if (mod->has_normals)
				pface->norm_indices = (int *)ri.TagMalloc(ri.modelPool, sizeof(int) * pface->num_elems, 0);

			/* Read face data. */
			pbuf = buf;
			i = 0;

			for (i = 0; i < pface->num_elems; ++i) {
				pbuf = strchr(pbuf, ' ');
				pbuf++; /* Skip space. */

				/* Try reading vertexes */
				if (sscanf(pbuf, "%d/%d/%d",
					&pface->vert_indices[i],
					&pface->uvw_indices[i],
					&pface->norm_indices[i]) != 3) {
					if (sscanf(pbuf, "%d//%d",
						&pface->vert_indices[i],
						&pface->norm_indices[i]) != 2) {
						if (sscanf(pbuf, "%d/%d",
							&pface->vert_indices[i],
							&pface->uvw_indices[i]) != 2) {
							sscanf(pbuf, "%d", &pface->vert_indices[i]);
						}
					}
				}

				/* indices must start at 0 */
				pface->vert_indices[i]--;

				if (mod->has_texCoords)
					pface->uvw_indices[i]--;

				if (mod->has_normals)
					pface->norm_indices[i]--;
			}

			pface++;
			break;
		} /* case 'f' */
	} /* switch */
}

#ifdef DEBUG
	/* Verbose info */
	ri.Con_Printf(PRINT_ALL, "second pass results: read\n");
	ri.Con_Printf(PRINT_ALL, "   * %i vertexes\n", pvert - mod->vertexes);
	ri.Con_Printf(PRINT_ALL, "   * %i texture coords.\n", puvw - mod->texCoords);
	ri.Con_Printf(PRINT_ALL, "   * %i normal vectors\n", pnorm - mod->normals);
	ri.Con_Printf(PRINT_ALL, "   * %i faces\n", pface - mod->faces);
#endif

#endif /* 0 */
	return 1;
}

/**
 * @brief
 */
int Mod_LoadOBJModel (model_t* mod, void* buffer, int bufSize)
{
	void *tmp = buffer;

	/* First pass: Read model info. */
	if (!R_OBJ_FirstPass(buffer, mod))
		return 0;

	buffer = tmp;

	/* Second pass: Read model data. */
	if (!R_OBJ_SecondPass(mod, buffer))
		return 0;

	mod->type = mod_obj;

	return 1;
}

/**
 * @brief
 */
static void R_OBJ_Render (model_t *mod)
{
#if 0
	int i, j;

	for (i = 0; i < mod->num_faces; ++i) {
		qglBegin(mod->faces[i].type);
		for (j = 0; j < mod->faces[i].num_elems; ++j) {
			if (mod->has_texCoords)
				qglTexCoord3fv(mod->texCoords[mod->faces[i].uvw_indices[j]].uvw);

			if (mod->has_normals)
				qglNormal3fv(mod->normals[mod->faces[i].norm_indices[j]].ijk);

			qglVertex4fv(mod->vertexes [mod->faces[i].vert_indices[j]].xyzw);
		}
		qglEnd();
	}
#endif
}

/**
 * @brief
 * @s R_OBJ_Render
 */
void R_DrawOBJModel (entity_t *e)
{
	currentmodel = e->model;
	assert(currentmodel->type == mod_obj);
	R_OBJ_Render(currentmodel);
}
