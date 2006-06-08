
#include "common/cmdlib.h"
#include "common/mathlib.h"
#include "common/scriplib.h"
#include "common/polylib.h"
#include "common/threads.h"
#include "common/bspfile.h"

#define	MAX_BRUSH_SIDES	128
#define	CLIP_EPSILON	0.1

#define	BOGUS_RANGE	8192

#define	TEXINFO_NODE		-1		/* side is allready on a node */


#define UNIT_SIZE			32
#define UNIT_HEIGHT			64
#define US					UNIT_SIZE
#define UH					UNIT_HEIGHT

typedef byte pos_t;
typedef pos_t pos3_t[3];

typedef int ipos_t;
typedef ipos_t ipos3_t[3];

#define VecToPos(v,p)		(p[0]=(((int)v[0]+4096)/US), p[1]=(((int)v[1]+4096)/US), p[2]=((int)v[2]/UH))
#define PosToVec(p,v)		(v[0]=((int)p[0]-128)*US+US/2, v[1]=((int)p[1]-128)*US+US/2, v[2]=(int)p[2]*UH+UH/2)


typedef struct plane_s
{
	vec3_t	normal;
	vec_t	dist;
	int		type;
	struct plane_s	*hash_chain;
} plane_t;

typedef struct
{
	vec_t	shift[2];
	vec_t	rotate;
	vec_t	scale[2];
	char	name[32];
	int		flags;
	int		value;
} brush_texture_t;

typedef struct side_s
{
	int			planenum;
	int			texinfo;
	winding_t	*winding;
	struct side_s	*original;	/* bspbrush_t sides will reference the mapbrush_t sides */
	int			contents;		/* from miptex */
	int			surf;			/* from miptex */
	qboolean	visible;		/* choose visible planes first */
	qboolean	tested;			/* this plane allready checked as a split */
	qboolean	bevel;			/* don't ever use for bsp splitting */
} side_t;

typedef struct brush_s
{
	int		entitynum;
	int		brushnum;

	int		contents;

	vec3_t	mins, maxs;

	int		numsides;
	side_t	*original_sides;

	qboolean finished;
} mapbrush_t;

#define	PLANENUM_LEAF			-1

#define	MAXEDGES		20

typedef struct face_s
{
	struct face_s	*next;		/* on node */

	/* the chain of faces off of a node can be merged or split, */
	/* but each face_t along the way will remain in the chain */
	/* until the entire tree is freed */
	struct face_s	*merged;	/* if set, this face isn't valid anymore */
	struct face_s	*split[2];	/* if set, this face isn't valid anymore */

	struct portal_s	*portal;
	int				texinfo;
	int				planenum;
	int				contents;	/* faces in different contents can't merge */
	int				outputnumber;
	winding_t		*w;
	int				numpoints;
	qboolean		badstartvert;	/* tjunctions cannot be fixed without a midpoint vertex */
	int				vertexnums[MAXEDGES];
} face_t;



typedef struct bspbrush_s
{
	struct bspbrush_s	*next;
	vec3_t	mins, maxs;
	int		side, testside;		/* side of node during construction */
	mapbrush_t	*original;
	int		numsides;
	side_t	sides[6];			/* variably sized */
} bspbrush_t;



#define	MAX_NODE_BRUSHES	8
typedef struct node_s
{
	/* both leafs and nodes */
	int				planenum;	/* -1 = leaf node */
	struct node_s	*parent;
	vec3_t			mins, maxs;	/* valid after portalization */
	bspbrush_t		*volume;	/* one for each leaf/node */

	/* nodes only */
	qboolean		detail_seperator;	/* a detail brush caused the split */
	side_t			*side;		/* the side that created the node */
	struct node_s	*children[2];
	face_t			*faces;

	/* leafs only */
	bspbrush_t		*brushlist;	/* fragments of all brushes in this leaf */
	int				contents;	/* OR of all brush contents */
	int				occupied;	/* 1 or greater can reach entity */
	entity_t		*occupant;	/* for leak file testing */
	int				cluster;	/* for portalfile writing */
	int				area;		/* for areaportals */
	struct portal_s	*portals;	/* also on nodes during construction */
} node_t;

typedef struct portal_s
{
	plane_t		plane;
	node_t		*onnode;		/* NULL = outside box */
	node_t		*nodes[2];		/* [0] = front side of plane */
	struct portal_s	*next[2];
	winding_t	*winding;

	qboolean	sidefound;		/* false if ->side hasn't been checked */
	side_t		*side;			/* NULL = non-visible */
	face_t		*face[2];		/* output face in bsp file */
} portal_t;

typedef struct
{
	node_t		*headnode;
	node_t		outside_node;
	vec3_t		mins, maxs;
} tree_t;

extern	int			entity_num;

extern	plane_t		mapplanes[MAX_MAP_PLANES];
extern	int			nummapplanes;

extern	int			nummapbrushes;
extern	mapbrush_t	mapbrushes[MAX_MAP_BRUSHES];

extern	vec3_t		map_mins, map_maxs;

#define	MAX_MAP_SIDES		(MAX_MAP_BRUSHES*6)

extern	int			nummapbrushsides;
extern	side_t		brushsides[MAX_MAP_SIDES];

extern	qboolean	noprune;
extern	qboolean	nodetail;
extern	qboolean	fulldetail;
extern	qboolean	nomerge;
extern	qboolean	nosubdiv;
extern	qboolean	nowater;
extern	qboolean	noweld;
extern	qboolean	noshare;
extern	qboolean	notjunc;
extern	qboolean	nocsg;
extern	qboolean	nobackclip;

extern	int			block_xl, block_xh, block_yl, block_yh;
extern	vec3_t		v_epsilon;
extern	int			brush_start, brush_end;

extern	vec_t		microvolume;

extern	char		outbase[32];

extern	char	source[1024];

void 	LoadMapFile (char *filename);
int		FindFloatPlane (vec3_t normal, vec_t dist);

/*============================================================================= */

/* textures.c */

typedef struct
{
	char	name[64];
	int		flags;
	int		value;
	int		contents;
	char	animname[64];
} textureref_t;

#define	MAX_MAP_TEXTURES	1024

extern	textureref_t	textureref[MAX_MAP_TEXTURES];

int	FindMiptex (char *name);

int TexinfoForBrushTexture (plane_t *plane, brush_texture_t *bt, vec3_t origin);

/*============================================================================= */

void FindGCD (int *v);

mapbrush_t *Brush_LoadEntity (entity_t *ent);
int	PlaneTypeForNormal (vec3_t normal);
qboolean MakeBrushPlanes (mapbrush_t *b);
int		FindIntPlane (int *inormal, int *iorigin);
void	CreateBrush (int brushnum);




/*============================================================================= */

/* draw.c */

extern vec3_t	draw_mins, draw_maxs;
extern	qboolean	drawflag;

void Draw_ClearWindow (void);
void DrawWinding (winding_t *w);

void GLS_BeginScene (void);
void GLS_Winding (winding_t *w, int code);
void GLS_EndScene (void);

/*============================================================================= */

/* csg */

qboolean IsInLevel( int contents, int level );
int MapBrushesBounds( int startbrush, int endbrush, int level, vec3_t clipmins, vec3_t clipmaxs, 
					  vec3_t mins, vec3_t maxs );
bspbrush_t *MakeBspBrushList (int startbrush, int endbrush, int level,
		vec3_t clipmins, vec3_t clipmaxs);
bspbrush_t *ChopBrushes (bspbrush_t *head);
bspbrush_t *InitialBrushList (bspbrush_t *list);
bspbrush_t *OptimizedBrushList (bspbrush_t *list);

void WriteBrushMap (char *name, bspbrush_t *list);

/*============================================================================= */

/* brushbsp */

void WriteBrushList (char *name, bspbrush_t *brush, qboolean onlyvis);

bspbrush_t *CopyBrush (bspbrush_t *brush);

void SplitBrush (bspbrush_t *brush, int planenum,
	bspbrush_t **front, bspbrush_t **back);

tree_t *AllocTree (void);
node_t *AllocNode (void);
bspbrush_t *AllocBrush (int numsides);
int	CountBrushList (bspbrush_t *brushes);
void FreeBrush (bspbrush_t *brushes);
vec_t BrushVolume (bspbrush_t *brush);

void BoundBrush (bspbrush_t *brush);
void FreeBrushList (bspbrush_t *brushes);

tree_t *BrushBSP (bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);

/*============================================================================= */

/* portals.c */

int VisibleContents (int contents);

void MakeHeadnodePortals (tree_t *tree);
void MakeNodePortal (node_t *node);
void SplitNodePortals (node_t *node);

qboolean	Portal_VisFlood (portal_t *p);

qboolean FloodEntities (tree_t *tree);
void FillOutside (node_t *headnode);
void FloodAreas (tree_t *tree);
void MarkVisibleSides (tree_t *tree, int start, int end);
void FreePortal (portal_t *p);
void EmitAreaPortals (node_t *headnode);

void MakeTreePortals (tree_t *tree);

/*============================================================================= */

/* glfile.c */

void OutputWinding (winding_t *w, FILE *glview);
void WriteGLView (tree_t *tree, char *source);

/*============================================================================= */

/* writebsp.c */

void SetModelNumbers (void);
void SetLightStyles (void);

void BeginBSPFile (void);
void WriteBSP (node_t *headnode);
void EndBSPFile (void);
void BeginModel (void);
void EndModel (void);
void EmitBrushes (void);
void EmitPlanes (void);

/*============================================================================= */

/* faces.c */

void MakeFaces (node_t *headnode);
void FixTjuncs (node_t *headnode);
int GetEdge2 (int v1, int v2,  face_t *f);

face_t	*AllocFace (void);
void FreeFace (face_t *f);

void MergeNodeFaces (node_t *node);

/*============================================================================= */

/* tree.c */

void FreeTree (tree_t *tree);
void FreeTree_r (node_t *node);
void PrintTree_r (node_t *node, int depth);
void FreeTreePortals_r (node_t *node);
void PruneNodes_r (node_t *node);
void PruneNodes (node_t *node);

/*============================================================================= */

/* trace.c */

void MakeTnodes ( int levels );
int TestLine (vec3_t start, vec3_t stop);
int TestLineMask (vec3_t start, vec3_t stop, int levels);
int TestLineDM (vec3_t start, vec3_t stop, vec3_t end, int levels);
int TestContents (vec3_t pos);

/*============================================================================= */

/* levels.c */

extern vec3_t	v_epsilon;
extern vec3_t	worldMins, worldMaxs;

void PushInfo( void );
void PopInfo( void );
void ProcessLevel( int levelnum );

/*============================================================================= */

/* routing.c */

void DoRouting( void );

extern qboolean	noprune;
extern qboolean	nodetail;
extern qboolean	fulldetail;
extern qboolean	onlyents;
extern qboolean	nomerge;
extern qboolean	nowater;
extern qboolean	nofill;
extern qboolean	nocsg;
extern qboolean	noweld;
extern qboolean	noshare;
extern qboolean	nosubdiv;
extern qboolean	notjunc;
extern qboolean	noopt;
extern qboolean	leaktest;
extern qboolean	verboseentities;
extern qboolean	norouting;
extern qboolean	nobackclip;
extern float subdivide_size;

/*============================================================================= */

/* qbsp3.c */

void ProcessModels (void);
void RadWorld (void);
