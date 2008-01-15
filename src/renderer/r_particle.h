/**
 * @file r_model.h
 * @brief Brush model header file
 * @note d*_t structures are on-disk representations
 * @note m*_t structures are in-memory
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

#ifndef R_PARTICLE_H
#define R_PARTICLE_H

extern ptlArt_t r_particlesArt[MAX_PTL_ART];
extern ptl_t r_particles[MAX_PTLS];
extern int r_numParticlesArt;
extern int r_numParticles;

#endif
