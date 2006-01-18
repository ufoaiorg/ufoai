#ifndef _GL_MD3_H
#define _GL_MD3_H

/* 
 * md3 header
 *
 */

typedef int aliasoffset_t;

typedef struct aliastag_s 
{
     vec3_t		origin;			// same as md3tag_t, but without the useless name 
     vec3_t		axis[3];		// 
     
} aliastag_t;
     

typedef struct alias3data_s {
     int		numSurfaces;		// size of tables
     //aliastag_t		weaponTag;		// 
     aliasoffset_t	ofsSurfaces[1];		// subsurfaces header table : allocated at loading time
} alias3data_t;


aliashdr_t *Mod_GetSurface (model_t *mod,int i);
model_t *Mod_GetSubModel (model_t *mod,int i);

#endif
