/**
 * @file r_model_alias.c
 * @brief shared alias model loading code (md2, md3)
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
#include "../../shared/parse.h"

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

void R_ModLoadAnims (mAliasModel_t *mod, void *buffer)
{
	const char *text, *token;
	mAliasAnim_t *anim;
	int n;

	for (n = 0, text = buffer; text; n++)
		Com_Parse(&text);
	n /= 4;
	if (n > MAX_ANIMS)
		n = MAX_ANIMS;

	mod->animdata = (mAliasAnim_t *) Mem_PoolAlloc(n * sizeof(*mod->animdata), vid_modelPool, 0);
	anim = mod->animdata;
	text = buffer;
	mod->num_anims = 0;

	do {
		/* get the name */
		token = Com_Parse(&text);
		if (!text)
			break;
		Q_strncpyz(anim->name, token, sizeof(anim->name));

		/* get the start */
		token = Com_Parse(&text);
		if (!text)
			break;
		anim->from = atoi(token);
		if (anim->from < 0)
			Com_Error(ERR_FATAL, "R_ModLoadAnims: negative start frame for %s", mod->animname);
		else if (anim->from > mod->num_frames)
			Com_Error(ERR_FATAL, "R_ModLoadAnims: start frame is higher than models frame count (%i) (model: %s)",
					mod->num_frames, mod->animname);

		/* get the end */
		token = Com_Parse(&text);
		if (!text)
			break;
		anim->to = atoi(token);
		if (anim->to < 0)
			Com_Error(ERR_FATAL, "R_ModLoadAnims: negative start frame for %s", mod->animname);
		else if (anim->to > mod->num_frames)
			Com_Error(ERR_FATAL, "R_ModLoadAnims: end frame is higher than models frame count (%i) (model: %s)",
					mod->num_frames, mod->animname);

		/* get the fps */
		token = Com_Parse(&text);
		if (!text)
			break;
		anim->time = (atof(token) > 0.01) ? (1000.0 / atof(token)) : (1000.0 / 0.01);

		/* add it */
		mod->num_anims++;
		anim++;
	} while (mod->num_anims < MAX_ANIMS);
}
