/*
Copyright (C) 2001-2002 Charles Hollemeersch

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

PENTA: the whole file is freakin penta...

  Md3 support
*/

#include "gl_md3.h"

#define MD3_VERSION			15
#define	MD3_XYZ_SCALE		(1.0/64)

//#define MD3DEBUG

typedef struct md3Frame_s {
	vec3_t		bounds[2];
	vec3_t		localOrigin;
	float		radius;
	char		name[16];
} md3Frame_t;

typedef struct {
	char			name[MAX_QPATH];
	int				shaderIndex;
} md3Shader_t;

typedef struct {
	int		ident;				// 

	char	name[MAX_QPATH];	// polyset name

	int		flags;
	int		numFrames;			// all surfaces in a model should have the same

	int		numShaders;			// all surfaces in a model should have the same
	int		numVerts;

	int		numTriangles;
	int		ofsTriangles;

	int		ofsShaders;			// offset from start of md3Surface_t
	int		ofsSt;				// texture coords are common for all frames
	int		ofsXyzNormals;		// numVerts * numFrames

	int		ofsEnd;				// next surface follows
} md3Surface_t;

typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];

	int			flags;

	int			numFrames;
	int			numTags;			
	int			numSurfaces;

	int			numSkins;

	int			ofsFrames;
	int			ofsTags;
	int			ofsSurfaces;
	int			ofsEnd;
} md3Header_t;

typedef struct {
	short		xyz[3];
	unsigned short normal;
} md3XyzNormal_t;

typedef struct {
	int			indexes[3];
} md3Triangle_t;

typedef struct {
	float		s;
	float		t;
} md3St_t;


typedef struct md3tag_s
{
     char		name[MAX_QPATH];	// supported names : weapon
     vec3_t		origin;			// pretty much self explanatory
     vec3_t		axis[3];		// no ?
     
} md3tag_t;


int findneighbourmd3_old(int index, int edgei, int numtris, mtriangle_t *triangles) {
	int i, j;
	mtriangle_t *current = &triangles[index];

	for (i=0; i<numtris; i++) {
		if (i == index) continue;
		for (j=0; j<3; j++) {      
			if (((current->vertindex[edgei] == triangles[i].vertindex[j]) 
			    && (current->vertindex[(edgei+1)%3] == triangles[i].vertindex[(j+1)%3]))
				||
			   ((current->vertindex[edgei] == triangles[i].vertindex[(j+1)%3]) 
			    && (current->vertindex[(edgei+1)%3] == triangles[i].vertindex[j])))
			{
				triangles[i].neighbours[j] = index;
				return i;
			}
  
		}
	}
	return -1;
}
/*
Yet another hack.  Some models seem to have edges shared between three triangles, this is obviously
a strange thing to have, we resolve it simply by throwing away that shared egde and giving all
triangles a "-1" neighbour for that edge.  This will give some unneeded fins for some edges of some models
but this number is generally verry low (< 3 edges per model) and only on a few models.
*/
int findneighbourmd3(int index, int edgei, int numtris, mtriangle_t *triangles) {
	int i, j, v1, v0, found,foundj = 0;
	mtriangle_t *current = &triangles[index];
	mtriangle_t *t;
	qboolean	dup;

	v0 = current->vertindex[edgei];
	v1 = current->vertindex[(edgei+1)%3];

	//XYZ
	found = -1;
	dup = false;
	for (i=0; i<numtris; i++) {
		if (i == index) continue;
		t = &triangles[i];

		for (j=0; j<3; j++) {      
			if (((current->vertindex[edgei] == triangles[i].vertindex[j]) 
			    && (current->vertindex[(edgei+1)%3] == triangles[i].vertindex[(j+1)%3]))
				||
			   ((current->vertindex[edgei] == triangles[i].vertindex[(j+1)%3]) 
			    && (current->vertindex[(edgei+1)%3] == triangles[i].vertindex[j])))
			{
				//no edge for this model found yet?
				if (found == -1) {
					found = i;
					foundj = j;
				}
				//the three edges story
				else
					dup = true;
			}
  
		}
	}

	//normal edge, setup neighbour pointers
	if (!dup) {
		if (found != -1)
			triangles[found].neighbours[foundj] = index;
		return found;
	}
	//naughty egde let no-one have the neighbour
	//Con_Printf("%s: warning: open edge added\n",loadname);
	return -1;
}

void TangentForTrimd3(int *index, ftrivertx_t *vertices, fstvert_t *texcos, vec3_t Tangent, vec3_t Binormal) {
//see:
//http://members.rogers.com/deseric/tangentspace.htm
	vec3_t stvecs [3];
	float *v0, *v1, *v2;
	float *st0, *st1, *st2;
	vec3_t vec1, vec2;
	vec3_t planes[3];
	int i;

	v0 = &vertices[index[0]].v[0];
	v1 = &vertices[index[1]].v[0];
	v2 = &vertices[index[2]].v[0];
	st0 = &texcos[index[0]].s;
	st1 = &texcos[index[1]].s;
	st2 = &texcos[index[2]].s;

	for (i=0; i<3; i++) {
		vec1[0] = v1[i]-v0[i];
		vec1[1] = st1[0]-st0[0];
		vec1[2] = st1[1]-st0[1];
		vec2[0] = v2[i]-v0[i];
		vec2[1] = st2[0]-st0[0];
		vec2[2] = st2[1]-st0[1];
		VectorNormalize(vec1);
		VectorNormalize(vec2);
		CrossProduct(vec1,vec2,planes[i]);
	}

	Tangent[0] = -planes[0][1]/planes[0][0];
	Tangent[1] = -planes[1][1]/planes[1][0];
	Tangent[2] = -planes[2][1]/planes[2][0];
	Binormal[0] = -planes[0][2]/planes[0][0];
	Binormal[1] = -planes[1][2]/planes[1][0];
	Binormal[2] = -planes[2][2]/planes[2][0];
	VectorNormalize(Tangent); //is this needed?
	VectorNormalize(Binormal);
}

void ClosestPointOnLine(vec3_t a, vec3_t b, vec3_t p, vec3_t res)
{
	vec3_t c,V;
	float d,t ;

	// a-b is the line, p the point in question
	VectorSubtract(p, a, c);
	VectorSubtract(b, a, V);
	d = Length(V);
	VectorNormalize(V); // normalize V
	t = DotProduct(V,c);

	// Check to see if t is beyond the extents of the line segment
	if (t < 0.0f)
	{
		VectorCopy(a, res);
	}
	if (t > d)
	{
		VectorCopy(b, res);
	}
	// Return the point between a and b
	VectorScale(V, t, V); //set length of V to t.
	VectorAdd(a, V, res);
}

void Orthogonalize(vec3_t v1, vec3_t v2, vec3_t res)
{
	vec3_t v2ProjV1;
	vec3_t iV1;
	VectorScale(v1, -1.0f, iV1);
	ClosestPointOnLine(v1, iV1, v2, v2ProjV1);
	VectorSubtract(v2, v2ProjV1, res);
	VectorNormalize(res);
}

void DecodeNormal(int quant, vec3_t norm) {
	
	float lat = ( quant >> 8 ) & 0xff;
	float lng = ( quant & 0xff );
	lat *= M_PI_F/128;
	lng *= M_PI_F/128;

	norm[0] = cos(lat) * sin(lng);
	norm[1] = sin(lat) * sin(lng);
	norm[2] = cos(lng);
}

/**
	Set up data that we need to calculate (not stored in the file)
*/
void SetupModelSurface(aliashdr_t	*pheader)
{
	int	i, j, k, l;
	mtriangle_t *tris;
	ftrivertx_t	*verts, *v;
	vec3_t triangle[3];
	plane_t	*norms;
	vec3_t	*tangents, *binormals;
	vec3_t v1, v2, normal;
	int	*indecies;
	fstvert_t *texcoords;

	tris = (mtriangle_t *)((byte *)pheader + pheader->triangles);
	verts = (ftrivertx_t *)((byte *)pheader + pheader->posedata);
	indecies = (int *)((byte *)pheader + pheader->indecies);
	texcoords = (fstvert_t *)((byte *)pheader + pheader->texcoords);

//Setup connectivity
	for (i=0; i<pheader->numtris; i++)
		for (j=0 ; j<3 ; j++) {
			//none found yet 
			if (tris[i].neighbours[j] == -1) {
				tris[i].neighbours[j] = findneighbourmd3(i, j, pheader->numtris, tris);
			}
		}

//Calculate plane equations
	norms = Hunk_Alloc (pheader->numtris * pheader->numposes * sizeof(plane_t));
	pheader->planes = (byte *)norms - (byte *)pheader;
	for (i=0; i<pheader->numposes; i++) {
		for (j=0; j<pheader->numtris ; j++) {

			//make 3 vec3_t's of this triangle's vertices
			for (k=0; k<3; k++) {
				v = &verts[i*pheader->poseverts + tris[j].vertindex[k]];
				for (l=0; l<3; l++)
					triangle[k][l] = v->v[l];		
			}

			//calculate their normal
			VectorSubtract(triangle[0], triangle[1], v1);
			VectorSubtract(triangle[2], triangle[1], v2);
			CrossProduct(v2,v1, normal);		
			VectorScale(normal, 1/Length(normal), norms[i*pheader->numtris+j].normal);
			
			//distance of plane eq
			norms[i*pheader->numtris+j].dist = DotProduct(triangle[0],norms[i*pheader->numtris+j].normal);
		}
	}

//Calculate tangents for vertices
	tangents = Hunk_Alloc (pheader->poseverts * pheader->numposes * sizeof(vec3_t));
	pheader->tangents = (byte *)tangents - (byte *)pheader;

	binormals = Hunk_Alloc (pheader->poseverts * pheader->numposes * sizeof(vec3_t));
	pheader->binormals = (byte *)binormals - (byte *)pheader;
	//for all frames
	for (i=0; i<pheader->numposes; i++) {

		//set temp to zero
		for (j=0; j<pheader->poseverts; j++) {
			tangents[i*pheader->poseverts+j][0] = 0;
			tangents[i*pheader->poseverts+j][1] = 0;
			tangents[i*pheader->poseverts+j][2] = 0;
			binormals[i*pheader->poseverts+j][0] = 0;
			binormals[i*pheader->poseverts+j][1] = 0;
			binormals[i*pheader->poseverts+j][2] = 0;
			numNormals[j] = 0;
		}

		//for all tris
		for (j=0; j<pheader->numtris; j++) {
			vec3_t tangent;
			vec3_t binormal;
			vec3_t normal;
			TangentForTrimd3(&indecies[j*3],&verts[i*pheader->poseverts],texcoords,tangent,binormal);
			//for all vertices in the tri
			for (k=0; k<3; k++) {
				l = indecies[j*3+k];

				VectorAdd(tangents[i*pheader->poseverts+l],tangent,
							tangents[i*pheader->poseverts+l]); 
				VectorAdd(binormals[i*pheader->poseverts+l],binormal,
							binormals[i*pheader->poseverts+l]); 
				numNormals[l]++;
			}
		}
		
		//calculate average
		for (j=0; j<pheader->poseverts; j++) {
			vec3_t norm;
			float lat, lng;
			if (!numNormals[j]) continue;
			tangents[i*pheader->poseverts+j][0] = tangents[i*pheader->poseverts+j][0]/numNormals[j];
			tangents[i*pheader->poseverts+j][1] = tangents[i*pheader->poseverts+j][1]/numNormals[j];
			tangents[i*pheader->poseverts+j][2] = tangents[i*pheader->poseverts+j][2]/numNormals[j];

			binormals[i*pheader->poseverts+j][0] = binormals[i*pheader->poseverts+j][0]/numNormals[j];
			binormals[i*pheader->poseverts+j][1] = binormals[i*pheader->poseverts+j][1]/numNormals[j];
			binormals[i*pheader->poseverts+j][2] = binormals[i*pheader->poseverts+j][2]/numNormals[j];

			VectorNormalize(tangents[i*pheader->poseverts+j]);
			VectorNormalize(binormals[i*pheader->poseverts+j]);
			
			DecodeNormal(verts[i*pheader->poseverts+j].lightnormalindex, normal);

			Orthogonalize(normal, tangents[i*pheader->poseverts+j], tangents[i*pheader->poseverts+j]);
			Orthogonalize(normal, binormals[i*pheader->poseverts+j], binormals[i*pheader->poseverts+j]);
			
		}
	}
}

/*
=================
Mod_LoadMd3Model

PENTA: Very similar to LoadAliasModel
DC: added multiple surface -> alias3data_t 
=================
*/

#define	LL(x) x=LittleLong(x)

void Mod_LoadMd3Model (model_t *mod, void *buffer)
{
	int					i, j, k, l;
	md3Header_t			*pinmodel;
	int					size;
	int					start, end, total;
	md3Surface_t		*surf;
	md3tag_t			*tag;
	vec3_t				md3scale = {MD3_XYZ_SCALE, 	MD3_XYZ_SCALE, 	MD3_XYZ_SCALE};
	vec3_t				md3origin = {0.0f, 0.0f, 0.0f};
	ftrivertx_t			*verts, *v;
	md3XyzNormal_t		*xyz;
	mtriangle_t			*tris;
	md3Triangle_t		*tri;
	md3St_t				*st;
	md3Frame_t			*frame;
	int					*indecies;
	md3Shader_t			*shader;
	fstvert_t			*texcoords;
	byte				fake[16];
	char				shadername[MAX_QPATH];
        alias3data_t			*palias3;
        int				surfcount;   
	
	start = Hunk_LowMark ();

	//Con_Printf("Loading md3 from %s\n",mod->name);
	pinmodel = (md3Header_t *)buffer;

//
// endian-adjust and copy the data, starting with the md3 header
//


	LL(pinmodel->version);
	if (pinmodel->version != MD3_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, pinmodel->version, MD3_VERSION);

	//swap header
	LL(pinmodel->numFrames);
	LL(pinmodel->numTags);			
	LL(pinmodel->numSurfaces);

	LL(pinmodel->numSkins);

	LL(pinmodel->ofsFrames);
	LL(pinmodel->ofsTags);
	LL(pinmodel->ofsSurfaces);
	LL(pinmodel->ofsEnd);

#ifdef MD3DEBUG
	Con_Printf("Statistics for model %s\n",loadname);
	Con_Printf("NumFrames: %i\n",pinmodel->numFrames);
	Con_Printf("NumSurfaces: %i\n",pinmodel->numSurfaces);
	Con_Printf("NumSkins: %i\n",pinmodel->numSkins);
        Con_Printf("NumTags: %i\n",surf->numTags);
#endif

	if ( pinmodel->numFrames < 1 ) {
		Con_Printf( "LoadMd3Model: %s has no frames\n", mod->name );
		return;
	}

	if ( pinmodel->numFrames > MAXALIASFRAMES) {
		Sys_Error ("LoadMd3Model: %s has too many frames",mod->name);
	}

	// swap all the frames
    frame = (md3Frame_t *) ( (byte *)pinmodel + pinmodel->ofsFrames );
    for ( i = 0 ; i < pinmodel->numFrames ; i++, frame++) {
    	frame->radius = LittleFloat( frame->radius );
        for ( j = 0 ; j < 3 ; j++ ) {
            frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
            frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
	    	frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
        }
	}

	// swap all the surfaces
	surf = (md3Surface_t *) ( (byte *)pinmodel + pinmodel->ofsSurfaces );

	if (pinmodel->numSurfaces > 1) {
		Con_Printf("%s: warning: Model with multiple surfaces\n",mod->name);
	}

	for ( i = 0 ; i < pinmodel->numSurfaces ; i++) {

        LL(surf->ident);
        LL(surf->flags);
        LL(surf->numFrames);
        LL(surf->numShaders);
        LL(surf->numTriangles);
        LL(surf->ofsTriangles);
        LL(surf->numVerts);
        LL(surf->ofsShaders);
        LL(surf->ofsSt);
        LL(surf->ofsXyzNormals);
        LL(surf->ofsEnd);
	
#ifdef MD3DEBUG		
		Con_Printf("->surface %i\n",i);
		Con_Printf("  NumTriangles: %i\n",surf->numTriangles);
		Con_Printf("  NumVertices: %i\n",surf->numVerts);
		Con_Printf("  NumFrames: %i\n",surf->numFrames);
		Con_Printf("  NumShaders: %i\n",surf->numShaders);
#endif
		if ( surf->numVerts > MAXALIASVERTS)
			Sys_Error ("LoadMd3Model: %s has too many vertices (%i)%i",mod->name,surf->numVerts,surf->numTriangles);

		if (surf->numTriangles <= 0)
			Sys_Error ("LoadMd3Model: %s has no triangles", mod->name);

		if (surf->numTriangles > MAXALIASTRIS)
			Sys_Error ("LoadMd3Model: %s has too many triangles",mod->name);

		// swap all the triangles
		tri = (md3Triangle_t *) ( (byte *)surf + surf->ofsTriangles );
		for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
			LL(tri->indexes[0]);
			LL(tri->indexes[1]);
			LL(tri->indexes[2]);
		}

		// swap all the ST
        st = (md3St_t *) ( (byte *)surf + surf->ofsSt );
        for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
            st->s = LittleFloat( st->s);
            st->t = LittleFloat( st->t);
        }

		// swap all the XyzNormals
        xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );
        for ( j = 0 ; j < surf->numVerts * surf->numFrames ; j++, xyz++ ) 
		{
            xyz->xyz[0] = LittleShort( xyz->xyz[0] );
            xyz->xyz[1] = LittleShort( xyz->xyz[1] );
            xyz->xyz[2] = LittleShort( xyz->xyz[2] );

            xyz->normal = LittleShort( xyz->normal );
        }


		// find the next surface
		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}

	//point to first surface again
	surf = (md3Surface_t *) ( (byte *)pinmodel + pinmodel->ofsSurfaces );

//
// We have now a working version of the md3 in the "*buffer" now convert that to an "alias" model
// this conversion is not to bad sice the I changed the way alias models work to make them more
// quake3 friendly, the only thing that remains is that we only use the first surface of the  model.
// - DC -
// added multiple surfaces.
// multiples surfaces : the cached data (alias3data_t) hold an aliashdr_t for each surface
//

	size = sizeof (alias3data_t);
	palias3 = Hunk_AllocName (size, mod->name);
	palias3->numSurfaces = pinmodel->numSurfaces;
        
	// allocate header offset array
	/* Didier, what's this?
	size = sizeof (aliashdr_t *) * (pinmodel->numSurfaces - 1);
	if (size)
		Hunk_Alloc (size);
	*/

	mod->flags = 0;
	mod->type = mod_alias;
	mod->numframes = pinmodel->numFrames;
	mod->synctype = ST_SYNC;
	
	mod->mins[0] = mod->mins[1] = mod->mins[2] =  99999.0;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = -99999.0; 
	
	for (surfcount = 0; surfcount < pinmodel->numSurfaces; ++surfcount) {
		
		//Alocate hunk mem for the header and the frame info (not the actual frame vertices)
		size = 	sizeof (aliashdr_t) + (pinmodel->numFrames-1) * sizeof (maliasframedesc_t);
		pheader = Hunk_Alloc (size);
		// store alias offset
		palias3->ofsSurfaces[surfcount] = (int)((char*)pheader - (char*)palias3);
		Q_memset(pheader,0,sizeof(aliashdr_t));
		
		//Convert the header to the old header
		pheader->ident = pinmodel->ident;
		pheader->version = pinmodel->version;
		VectorCopy(md3scale,pheader->scale);
		VectorCopy(md3origin,pheader->scale_origin);
		pheader->boundingradius = 100; //This seems not used anymore by quake
		VectorCopy(md3origin,pheader->eyeposition);//This seems not used anymore by quake
		pheader->numskins = 1; //Hacked value
		pheader->skinwidth = 4;//Hacked value
		pheader->skinheight = 4;//Hacked value
		pheader->numverts = surf->numVerts;
		pheader->numtris = surf->numTriangles;
		pheader->numframes = surf->numFrames;
		pheader->synctype = mod->synctype;
		pheader->flags = 0;//Hacked value
		pheader->size = 1;//All right, the unofficial specs say the average size of triangles, so we just put something there
		pheader->numposes = surf->numFrames;
		pheader->poseverts = surf->numVerts;
		
		//Allocate the vertices
		verts = Hunk_Alloc (pheader->numposes * pheader->poseverts * sizeof(ftrivertx_t) );
		pheader->posedata = (byte *)verts - (byte *)pheader;
		xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );
		
		//Convert the frames
		frame = (md3Frame_t *) ( (byte *)pinmodel + pinmodel->ofsFrames );
		for (i=0; i<pheader->numframes; i++, frame++) {
			strcpy (pheader->frames[i].name, frame->name);
			pheader->frames[i].firstpose = i;
			pheader->frames[i].numposes = 1;
			pheader->frames[i].frame = i;
			pheader->frames[i].interval = 0.1f;
			pheader->mins[0] = pheader->mins[1] = pheader->mins[2] =  99999.0;
			pheader->maxs[0] = pheader->maxs[1] = pheader->maxs[2] = -99999.0; 
			//Convert the vertices
			for (j=0; j<pheader->poseverts; j++) {
				k = i*pheader->poseverts+j;
				verts[k].v[0] = xyz[k].xyz[0]*MD3_XYZ_SCALE;
				verts[k].v[1] = xyz[k].xyz[1]*MD3_XYZ_SCALE;
				verts[k].v[2] = xyz[k].xyz[2]*MD3_XYZ_SCALE;
				verts[k].lightnormalindex = xyz[k].normal;
				//setup correct surface bounding box
				for (l=0; l<3; l++) {
					pheader->mins[l] = min(pheader->mins[l],verts[k].v[l]);
					pheader->maxs[l] = max(pheader->maxs[l],verts[k].v[l]);
				}
			}
			//setup correct model bounding box
			for (j=0; j<3; j++) {
				mod->mins[j] = min(mod->mins[j],pheader->mins[j]);
				mod->maxs[j] = max(mod->maxs[j],pheader->maxs[j]);
			}
			
		}
		//Con_Printf("%s: %f,%f,%f %f,%f,%f\n",loadname,mod->mins[0],mod->mins[1],mod->mins[2],mod->maxs[0],mod->maxs[1],mod->maxs[2]);
		
		//Convert the triangles
		tris = Hunk_Alloc (pheader->numtris * sizeof(mtriangle_t));
		pheader->triangles = (byte *)tris - (byte *)pheader;
		tri = (md3Triangle_t *) ( (byte *)surf + surf->ofsTriangles );
		for (i=0; i<pheader->numtris; i++) {
			for (j=0; j<3; j++) {
				tris[i].vertindex[j] = tri[i].indexes[j];
				tris[i].neighbours[j] = -1;
			}
			tris[i].facesfront = true; //doesn't matter
		}
		
		//Convert texcoords for triangles
		texcoords = Hunk_Alloc (pheader->poseverts * sizeof(fstvert_t));
		pheader->texcoords = (byte *)texcoords - (byte *)pheader;
		st = (md3St_t *) ( (byte *)surf + surf->ofsSt );
		for (i=0; i<pheader->poseverts ; i++) {
			texcoords[i].s = st[i].s;
			texcoords[i].t = st[i].t;
		}
		
		//Create index lists
		indecies = Hunk_Alloc (pheader->numtris * sizeof(int) * 3);
		pheader->indecies = (byte *)indecies - (byte *)pheader;
		for (i=0 ; i<pheader->numtris ; i++) {
			for (j=0 ; j<3 ; j++) {
				//Throw vertex index into our index array
				(*indecies) = tris[i].vertindex[j];
				indecies++;
			}
		}
		indecies = (int *)((byte *)pheader+pheader->indecies);
		
		SetupModelSurface(pheader);
		
		//Load skins
		for (i=0; i<16; i++)
			fake[i] = 254;
		
		shader = (md3Shader_t *) ( (byte *)surf + surf->ofsShaders );
		
		if (!shader->name[0]) {
			Q_strcpy(shader->name,"unnamed");
		}
		
		COM_StripExtension (shader->name, shadername);
		pheader->shader = GL_ShaderForName(shadername);
		
		
#ifdef MD3DEBUG
		Con_Printf("Load shader %s\n",shadername);
#endif
		// next surface
		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	} /* for numsurf */

	//calculate radius
	mod->radius = Mod_RadiusFromBounds (mod->mins, mod->maxs);


/* monster or player models only ? */
/* tags */
/*
tag = (md3tag_t *)( (byte *)pinmodel + pinmodel->ofsTags );

  for (i = 0; i< pinmodel->numTags; ++i){
  
	Con_Printf("Tag %s\n",tag[i].name);
	
	  // swap everything first 
	  for ( j = 0 ; j < 3 ; j++ ) {
	  tag[i].origin[j] = LittleFloat( tag[i].origin[j] );
	  tag[i].axis[0][j] = LittleFloat( tag[i].axis[0][j] );
	  tag[i].axis[1][j] = LittleFloat( tag[i].axis[1][j] );
	  tag[i].axis[2][j] = LittleFloat( tag[i].axis[2][j] );
	  }
	  // then look for supported tags
	  // weapon tag ?
	  if (!strcmp(tag[i].name,"tag_weapon")){
	  // for weapon, we only need origin, as the weapon 
	  // follows the player look
	  VectorCopy(palias3->weaponTag.origin,tag[i].origin);
	  }
	  }
*/

	if (!strcmp (mod->name, "progs/g_shot.mdl") || //Hack to give .md3 files renamed to .mdl rotate effects - Eradicator
		!strcmp (mod->name, "progs/g_nail.mdl") ||
		!strcmp (mod->name, "progs/g_nail2.mdl") ||
		!strcmp (mod->name, "progs/g_rock.mdl") ||
		!strcmp (mod->name, "progs/g_rock2.mdl") || 
		!strcmp (mod->name, "progs/g_light.mdl") ||
		!strcmp (mod->name, "progs/armor.mdl") ||
		!strcmp (mod->name, "progs/backpack.mdl") ||
		!strcmp (mod->name, "progs/w_g_key.mdl") ||
		!strcmp (mod->name, "progs/w_s_key.mdl") ||
		!strcmp (mod->name, "progs/m_g_key.mdl") ||
		!strcmp (mod->name, "progs/m_s_key.mdl") ||
		!strcmp (mod->name, "progs/b_g_key.mdl") ||
		!strcmp (mod->name, "progs/b_s_key.mdl") ||
		!strcmp (mod->name, "progs/quaddama.mdl") ||
		!strcmp (mod->name, "progs/invisibl.mdl") ||
		!strcmp (mod->name, "progs/invulner.mdl") ||
		!strcmp (mod->name, "progs/jetpack.mdl") || 
		!strcmp (mod->name, "progs/cube.mdl") ||
		!strcmp (mod->name, "progs/suit.mdl") ||
		!strcmp (mod->name, "progs/boots.mdl") ||
		!strcmp (mod->name, "progs/end1.mdl") ||
		!strcmp (mod->name, "progs/end2.mdl") ||
		!strcmp (mod->name, "progs/end3.mdl") ||
		!strcmp (mod->name, "progs/end4.mdl")) {
		mod->flags |= EF_ROTATE; 
	}
	else if (!strcmp (mod->name, "progs/missile.mdl")) {
		mod->flags |= EF_ROCKET;
	}
	else if (!strcmp (mod->name, "progs/gib1.mdl") || //EF_GIB
			 !strcmp (mod->name, "progs/gib2.mdl") || 
			 !strcmp (mod->name, "progs/gib3.mdl") || 
			 !strcmp (mod->name, "progs/h_player.mdl") || 
			 !strcmp (mod->name, "progs/h_dog.mdl") || 
			 !strcmp (mod->name, "progs/h_mega.mdl") || 
			 !strcmp (mod->name, "progs/h_guard.mdl") || 
			 !strcmp (mod->name, "progs/h_wizard.mdl") || 
			 !strcmp (mod->name, "progs/h_knight.mdl") || 
			 !strcmp (mod->name, "progs/h_hellkn.mdl") || 
			 !strcmp (mod->name, "progs/h_zombie.mdl") || 
			 !strcmp (mod->name, "progs/h_shams.mdl") || 
			 !strcmp (mod->name, "progs/h_shal.mdl") || 
			 !strcmp (mod->name, "progs/h_ogre.mdl") ||
			 !strcmp (mod->name, "progs/armor.mdl") ||
			 !strcmp (mod->name, "progs/h_demon.mdl")) {
		mod->flags |= EF_GIB;
	}
	else if (!strcmp (mod->name, "progs/grenade.mdl")) {
		mod->flags |= EF_GRENADE;
	}	
	else if (!strcmp (mod->name, "progs/w_spike.mdl")) //EF_TRACER
	{
		mod->flags |= EF_TRACER;
	}
	else if (!strcmp (mod->name, "progs/k_spike.mdl")) //EF_TRACER2
	{
		mod->flags |= EF_TRACER2;
	}
	else if (!strcmp (mod->name, "progs/v_spike.mdl")) //EF_TRACER3
	{
		mod->flags |= EF_TRACER3;
	}
	else if (!strcmp (mod->name, "progs/zom_gib.mdl")) //EF_ZOMGIB
	{
		mod->flags |= EF_ZOMGIB;
	}


//
// move the complete, relocatable alias model to the cache
//	
	end = Hunk_LowMark ();
	total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
	return;
	memcpy (mod->cache.data, palias3, total);        



	Hunk_FreeToLowMark (start);

}

/**

	The tkmo format is very similar to md3 but hasn't got some of it's quirks
	(mainly related to 3d max exporting)

*/

static char *parseKParam(char *data, const char *keyword,int *result) {
	data = COM_Parse (data);
	
	if (strcmp(com_token, keyword)) {
		Sys_Error("Invalid TKMO, expected %s found %s\n",keyword, com_token);
	}
		
	data = COM_Parse (data);
	(*result) = atoi(com_token);
	return data;
}

static char *ParseVectorLen(char *data, float *dest, int len) {
	int i;
	for (i=0; i<len; i++) {
		data = COM_Parse(data);
		dest[i] = atof(com_token);
	}
	return data;
}

int NormalToLatLong( const vec3_t normal);

void Mod_LoadTenebraeKeyframeModel (model_t *mod, void *buffer)
{
	int					i, j, k, l;
	md3Header_t			*pinmodel;
	int					size;
	int					start, end, total;
	md3Surface_t		*surf;
	md3tag_t			*tag;
	vec3_t				tkmoscale = {1.0f, 	1.0f, 	1.0f};
	vec3_t				tkmoorigin = {0.0f, 0.0f, 0.0f};
	ftrivertx_t			*verts, *v;
	md3XyzNormal_t		*xyz;
	mtriangle_t			*tris;
	md3Triangle_t		*tri;
	md3St_t				*st;
	md3Frame_t			*frame;
	int					*indecies;
	md3Shader_t			*shader;
	fstvert_t			*texcoords;
	byte				fake[16];
	char				shadername[MAX_QPATH];
    alias3data_t			*palias3;
     int				surfcount;   
	char *data;
	vec3_t				temp;

	//---//

	start = Hunk_LowMark ();
	data = (char *)buffer;
	
	data = COM_Parse(data);
	if (strcmp(com_token, "TKMO")) Sys_Error("Invalid TKMO, expected TKMO found %s\n", com_token);


	mod->flags = 0;
	mod->type = mod_alias;
	data = parseKParam(data, "frames", &mod->numframes);
	mod->synctype = ST_SYNC;
	
	mod->mins[0] = mod->mins[1] = mod->mins[2] =  99999.0;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = -99999.0; 

	size = sizeof (alias3data_t);
	palias3 = Hunk_AllocName(size, mod->name);
	data = parseKParam(data, "materials", &palias3->numSurfaces);
	
	for (surfcount=0; surfcount<palias3->numSurfaces; surfcount++) {

		//Alocate hunk mem for the header and the frame info (not the actual frame vertices)
		size = 	sizeof (aliashdr_t) + (mod->numframes-1) * sizeof (maliasframedesc_t);
		pheader = Hunk_Alloc (size);
		palias3->ofsSurfaces[surfcount] = (int)((char*)pheader - (char*)palias3);
		Q_memset(pheader,0,sizeof(aliashdr_t));

		pheader->numframes = mod->numframes;
		//Con_Printf("Surface: %i\n", surfcount);
		pheader->numposes = pheader->numframes;

		// read the shader to use on this surface
		data = COM_Parse(data);
		if (strcmp(com_token, "material")) Sys_Error("Invalid TKMO, expected material found %s\n", com_token);
		data = COM_Parse(data);
		strcpy(shadername, com_token);
		
		//Fields that are filled without reading anything out of the file...
		//FIXME: a lot of those can probably be removed.
		pheader->ident = TEKEYFRAMEHEADER;
		pheader->version = 1;
		VectorCopy(tkmoscale,pheader->scale);
		VectorCopy(tkmoorigin,pheader->scale_origin);
		pheader->boundingradius = 100; //This seems not used anymore by quake
		VectorCopy(tkmoorigin,pheader->eyeposition);//This seems not used anymore by quake
		pheader->numskins = 1; //Hacked value
		pheader->skinwidth = 4;//Hacked value
		pheader->skinheight = 4;//Hacked value
		pheader->synctype = mod->synctype;
		pheader->flags = 0;//Hacked value
		pheader->size = 1;//All right, the unofficial specs say the average size of triangles, so we just put something there

	//Read the triangle indexes
		//Con_Printf("Indexes\n");
		data = parseKParam(data, "faces", &pheader->numtris);
		tris = Hunk_Alloc (pheader->numtris * sizeof(mtriangle_t));
		pheader->triangles = (byte *)tris - (byte *)pheader;
		for (i=0; i<pheader->numtris; i++) {
			data = ParseVectorLen(data, temp, 3);
			for (j=0; j<3; j++) {
				tris[i].vertindex[j] = (int)(temp[j]);
				tris[i].neighbours[j] = -1;
			}
			tris[i].facesfront = true; //doesn't matter
		}

	//Read the texture coords
		//Con_Printf("Stverts\n");
		data = parseKParam(data, "stvertices", &pheader->poseverts);
		pheader->numverts = pheader->poseverts;
		texcoords = Hunk_Alloc (pheader->poseverts * sizeof(fstvert_t));
		pheader->texcoords = (byte *)texcoords - (byte *)pheader;
		for (i=0; i<pheader->poseverts ; i++) {
			data = ParseVectorLen(data, temp, 2);
			texcoords[i].s = temp[0];
			texcoords[i].t = temp[1];
		}

	//Read in the frame vertices

		
		//Allocate the vertices
		verts = Hunk_Alloc (pheader->numposes * pheader->poseverts * sizeof(ftrivertx_t) );
		pheader->posedata = (byte *)verts - (byte *)pheader;
		Con_Printf("Frames: %i\n",pheader->numframes);
		for (i=0; i<pheader->numframes; i++) {
			
			//Con_Printf("Frame %i\n",i);

			//Parse start of frame & framename
			data = COM_Parse(data);
			if (strcmp(com_token, "frame")) Sys_Error("Invalid TKMO, expected frame found %s\n", com_token);
			data = COM_Parse(data);
			strcpy (pheader->frames[i].name, com_token);

			//Parse verts
			pheader->frames[i].firstpose = i;
			pheader->frames[i].numposes = 1;
			pheader->frames[i].frame = i;
			pheader->frames[i].interval = 0.1f;
			pheader->mins[0] = pheader->mins[1] = pheader->mins[2] =  99999.0;
			pheader->maxs[0] = pheader->maxs[1] = pheader->maxs[2] = -99999.0; 
			//Convert the vertices
			for (j=0; j<pheader->poseverts; j++) {
				vec3_t pos, normal;

				data = ParseVectorLen(data, pos, 3);
				data = ParseVectorLen(data, normal, 3);

				//Con_Printf("Normal: %f %f %f\n",normal[0],normal[1],normal[2]);

				k = i*pheader->poseverts+j;
				verts[k].v[0] = pos[0];
				verts[k].v[1] = pos[1];
				verts[k].v[2] = pos[2];
				verts[k].lightnormalindex = NormalToLatLong(normal);
				
				//setup correct surface bounding box
				for (l=0; l<3; l++) {
					pheader->mins[l] = min(pheader->mins[l],verts[k].v[l]);
					pheader->maxs[l] = max(pheader->maxs[l],verts[k].v[l]);
				}
			}

			//setup correct model bounding box
			for (j=0; j<3; j++) {
				mod->mins[j] = min(mod->mins[j],pheader->mins[j]);
				mod->maxs[j] = max(mod->maxs[j],pheader->maxs[j]);
			}
			
		}
		
		//Create index lists
		indecies = Hunk_Alloc (pheader->numtris * sizeof(int) * 3);
		pheader->indecies = (byte *)indecies - (byte *)pheader;
		for (i=0 ; i<pheader->numtris ; i++) {
			for (j=0 ; j<3 ; j++) {
				//Throw vertex index into our index array
				(*indecies) = tris[i].vertindex[j];
				indecies++;
			}
		}
		indecies = (int *)((byte *)pheader+pheader->indecies);
		
		SetupModelSurface(pheader);

		if (!shadername[0]) {
			Q_strcpy(shadername,"unnamed");
		}
		
		pheader->shader = GL_ShaderForName(shadername);
	} /* for numsurf */

	//calculate radius
	mod->radius = Mod_RadiusFromBounds (mod->mins, mod->maxs);

//
// move the complete, relocatable alias model to the cache
//	
	end = Hunk_LowMark ();
	total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
	return;
	memcpy (mod->cache.data, palias3, total);        



	Hunk_FreeToLowMark (start);

}
