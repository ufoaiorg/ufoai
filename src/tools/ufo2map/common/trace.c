// trace.c

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include <stddef.h>

#define	ON_EPSILON	0.1

typedef struct tnode_s
{
	int		type;
	vec3_t	normal;
	float	dist;
	int		children[2];
	int		pad;
} tnode_t;

tnode_t		*tnodes, *tnode_p;

int		numtheads;
int		thead[260];
int		theadlevel[260];

int		neededContents = (CONTENTS_SOLID | CONTENTS_STEPON | CONTENTS_PLAYERCLIP);
int		forbiddenContents = (CONTENTS_PASSABLE);
vec3_t	tr_end;

/*
==============
MakeTnode

Converts the disk node structure into the efficient tracing structure
==============
*/
void MakeTnode (int nodenum)
{
	tnode_t			*t;
	dplane_t		*plane;
	int				i, contents;
	dnode_t 		*node;

	t = tnode_p++;

	node = dnodes + nodenum;
	plane = dplanes + node->planenum;

	t->type = plane->type;
	VectorCopy (plane->normal, t->normal);
	t->dist = plane->dist;

	for (i=0 ; i<2 ; i++)
	{
		if (node->children[i] < 0)
		{
			contents = dleafs[-node->children[i] - 1].contents & ~(1<<31);
			if ( (contents & neededContents) && !(contents & forbiddenContents) )
				t->children[i] = 1 | (1<<31); //-node->children[i] | (1<<31); // leaf+1
			else
				t->children[i] = (1<<31);
		}
		else
		{
			t->children[i] = tnode_p - tnodes;
			MakeTnode (node->children[i]);
		}
	}

}


/*
=============
BuildTnode_r
=============
*/
void BuildTnode_r( int node )
{
	if ( dnodes[node].planenum == -1 )
	{
		dnode_t *n;
		tnode_t *t;
		vec3_t	c0maxs, c1mins;
		int		i;

		n = &dnodes[node];

		// alloc new node
		t = tnode_p++;

		if ( n->children[0] < 0 || n->children[1] < 0 )
			Error( "Unexpected leaf" );

		VectorCopy( dnodes[n->children[0]].maxs, c0maxs );
		VectorCopy( dnodes[n->children[1]].mins, c1mins );

		//	printf( "(%i %i : %i %i) (%i %i : %i %i)\n",
		//		(int)dnodes[n->children[0]].mins[0], (int)dnodes[n->children[0]].mins[1], (int)dnodes[n->children[0]].maxs[0], (int)dnodes[n->children[0]].maxs[1],
		//		(int)dnodes[n->children[1]].mins[0], (int)dnodes[n->children[1]].mins[1], (int)dnodes[n->children[1]].maxs[0], (int)dnodes[n->children[1]].maxs[1] );

		for ( i = 0; i < 2; i++ )
			if ( c0maxs[i] <= c1mins[i] )
			{
				// create a separation plane
				t->type = i;
				t->normal[0] = i;
				t->normal[1] = i^1;
				t->normal[2] = 0;
				t->dist = (c0maxs[i] + c1mins[i]) / 2;

				t->children[1] = tnode_p - tnodes;
				BuildTnode_r( n->children[0] );
				t->children[0] = tnode_p - tnodes;
				BuildTnode_r( n->children[1] );
				return;
			}

		// can't construct such a separation plane
		t->type = PLANE_NONE;

		for ( i = 0; i < 2; i++ )
		{
			t->children[i] = tnode_p - tnodes;
			BuildTnode_r( n->children[i] );
		}

	}
	else
	{
		MakeTnode( node );
	}
}


/*
=============
MakeTnodes

Loads the node structure out of a .bsp file to be used for light occlusion
=============
*/
void MakeTnodes ( int levels )
{
	int		i;

	// 32 byte align the structs
	tnodes = malloc( (numnodes+1) * sizeof(tnode_t));
#ifndef __x86_64__
	tnodes = (tnode_t *)(((int)tnodes + 31)&~31);
#else
	tnodes = (tnode_t *)(((ptrdiff_t)tnodes + 31)&~31);
#endif
	tnode_p = tnodes;
	numtheads = 0;

	for ( i = 0; i < levels; i++ )
	{
		if ( !dmodels[i].numfaces )
			continue;

		thead[numtheads] = tnode_p - tnodes;
		theadlevel[numtheads] = i;
		numtheads++;
		BuildTnode_r( dmodels[i].headnode );
	}
}


//==========================================================


int TestLine_r (int node, vec3_t start, vec3_t stop)
{
	tnode_t	*tnode;
	float	front, back;
	vec3_t	mid;
	float	frac;
	int		side;
	int		r;

	if (node & (1<<31))
		return node & ~(1<<31);	// leaf node

	tnode = &tnodes[node];
	switch (tnode->type)
	{
	case PLANE_X:
		front = start[0] - tnode->dist;
		back = stop[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = start[1] - tnode->dist;
		back = stop[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = start[2] - tnode->dist;
		back = stop[2] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TestLine_r (tnode->children[0], start, stop);
		if (r)
			return r;
		return TestLine_r (tnode->children[1], start, stop);
		break;
	default:
		front = (start[0]*tnode->normal[0] + start[1]*tnode->normal[1] + start[2]*tnode->normal[2]) - tnode->dist;
		back = (stop[0]*tnode->normal[0] + stop[1]*tnode->normal[1] + stop[2]*tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TestLine_r (tnode->children[0], start, stop);

	if (front < ON_EPSILON && back < ON_EPSILON)
		return TestLine_r (tnode->children[1], start, stop);

	side = front < 0;

	frac = front / (front-back);

	mid[0] = start[0] + (stop[0] - start[0])*frac;
	mid[1] = start[1] + (stop[1] - start[1])*frac;
	mid[2] = start[2] + (stop[2] - start[2])*frac;

	r = TestLine_r (tnode->children[side], start, mid);
	if (r)
		return r;
	return TestLine_r (tnode->children[!side], mid, stop);
}


int TestLineDist_r (int node, vec3_t start, vec3_t stop)
{
	tnode_t	*tnode;
	float	front, back;
	vec3_t	mid;
	float	frac;
	int		side;
	int		r;

	if (node & (1<<31))
	{
		r = node & ~(1<<31);
		if ( r ) VectorCopy( start, tr_end );
		return r;	// leaf node
	}

	tnode = &tnodes[node];
	switch (tnode->type)
	{
	case PLANE_X:
		front = start[0] - tnode->dist;
		back = stop[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = start[1] - tnode->dist;
		back = stop[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = start[2] - tnode->dist;
		back = stop[2] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TestLineDist_r (tnode->children[0], start, stop);
		if ( r ) VectorCopy( tr_end, mid );
		side = TestLineDist_r (tnode->children[1], start, stop);
		if ( side && r )
		{
			if ( VectorNearer( mid, tr_end, start ) )
			{
				VectorCopy( mid, tr_end );
				return r;
			}
			else return side;
		}
		if ( r )
		{
			VectorCopy( mid, tr_end );
			return r;
		}

		return side;

		break;
	default:
		front = (start[0]*tnode->normal[0] + start[1]*tnode->normal[1] + start[2]*tnode->normal[2]) - tnode->dist;
		back = (stop[0]*tnode->normal[0] + stop[1]*tnode->normal[1] + stop[2]*tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TestLineDist_r (tnode->children[0], start, stop);

	if (front < ON_EPSILON && back < ON_EPSILON)
		return TestLineDist_r (tnode->children[1], start, stop);

	side = front < 0;

	frac = front / (front-back);

	mid[0] = start[0] + (stop[0] - start[0])*frac;
	mid[1] = start[1] + (stop[1] - start[1])*frac;
	mid[2] = start[2] + (stop[2] - start[2])*frac;

	r = TestLineDist_r (tnode->children[side], start, mid);
	if (r)
		return r;
	return TestLineDist_r (tnode->children[!side], mid, stop);
}


int TestLine (vec3_t start, vec3_t stop)
{
	int		i;

	for ( i = 0; i < numtheads; i++ )
	{
		if ( TestLine_r( thead[i], start, stop ) )
			return 1;

	}
	return 0;
}

int TestLineMask (vec3_t start, vec3_t stop, int levels)
{
	int		i;

	for ( i = 0; i < numtheads; i++ )
	{
		if ( theadlevel[i] > 255 + levels )
			continue;

		if ( TestLine_r( thead[i], start, stop ) )
			return 1;

	}
	return 0;
}

int TestLineDM (vec3_t start, vec3_t stop, vec3_t end, int levels)
{
	int		i;

	VectorCopy( stop, end );

	for ( i = 0; i < numtheads; i++ )
	{
		if ( theadlevel[i] > 255 + levels )
			continue;

		if ( TestLineDist_r( thead[i], start, stop ) )
			if ( VectorNearer( tr_end, end, start ) )
				VectorCopy( tr_end, end );
	}

	if ( VectorCompare( end, stop ) ) return 0;
	else return 1;
}



/*
==============
TestContents_r
==============
*/
int TestContents_r (int node, vec3_t pos)
{
	tnode_t	*tnode;
	float	front;
	int		r;

	if (node & (1<<31))
		return node & ~(1<<31);	// leaf node

	tnode = &tnodes[node];
	switch (tnode->type)
	{
	case PLANE_X:
		front = pos[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = pos[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = pos[2] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TestContents_r (tnode->children[0], pos);
		if (r) return r;
		return TestContents_r (tnode->children[1], pos);
		break;
	default:
		front = (pos[0]*tnode->normal[0] + pos[1]*tnode->normal[1] + pos[2]*tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= 0)
		return TestContents_r (tnode->children[0], pos);
	else
		return TestContents_r (tnode->children[1], pos);
}


int TestContents (vec3_t pos)
{
	int		i;

	for ( i = numtheads-1; i >= 0; i-- )
	{
		if ( theadlevel[i] != 258 )
			continue;

		if ( TestContents_r( thead[i], pos ) )
			return 1;

		break;
	}
	return 0;
}

