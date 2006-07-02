/* gl_mesh.c: triangle model functions */

#include "gl_local.h"

/*
=============================================================

  PARTICLE DRAWING

=============================================================
*/

/*
===============
R_DrawSprite
===============
*/
void R_DrawSprite(ptl_t * p)
{
	vec3_t up, right;
	vec3_t pos;

	/* get transformation */
	switch (p->style) {
	case STYLE_FACING:
		VectorScale(vright, p->size[0], right);
		VectorScale(vup, p->size[1], up);
		break;

	case STYLE_ROTATED:
		AngleVectors(p->angles, NULL, right, up);
		VectorScale(right, p->size[0], right);
		VectorScale(up, p->size[1], up);
		break;

	case STYLE_BEAM:
		AngleVectors(p->angles, right, NULL, NULL);
		CrossProduct(right, vpn, up);
		VectorNormalize(up);
		VectorScale(right, p->size[0], right);
		VectorScale(up, p->size[1], up);
		break;

	default:
		/* shouldn't happen */
		return;
	}

	/* load texture set up coordinates */
	GL_Bind(((image_t *) r_newrefdef.ptl_art[p->pic].art)->texnum);

	VectorMA(p->s, -0.5, up, pos);
	VectorMA(pos, -0.5, right, pos);

	/* draw it */
	qglBegin(GL_TRIANGLE_FAN);

	qglColor4fv(p->color);

	qglTexCoord2f(0, 0);
	qglVertex3fv(pos);

	VectorAdd(pos, up, pos);
	qglTexCoord2f(0, 1);
	qglVertex3fv(pos);

	VectorAdd(pos, right, pos);
	qglTexCoord2f(1, 1);
	qglVertex3fv(pos);

	VectorSubtract(pos, up, pos);
	qglTexCoord2f(1, 0);
	qglVertex3fv(pos);

	qglEnd();
}


/*
===============
R_DrawPtlModel
===============
*/
void R_DrawPtlModel(ptl_t * p)
{
	modelInfo_t mi;

	/* initialize minfo */
	memset(&mi, 0, sizeof(modelInfo_t));
	mi.color = p->color;
	mi.origin = p->s;
	mi.angles = p->angles;
	mi.model = (model_t *) r_newrefdef.ptl_art[p->model].art;

	/* draw it */
	R_DrawModelParticle(&mi);
}


/*
===============
R_DrawPtlLine
===============
*/
void R_DrawPtlLine(ptl_t * p)
{
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_LINE_SMOOTH);

	/* draw line from s to v */
	qglBegin(GL_LINE_STRIP);
	qglColor4fv(p->color);
	qglVertex3fv(p->s);
	qglVertex3fv(p->v);
	qglEnd();

	qglDisable(GL_LINE_SMOOTH);
	qglEnable(GL_TEXTURE_2D);
}


/*
=============================================================

  GENERIC PARTICLE FUNCTIONS

=============================================================
*/

/*
===============
GL_SetBlendMode
===============
*/
static int blend_mode;

void GL_SetBlendMode(int mode)
{
	if (blend_mode != mode) {
		blend_mode = mode;
		switch (mode) {
		case BLEND_REPLACE:
			GL_TexEnv(GL_REPLACE);
			break;
		case BLEND_BLEND:
			GL_TexEnv(GL_MODULATE);
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case BLEND_ADD:
			GL_TexEnv(GL_MODULATE);
			qglBlendFunc(GL_ONE, GL_ONE);
			break;
		case BLEND_FILTER:
			GL_TexEnv(GL_MODULATE);
			qglBlendFunc(GL_ZERO, GL_SRC_COLOR);
			break;
		case BLEND_INVFILTER:
			GL_TexEnv(GL_MODULATE);
			qglBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
			break;
		default:
			ri.Sys_Error(ERR_DROP, "unknown blend mode");
			break;
		}
	}
}


/*
===============
R_DrawPtls
===============
*/
void R_DrawPtls(void)
{
	ptl_t *p;
	int i;

	if (gl_fog->value && r_newrefdef.fog)
		qglDisable(GL_FOG);
	/* no z buffering */
	qglDepthMask(GL_FALSE);
	qglDisable(GL_CULL_FACE);
	qglEnable(GL_BLEND);
	blend_mode = BLEND_REPLACE;

	for (i = 0, p = r_newrefdef.ptls; i < r_newrefdef.num_ptls; i++, p++)
		if (p->inuse) {
			/* test for visibility */
			if (p->levelFlags && !((1 << r_newrefdef.worldlevel) & p->levelFlags))
				continue;

			/* set blend mode and draw gfx */
			GL_SetBlendMode(p->blend);
			if (p->style == STYLE_LINE)
				R_DrawPtlLine(p);
			if (p->pic != -1)
				R_DrawSprite(p);
			if (p->model != -1) {
				qglEnable(GL_CULL_FACE);
				R_DrawPtlModel(p);
				qglDisable(GL_CULL_FACE);
				blend_mode = -1;
			}
		}

	qglDisable(GL_BLEND);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglEnable(GL_CULL_FACE);
	qglDepthMask(GL_TRUE);
	if (gl_fog->value && r_newrefdef.fog)
		qglEnable(GL_FOG);
}
