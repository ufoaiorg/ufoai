/*
==============================================================

CMODEL

==============================================================
*/
#include "../qcommon/qfiles.h"

#define MAX_MAPTILES	512

extern vec3_t map_min, map_max;

void CM_LoadMap(char *tiles, char *pos);
int CheckBSPFile(char *filename);
cmodel_t *CM_InlineModel(char *name);	/* *0, *1, *2, etc */

int CM_NumClusters(void);
int CM_NumInlineModels(void);
char *CM_EntityString(void);

/*
==============================================================

CMODEL BOX TRACING

==============================================================
*/

/* WARNING: The functions, that are commented out, */
/* possibly don't give the expected results, because of */
/* the new map tiles, all the others should work */

/* creates a clipping hull for an arbitrary box */
int CM_HeadnodeForBox(int tile, vec3_t mins, vec3_t maxs);


/* returns an ORed contents mask */
/*int			CM_PointContents (vec3_t p, int headnode); */
/*int			CM_TransformedPointContents (vec3_t p, int headnode, vec3_t origin, vec3_t angles); */

trace_t CM_BoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int tile, int headnode, int brushmask);
trace_t CM_TransformedBoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int tile, int headnode, int brushmask, vec3_t origin, vec3_t angles);
trace_t CM_CompleteBoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int levelmask, int brushmask);

int CM_EntTestLine(vec3_t start, vec3_t stop);
int CM_EntTestLineDM(vec3_t start, vec3_t stop, vec3_t end);
int CM_TestLine(vec3_t start, vec3_t stop);
int CM_TestLineDM(vec3_t start, vec3_t stop, vec3_t end);


/*
==========================================================

  GRID ORIENTED MOVEMENT AND SCANNING

==========================================================
*/

extern struct routing_s svMap, clMap;

void Grid_RecalcRouting(struct routing_s *map, char *name, char **list);
void Grid_MoveCalc(struct routing_s *map, pos3_t from, int distance, byte ** fb_list, int fb_length);
void Grid_MoveStore(struct routing_s *map);
int Grid_MoveLength(struct routing_s *map, pos3_t to, qboolean stored);
int Grid_MoveNext(struct routing_s *map, pos3_t from);
int Grid_Height(struct routing_s *map, pos3_t pos);
int Grid_Fall(struct routing_s *map, pos3_t pos);
void Grid_PosToVec(struct routing_s *map, pos3_t pos, vec3_t vec);


/*
==========================================================

  MISC WORLD RELATED

==========================================================
*/

float Com_GrenadeTarget(vec3_t from, vec3_t at, float speed, vec3_t v0);


