/**
 * @file routing.c
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


#include "bsp.h"

/* some essential definitions */
#define WIDTH	256
#define HEIGHT	8

#define QUANT	4
#define PLAYER_HEIGHT		(UNIT_HEIGHT - 16)
#define SH_BIG	9
#define SH_LOW	2

static const vec3_t dup_vec = {0, 0, PLAYER_HEIGHT-UNIT_HEIGHT/2};
static const vec3_t dwn_vec = {0, 0, -UNIT_HEIGHT/2};

static const vec3_t move_vec[4] = { {UNIT_SIZE, 0, 0}, {-UNIT_SIZE, 0, 0}, {0, UNIT_SIZE, 0}, {0, -UNIT_SIZE, 0} };
static const vec3_t testvec[5] = { {-UNIT_SIZE/2+5,-UNIT_SIZE/2+5,0}, {UNIT_SIZE/2-5,UNIT_SIZE/2-5,0}, {-UNIT_SIZE/2+5,UNIT_SIZE/2-5,0}, {UNIT_SIZE/2-5,-UNIT_SIZE/2+5,0}, {0,0,0} };

/** routing data structures */
byte	route[HEIGHT][WIDTH][WIDTH];
byte	fall[WIDTH][WIDTH];
byte	step[WIDTH][WIDTH];
byte	filled[WIDTH][WIDTH];	/**< totally blocked units */

/** @brief world min and max values converted from vec to pos */
static ipos3_t wpMins, wpMaxs;

/**
 * @sa DoRouting
 */
static void CheckUnit (unsigned int unitnum)
{
	int i, x, y, z;
	pos3_t pos;
	vec3_t start, end;
	vec3_t tr_end;
	vec3_t tvs, tve;
	float height;

	/* get coordinates of that unit */
	z = unitnum / WIDTH / WIDTH;
	y = (unitnum / WIDTH) % WIDTH;
	x = unitnum % WIDTH;

	/* test bounds */
	if (x > wpMaxs[0] || y > wpMaxs[1] || z > wpMaxs[2]
			|| x < wpMins[0] || y < wpMins[1] || z < wpMins[2]) {
		/* don't enter - outside world */
		fall[y][x] |= 1 << z;
		return;
	}

	/* calculate tracing coordinates */
	VectorSet(pos, x, y, z);
	PosToVec(pos, end);

	/* step height check */
	if (TestContents(end))
		step[y][x] |= 1 << z;

	/* prepare fall down check */
	VectorCopy(end, start);
	start[2] -= UNIT_HEIGHT / 2 - 4;
	end[2]   -= UNIT_HEIGHT / 2 + 4;

	/* FIXME: Don't allow falling over more than 1 level (z-direction) */
	/* test for fall down */
	if (TestLineMask(start, end, 2)) {
		PosToVec(pos, end);
		VectorAdd(end, dup_vec, start);
		VectorAdd(end, dwn_vec, end);
		height = 0;

		/* test for ground with a "middled" height */
		for (i = 0; i < 5; i++) {
			VectorAdd(start, testvec[i], tvs);
			VectorAdd(end, testvec[i], tve);
			TestLineDM(tvs, tve, tr_end, 2);
			height += tr_end[2];

			/* stop if it's totally blocked somewhere */
			/* and try a higher starting point */
			if (VectorCompareEps(tvs, tr_end, EQUAL_EPSILON))
				break;
		}

		/* tr_end[0] & [1] are correct (testvec[4]) */
		height += tr_end[2];
		tr_end[2] = height / 6.0;

		if (i == 5 && !VectorCompareEps(start, tr_end, EQUAL_EPSILON)) {
			/* found a possibly valid ground */
			height = PLAYER_HEIGHT - (start[2] - tr_end[2]);
			end[2] = start[2] + height;

			if (!TestLineDM(start, end, tr_end, 2))
				route[z][y][x] = ((height + QUANT / 2) / QUANT < 0) ? 0 : (height + QUANT / 2) / QUANT;
			else
				filled[y][x] |= 1 << z; /* don't enter */
		} else {
/*			Com_Printf("."); */
			/* elevated a lot */
			end[2] = start[2];
			start[2] += UNIT_HEIGHT - PLAYER_HEIGHT;
			height = 0;

			/* test for ground with a "middled" height */
			for (i = 0; i < 5; i++) {
				VectorAdd(start, testvec[i], tvs);
				VectorAdd(end, testvec[i], tve);
				TestLineDM(tvs, tve, tr_end, 2);
				height += tr_end[2];
			}
			/* tr_end[0] & [1] are correct (testvec[4]) */
			height += tr_end[2];
			tr_end[2] = height / 6.0;

			if (VectorCompareEps(start, tr_end, EQUAL_EPSILON)) {
				filled[y][x] |= 1 << z; /* don't enter */
			} else {
				/* found a possibly valid elevated ground */
				end[2] = start[2] + PLAYER_HEIGHT - (start[2]-tr_end[2]);
				height = UNIT_HEIGHT - (start[2]-tr_end[2]);

/*				Com_Printf("%i %i\n", (int)height, (int)(start[2]-tr_end[2])); */

				if (!TestLineDM(start, end, tr_end, 2))
					route[z][y][x] = ((height + QUANT / 2) / QUANT < 0) ? 0 : (height + QUANT / 2) / QUANT;
				else
					filled[y][x] |= 1 << z; /* don't enter */
			}
		}
	} else {
		/* fall down */
		route[z][y][x] = 0;
		fall[y][x] |= 1 << z;
	}
}


/**
 * @sa DoRouting
 */
static void CheckConnections (unsigned int unitnum)
{
	int x, y, z, sz, ax, ay;
	int i, h, sh;
	pos3_t pos;
	vec3_t start, ts, te;
	/* Falling deeper than one level or falling through the map is forbidden. This array */
	/* includes the critical bit mask for every level: the bit for the current level and */
	/* the level below, for the lowest level falling is completely forbidden */
	const byte deep_fall[] = {0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0};

	/* get coordinates of that unit */
	z = unitnum / WIDTH / WIDTH;
	y = (unitnum / WIDTH) % WIDTH;
	x = unitnum % WIDTH;

	assert(z < HEIGHT);
	assert(y < WIDTH);
	assert(x < WIDTH);

	/* totally blocked unit */
	if (filled[y][x] & (1 << z))
		return;

	h = (route[z][y][x] & 0xF);

	/* prepare trace */
	VectorSet(pos, x, y, z);
	PosToVec(pos, start);
	start[2] += h * QUANT;
	VectorCopy(start, ts);

	sh = (step[y][x] & (1 << z)) ? SH_BIG : SH_LOW;
	sz = z + (h + sh) / 0x10;
	h = (h + sh) % 0x10;

	/* range check */
	if (sz >= HEIGHT) {
		sz = HEIGHT - 1;
		h = 0x0F;
	}

	/* test connections in all 4 directions */
	for (i = 0; i < 4; i++) {
		/* target coordinates */
		switch (i) {
		case 0:
			ax = x + 1;
			ay = y;
			break;
		case 1:
			ax = x - 1;
			ay = y;
			break;
		case 2:
			ax = x;
			ay = y + 1;
			break;
		case 3:
			ax = x;
			ay = y - 1;
			break;
		}
		/* range check*/
		if ( x < 0 || x >= WIDTH || y < 0 || y >= WIDTH)
			continue;
		/* height check */
		if ((route[sz][ay][ax] & 0x0F) > h)
			continue;
		/* filled check */
		if (filled[ay][ax] & (1 << sz))
			continue;
		/* deep fall check */
		if ((fall[ay][ax] & deep_fall[sz]) == deep_fall[sz])
			continue;

		/* test for walls */
		VectorAdd(start, move_vec[i], te);

		/* center check */
		if (TestLineMask(start, te, 2))
			continue;

		/* lower check */
		ts[2] = te[2] -= UNIT_HEIGHT / 2 - sh * QUANT - 2;
		if (TestLineMask(ts, te, 2))
			continue;

		route[z][y][x] |= 1 << (i + 4);
	}
}


/**
 * @brief Calculates the routing of a map
 * @sa CheckUnit
 * @sa CheckConnections
 * @sa ProcessWorldModel
 */
void DoRouting (void)
{
	int i;
	byte *data;

	PushInfo();
	nummodels += 1;

	/* process actorclip-level */
	/*ProcessLevel(LEVEL_ACTORCLIP);*/
	/* process stepon-level */
	ProcessLevel(LEVEL_STEPON);

	/* build tracing structure */
/*	EmitBrushes(); */
	EmitPlanes();
	MakeTnodes(LEVEL_TRACING);

	PopInfo();
	nummodels -= 1;

	/* reset */
	memset(fall, 0, sizeof(fall));
	memset(filled, 0, sizeof(filled));

	/* get world bounds for optimizing */
	VecToPos(worldMins, wpMins);
	VectorSubtract(wpMins, v_epsilon, wpMins);

	VecToPos(worldMaxs, wpMaxs);
	VectorAdd(wpMaxs, v_epsilon, wpMaxs);

	for (i = 0; i < 3; i++) {
		if (wpMins[i] < 0)
			wpMins[i] = 0;
		if (wpMaxs[i] > ROUTING_NOT_REACHABLE)
			wpMaxs[i] = ROUTING_NOT_REACHABLE;
	}

/*	Com_Printf("(%i %i %i) (%i %i %i)\n", wpMins[0], wpMins[1], wpMins[2], wpMaxs[0], wpMaxs[1], wpMaxs[2]); */

	/* scan area heights */
	U2M_ProgressBar(CheckUnit, HEIGHT * WIDTH * WIDTH, qtrue, "UNITCHECK");

	/* scan connections */
	U2M_ProgressBar(CheckConnections, HEIGHT * WIDTH * WIDTH, qtrue, "CONNCHECK");

	/* store the data */
	data = droutedata;
	*data++ = SH_LOW;
	*data++ = SH_BIG;
	data = CompressRouting(&(route[0][0][0]), data, WIDTH * WIDTH * HEIGHT);
	data = CompressRouting(&(fall[0][0]), data, WIDTH * WIDTH);
	data = CompressRouting(&(step[0][0]), data, WIDTH * WIDTH);

	routedatasize = data - droutedata;

/*	CloseTnodes();*/
}
