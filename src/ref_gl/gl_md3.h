/**
 * MD3 representation in memory
 * Vic code with a few my changes
 * -Harven
 */

typedef struct maliascoord_s
{
	vec2_t			st;
} maliascoord_t;

typedef struct maliasvertex_s
{
	vec3_t			point;
	vec3_t			normal;
} maliasvertex_t;

typedef struct
{
    vec3_t			mins, maxs;
    vec3_t			translate;
    float			radius;
} maliasframe_t;

typedef struct
{
	char			name[MD3_MAX_PATH];
	dorientation_t	orient;
} maliastag_t;

typedef struct 
{
	char			name[MD3_MAX_PATH];
	int				shader;
} maliasskin_t;

typedef struct
{
    int				num_verts;
	char			name[MD3_MAX_PATH];
	maliasvertex_t	*vertexes;
	maliascoord_t	*stcoords;

    int				num_tris;
    index_t			*indexes;
	int				*trneighbors;

    int				num_skins;
	maliasskin_t	*skins;
} maliasmesh_t;

typedef struct maliasmodel_s
{
    int				num_frames;
	maliasframe_t	*frames;

    int				num_tags;
	maliastag_t		*tags;

    int				num_meshes;
	maliasmesh_t	*meshes;

	int				num_skins;
	maliasskin_t	*skins;
} maliasmodel_t;

void R_DrawAliasMD3Model (entity_t *e);
