/**
 * @file routing.c
 * @brief
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


#include "qbsp.h"

/* some essential definitions */
#define WIDTH	256
#define HEIGHT	8

#define QUANT	4
#define PH		(UH-16)
#define SH_BIG	9
#define SH_LOW	2

const vec3_t dup = {0, 0, PH-UH/2};
const vec3_t dwn = {0, 0, -UH/2};

const vec3_t move_vec[4] = { {US, 0, 0}, {-US, 0, 0}, {0, US, 0}, {0, -US, 0} };
const vec3_t testvec[5] = { {-US/2+5,-US/2+5,0}, {US/2-5,US/2-5,0}, {-US/2+5,US/2-5,0}, {US/2-5,-US/2+5,0}, {0,0,0} };

/* data structures */
byte	area[HEIGHT][WIDTH][WIDTH];
byte	fall[WIDTH][WIDTH];
byte	step[WIDTH][WIDTH];
byte	filled[WIDTH][WIDTH];


ipos3_t	wpMins, wpMaxs;


/*
============
CheckUnit_Thread
============
*/
void CheckUnit_Thread (int unitnum)
{
	int		x, y, z;
	int		i;
	pos3_t	pos;
	vec3_t	start, end;
	vec3_t	tr_end;
	vec3_t	tvs, tve;
	float	height;

	/* get coordinates of that unit */
	z = unitnum / WIDTH / WIDTH;
	y = (unitnum / WIDTH) % WIDTH;
	x = unitnum % WIDTH;

	/* test bounds */
	if ( x > wpMaxs[0] || y > wpMaxs[1] || z > wpMaxs[2]
			|| x < wpMins[0] || y < wpMins[1] || z < wpMins[2] ) {
		/* don't enter */
		fall[y][x] |= 1 << z;
		return;
	}

	/* calculate tracing coordinates */
	pos[0] = x; pos[1] = y; pos[2] = z;
	PosToVec( pos, end );

	/* step height check */
	if ( TestContents( end ) )
		step[y][x] |= 1 << z;

	/* prepare fall down check */
	VectorCopy( end, start );
	start[2] -= UH/2-4;
	end[2]   -= UH/2+4;

	/* test for fall down */
	if ( TestLineMask( start, end, 2 ) ) {
		PosToVec( pos, end );
		VectorAdd( end, dup, start );
		VectorAdd( end, dwn, end );
		height = 0;

		/* test for ground with a "middled" height */
		for ( i = 0; i < 5; i++ ) {
			VectorAdd( start, testvec[i], tvs );
			VectorAdd( end, testvec[i], tve );
			TestLineDM( tvs, tve, tr_end, 2 );
			height += tr_end[2];

			/* stop if it's totally blocked somewhere */
			/* and try a higher starting point */
			if ( VectorCompare( tvs, tr_end ) )
				break;
		}

		/* tr_end[0] & [1] are correct (testvec[4]) */
		height += tr_end[2];
		tr_end[2] = height / 6.0;

		if ( i == 5 && !VectorCompare( start, tr_end ) ) {
			/* found a possibly valid ground */
			height = PH - (start[2]-tr_end[2]);
			end[2] = start[2] + height;

			if ( !TestLineDM( start, end, tr_end, 2 ) )
				area[z][y][x] = ((height+QUANT/2)/QUANT < 0) ? 0 : (height+QUANT/2)/QUANT;
			else
				filled[y][x] |= 1 << z; /* don't enter */
		} else {
/*			printf( "." ); */
			/* elevated a lot */
			end[2] = start[2];
			start[2] += UH-PH;
			height = 0;

			/* test for ground with a "middled" height */
			for ( i = 0; i < 5; i++ ) {
				VectorAdd( start, testvec[i], tvs );
				VectorAdd( end, testvec[i], tve );
				TestLineDM( tvs, tve, tr_end, 2 );
				height += tr_end[2];
			}
			/* tr_end[0] & [1] are correct (testvec[4]) */
			height += tr_end[2];
			tr_end[2] = height / 6.0;

			if ( VectorCompare( start, tr_end ) ) {
				filled[y][x] |= 1<<z; /* don't enter */
			} else {
				/* found a possibly valid elevated ground */
				end[2] = start[2] + PH - (start[2]-tr_end[2]);
				height = UH - (start[2]-tr_end[2]);

/*				printf( "%i %i\n", (int)height, (int)(start[2]-tr_end[2]) ); */

				if ( !TestLineDM( start, end, tr_end, 2 ) )
					area[z][y][x] = ((height+QUANT/2)/QUANT < 0) ? 0 : (height+QUANT/2)/QUANT;
				else
					filled[y][x] |= 1 << z; /* don't enter */
			}
		}
	} else {
		/* fall down */
		area[z][y][x] = 0;
		fall[y][x] |= 1 << z;
	}
}


/*
============
CheckConnections_Thread
============
*/
void CheckConnections_Thread (int unitnum)
{
	int		x, y, z, sz;
	int		i, h, sh;
	pos3_t	pos;
	vec3_t	start, ts, te;

	/* get coordinates of that unit */
	z = unitnum / WIDTH / WIDTH;
	y = (unitnum / WIDTH) % WIDTH;
	x = unitnum % WIDTH;

	/* totally blocked unit */
	if ( filled[y][x] & (1<<z) )
		return;

	h = (area[z][y][x] & 0xF);

	/* prepare trace */
	pos[0] = x; pos[1] = y; pos[2] = z;
	PosToVec( pos, start );
	start[2] += h * QUANT;
	VectorCopy( start, ts );

	sh = (step[y][x] & (1<<z)) ? SH_BIG : SH_LOW;
	sz = z + (h + sh) / 0x10;
	h = (h + sh) % 0x10;

	/* range check */
	if ( sz >= 8 ) {
		sz = 7; h = 0x0F;
	}

	/* test connections */
	for ( i = 0; i < 4; i++ ) {
		/* range check and test height */
		if ( i == 0 && (x >= 255 || (filled[y][x+1] & (1<<sz)) || (area[sz][y][x+1]&0x0F) > h) ) continue;
		if ( i == 1 && (x <=   0 || (filled[y][x-1] & (1<<sz)) || (area[sz][y][x-1]&0x0F) > h) ) continue;
		if ( i == 2 && (y >= 255 || (filled[y+1][x] & (1<<sz)) || (area[sz][y+1][x]&0x0F) > h) ) continue;
		if ( i == 3 && (y <=   0 || (filled[y-1][x] & (1<<sz)) || (area[sz][y-1][x]&0x0F) > h) ) continue;

		/* test for walls */
		VectorAdd( start, move_vec[i], te );

		/* center check */
		if ( TestLineMask( start, te, 2 ) ) continue;

		/* lower check */
		ts[2] = te[2] -= UH/2 - sh*QUANT - 2;
		if ( TestLineMask( ts, te, 2 ) ) continue;

		area[z][y][x] |= 1 << (i+4);
	}
}


/*
============
DoRouting

============
*/
void DoRouting(void)
{
	int		x, y, i;
	byte	*data;

	PushInfo();
	nummodels += 1;

	/* process playerclip-level */
/*	ProcessLevel(256); */
	/* process stepon-level */
	ProcessLevel(258);

	/* build tracing structure */
/*	EmitBrushes(); */
	EmitPlanes();
	MakeTnodes(259);

	PopInfo();
	nummodels -= 1;

	/* reset */
	for ( y = 0; y < WIDTH; y++ )
		for ( x = 0; x < WIDTH; x++ ) {
			fall[y][x] = 0;
			filled[y][x] = 0;
		}

	/* get world bounds for optimizing */
	VecToPos( worldMins, wpMins );
	VectorSubtract( wpMins, v_epsilon, wpMins );

	VecToPos( worldMaxs, wpMaxs );
	VectorAdd( wpMaxs, v_epsilon, wpMaxs );

	for ( i = 0; i < 3; i++ ) {
		if ( wpMins[i] <   0 ) wpMins[i] = 0;
		if ( wpMaxs[i] > 255 ) wpMaxs[i] = 255;
	}

/*	printf( "(%i %i %i) (%i %i %i)\n", wpMins[0], wpMins[1], wpMins[2], wpMaxs[0], wpMaxs[1], wpMaxs[2] ); */

	/* scan area heights */
	RunThreadsOnIndividual ( HEIGHT*WIDTH*WIDTH, !verbose, CheckUnit_Thread);

	/* scan connections */
	RunThreadsOnIndividual ( HEIGHT*WIDTH*WIDTH, !verbose, CheckConnections_Thread);

	/* store the data */
	data = droutedata;
	*data++ = SH_LOW;
	*data++ = SH_BIG;
	data = CompressRouting( &(area[0][0][0]), data, 256*256*8 );
	data = CompressRouting( &(fall[0][0]), data, 256*256 );
	data = CompressRouting( &(step[0][0]), data, 256*256 );

	routedatasize = data - droutedata;
}
