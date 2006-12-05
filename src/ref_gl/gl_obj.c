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
 * Modifications to integrate this code into ufoai added by Werner Höhrer and possibly other ufoai members in the future.
 */

/**
  * @todo Rename load/display functions to match the ones for md2/md3.
  * @todo Fix all usages of glut functions (pure GL equivalents?) - search for "hoehrer: removed"
  *
  */

/* hoehrer: removed
#include <GL/glut.h>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Vector */
/* hoehrer: removed
typedef float vec3_t[3];
typedef float vec4_t[4];
*/
#include "gl_local.h"

/* Vertex */
typedef struct
{
	vec4_t xyzw;
} obj_vertex_t;


/* Texture coordinates */
typedef struct
{
	vec3_t uvw;
} obj_texCoord_t;


/* Normal vector */
typedef struct
{
	vec3_t ijk;
} obj_normal_t;


/* Polygon/Face */
typedef struct
{
	GLenum type;        /* primitive type */
	int num_elems;      /* number of vertices */

	int *vert_indices;  /* vertex indices */
	int *uvw_indices;   /* texture coordinate indices */
	int *norm_indices;  /* normal vector indices */
} obj_face_t;


/* OBJ model structure */
typedef struct
{
	int num_verts;              /* number of vertices */
	int num_texCoords;          /* number of texture coords. */
	int num_normals;            /* number of normal vectors */
	int num_faces;              /* number of polygons */

	int has_texCoords;          /* has texture coordinates? */
	int has_normals;            /* has normal vectors? */

	obj_vertex_t *vertices;     /* vertex list */
	obj_texCoord_t *texCoords;  /* tex. coord. list */
	obj_normal_t *normals;      /* normal vector list */
	obj_face_t *faces;          /* model's polygons */
} obj_model_t;


obj_model_t *objfile;

/**
 * @brief 
 */
void FreeModel (obj_model_t *mdl)
{
	int i;

	if (mdl) {
		if (mdl->vertices) {
			free (mdl->vertices);
			mdl->vertices = NULL;
		}

		if (mdl->texCoords) {
			free (mdl->texCoords);
			mdl->texCoords = NULL;
		}

		if (mdl->normals) {
			free (mdl->normals);
			mdl->normals = NULL;
		}

		if (mdl->faces) {
			for (i = 0; i < mdl->num_faces; ++i) {
				if (mdl->faces[i].vert_indices)
					free(mdl->faces[i].vert_indices);

				if (mdl->faces[i].uvw_indices)
					free(mdl->faces[i].uvw_indices);

				if (mdl->faces[i].norm_indices)
					free(mdl->faces[i].norm_indices);
			}

			free(mdl->faces);
			mdl->faces = NULL;
		}
	}
}

/**
 * @brief 
 */
int MallocModel (obj_model_t *mdl)
{
	if (mdl->num_verts) {
		mdl->vertices = (obj_vertex_t *)
		malloc(sizeof (obj_vertex_t) * mdl->num_verts);
		if (!mdl->vertices)
			return 0;
	}

	if (mdl->num_texCoords) {
		mdl->texCoords = (obj_texCoord_t *) malloc(sizeof (obj_texCoord_t) * mdl->num_texCoords);
		if (!mdl->texCoords)
			return 0;
	}

	if (mdl->num_normals) {
		mdl->normals = (obj_normal_t *) malloc (sizeof (obj_normal_t) * mdl->num_normals);
		if (!mdl->normals)
			return 0;
	}

	if (mdl->num_faces) {
		mdl->faces = (obj_face_t *) malloc (sizeof (obj_face_t) * mdl->num_faces);
		if (!mdl->faces)
			return 0;
	}

	return 1;
}

/**
 * @brief 
 */
int FirstPass (FILE *fp, obj_model_t *mdl)
{
	int v, t, n;
	char buf[256];

	while (!feof (fp)) {
		/* read whole line */
		fgets(buf, sizeof (buf), fp);

		switch (buf[0]) {
		case 'v': {
			if (buf[1] == ' ') {
				/* Vertex */
				mdl->num_verts++;
			} else if (buf[1] == 't') {
				/* Texture coordinates */
				mdl->num_texCoords++;
			} else if (buf[1] == 'n') {
				/* Normal vector */
				mdl->num_normals++;
			} else {
				printf ("warning: unknown token \"%s\"! (ignoring)\n", buf);
			}
			break;
		} /* case 'v' */

		case 'f': {
			/* Face */
			if (sscanf (buf + 2, "%d/%d/%d", &v, &n, &t) == 3) {
				mdl->num_faces++;
				mdl->has_texCoords = 1;
				mdl->has_normals = 1;
			} else if (sscanf (buf + 2, "%d//%d", &v, &n) == 2) {
				mdl->num_faces++;
				mdl->has_texCoords = 0;
				mdl->has_normals = 1;
			} else if (sscanf (buf + 2, "%d/%d", &v, &t) == 2) {
				mdl->num_faces++;
				mdl->has_texCoords = 1;
				mdl->has_normals = 0;
			} else if (sscanf (buf + 2, "%d", &v) == 1) {
				mdl->num_faces++;
				mdl->has_texCoords = 0;
				mdl->has_normals = 0;
			} else {
				/* Should never be there or the model is very crappy. */
				fprintf (stderr, "Error: Found face with no vertex!\n");
			}

			break;
		} /* case 'f' */

		case 'g': {
			/* Group */
			/*	fscanf (fp, "%s", buf); */
			break;
		}

		default:
			break;
		} /* switch */
	} /* while */

	/* Check if informations are valid. */
	if ((mdl->has_texCoords && !mdl->num_texCoords) ||
	    (mdl->has_normals && !mdl->num_normals)) {
		fprintf (stderr, "error: contradiction between collected info!\n");
		return 0;
	}

	if (!mdl->num_verts) {
		fprintf (stderr, "error: no vertex found!\n");
		return 0;
	}

#if 1
	/* Verbose info */
	printf ("first pass results: found\n");
	printf ("   * %i vertices\n", mdl->num_verts);
	printf ("   * %i texture coords.\n", mdl->num_texCoords);
	printf ("   * %i normal vectors\n", mdl->num_normals);
	printf ("   * %i faces\n", mdl->num_faces);
	printf ("   * has texture coords.: %s\n", mdl->has_texCoords ? "yes" : "no");
	printf ("   * has normals: %s\n", mdl->has_normals ? "yes" : "no");
#endif

	return 1;
}

/**
 * @brief 
 */
int SecondPass (FILE *fp, obj_model_t *mdl)
{
	obj_vertex_t *pvert = mdl->vertices;
	obj_texCoord_t *puvw = mdl->texCoords;
	obj_normal_t *pnorm = mdl->normals;
	obj_face_t *pface = mdl->faces;
	char buf[128], *pbuf;
	int i;

	while (!feof (fp)) {
		/* Read whole line. */
		fgets(buf, sizeof (buf), fp);

		switch (buf[0]) {
		case 'v': {
			if (buf[1] == ' ') {
				/* Vertex */
				if (sscanf (buf + 2, "%f %f %f %f",
					&pvert->xyzw[0], &pvert->xyzw[1],
					&pvert->xyzw[2], &pvert->xyzw[3]) != 4)
				{
					if (sscanf (buf + 2, "%f %f %f",
						&pvert->xyzw[0],
						&pvert->xyzw[1], &pvert->xyzw[2] ) != 3)
					{
						fprintf (stderr, "error reading vertex data!\n");
						return 0;
					} else {
						pvert->xyzw[3] = 1.0;
					}
				}

				pvert++;
			} else if (buf[1] == 't') {
				/* Texture coordinates */
				if (sscanf (buf + 2, "%f %f %f",
					&puvw->uvw[0],
					&puvw->uvw[1], &puvw->uvw[2]) != 3)
				{
					if (sscanf (buf + 2, "%f %f",
						&puvw->uvw[0],
						&puvw->uvw[1]) != 2)
					{
						if (sscanf (buf + 2, "%f", &puvw->uvw[0]) != 1) {
							fprintf (stderr, "error reading texture coordinates!\n");
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
				if (sscanf (buf + 2, "%f %f %f",
					&pnorm->ijk[0],
					&pnorm->ijk[1],
					&pnorm->ijk[2]) != 3)
				{
					fprintf (stderr, "error reading normal vectors!\n");
					return 0;
				}
				pnorm++;
			}

			break;
		} /* case 'v' */

		case 'f': {
			pbuf = buf;
			pface->num_elems = 0;

			/* Count number of vertices for this face. */
			while (*pbuf) {
				if (*pbuf == ' ')
					pface->num_elems++;
				pbuf++;
			}

			/* select primitive type */
			if (pface->num_elems < 3) {
				fprintf (stderr, "error: a face must have at least 3 vertices!\n");
				return 0;
			} else if (pface->num_elems == 3) {
				pface->type = GL_TRIANGLES;
			} else if (pface->num_elems == 4) {
				pface->type = GL_QUADS;
			} else {
				pface->type = GL_POLYGON;
			}

			/* Memory allocation for vertices. */
			pface->vert_indices = (int *)malloc (sizeof (int) * pface->num_elems);

			if (mdl->has_texCoords)
				pface->uvw_indices = (int *)malloc (sizeof (int) * pface->num_elems);

			if (mdl->has_normals)
				pface->norm_indices = (int *)malloc (sizeof (int) * pface->num_elems);

			/* Read face data. */
			pbuf = buf;
			i = 0;

			for (i = 0; i < pface->num_elems; ++i) {
				pbuf = strchr (pbuf, ' ');
				pbuf++; /* Skip space. */

				/* Try reading vertices */
				if (sscanf (pbuf, "%d/%d/%d",
					&pface->vert_indices[i],
					&pface->uvw_indices[i],
					&pface->norm_indices[i]) != 3)
				{
					if (sscanf (pbuf, "%d//%d",
						&pface->vert_indices[i],
						&pface->norm_indices[i]) != 2)
					{
						if (sscanf (pbuf, "%d/%d",
							&pface->vert_indices[i],
							&pface->uvw_indices[i]) != 2)
						{
							sscanf (pbuf, "%d", &pface->vert_indices[i]);
						}
					}
				}

				/* indices must start at 0 */
				pface->vert_indices[i]--;

				if (mdl->has_texCoords)
					pface->uvw_indices[i]--;

				if (mdl->has_normals)
					pface->norm_indices[i]--;
			}

			pface++;
			break;
		} /* case 'f' */
	} /* switch */
}

#if 1
	/* Verbose info */
	printf ("second pass results: read\n");
	printf ("   * %i vertices\n", pvert - mdl->vertices);
	printf ("   * %i texture coords.\n", puvw - mdl->texCoords);
	printf ("   * %i normal vectors\n", pnorm - mdl->normals);
	printf ("   * %i faces\n", pface - mdl->faces);
#endif

	return 1;
}

/**
 * @brief 
 */
int ReadOBJModel (const char *filename, obj_model_t *mdl)
{
	FILE *fp = NULL;

	fp = fopen (filename, "r");
	if (!fp) {
		fprintf (stderr, "error: couldn't open \"%s\"!", filename);
		return 0;
	}

	/* Reset model data. */
	memset(mdl, 0, sizeof (obj_model_t));

	/* First pass: Read model info. */
	if (!FirstPass (fp, mdl)) {
		fclose (fp);
		return 0;
	}

	rewind(fp);

	/* Memory allocation */
	if (!MallocModel (mdl)) {
		fclose (fp);
		FreeModel (mdl);
		return 0;
	}

	/* Second pass: Read model data. */
		if (!SecondPass (fp, mdl)) {
		fclose (fp);
		FreeModel (mdl);
		return 0;
	}

	fclose (fp);
	return 1;
}

/**
 * @brief 
 */
void RenderOBJModel (obj_model_t *mdl)
{
	int i, j;

	for (i = 0; i < mdl->num_faces; ++i) {
		glBegin (mdl->faces[i].type);
		for (j = 0; j < mdl->faces[i].num_elems; ++j) {
			if (mdl->has_texCoords)
				glTexCoord3fv (mdl->texCoords[mdl->faces[i].uvw_indices[j]].uvw);

			if (mdl->has_normals)
				glNormal3fv (mdl->normals[mdl->faces[i].norm_indices[j]].ijk);

			glVertex4fv (mdl->vertices [mdl->faces[i].vert_indices[j]].xyzw);
		}
		glEnd();
	}
}

/**
 * @brief 
 */
void init (const char *filename)
{
	/* Initialize OpenGL */
	glClearColor (0.5f, 0.5f, 0.5f, 1.0f);
	glShadeModel (GL_SMOOTH);

	glEnable (GL_DEPTH_TEST);
	glEnable (GL_LIGHTING);
	glEnable (GL_LIGHT0);

	/* Initialize OpenGL light */
	GLfloat lightpos[] = { 5.0f, 10.0f, 0.0f, 1.0f };
	glLightfv (GL_LIGHT0, GL_POSITION, lightpos);

	/* Load model file */
	objfile = (obj_model_t *)malloc (sizeof (obj_model_t));

	if ( !ReadOBJModel (filename, objfile)) {
		exit (-1);
	}
}

/**
 * @brief Free memory.
 */
void shutdownApp (void)
{
	if (objfile) {
		FreeModel (objfile);
		free (objfile);
		objfile = NULL;
	}
}

/**
 * @brief 
 */
void reshape (int w, int h)
{
	if (h == 0)
		h = 1;

	glViewport (0, 0, (GLsizei)w, (GLsizei)h);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	gluPerspective (45.0, (GLfloat)w/(GLfloat)h, 0.1, 1000.0);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	/* hoehrer: removed
	glutPostRedisplay ();
	*/
}

/**
 * @brief 
 */
void display (void)
{
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity ();

	glTranslatef (0.0f, 0.0f, -15.0f);

	/* draw model */
	RenderOBJModel (objfile);

	/* hoehrer: removed
	glutSwapBuffers ();
	*/
}

#if 0
/* hoehrer: removed */
/**
 * @brief 
 */
void keyboard (unsigned char key, int x, int y)
{
	switch (key) {
	case 27: /* escape */
		exit(0);
		break;
	}
}

/**
 * @brief 
 */
int main (int argc, char *argv[])
{
	if (argc < 2) {
		fprintf (stderr, "usage: %s filename.obj\n", argv[0]);
		return -1;
	}

	glutInit (&argc, argv);
	glutInitDisplayMode (GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize (640, 480);
	glutCreateWindow ("OBJ Model");

	atexit (shutdownApp);
	init(argv[1]);

	glutReshapeFunc (reshape);
	glutDisplayFunc (display);
	glutKeyboardFunc (keyboard);

	glutMainLoop ();

	return 0;
}
#endif
