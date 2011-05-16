/**
 * @file r_model_dpm.h
 * @brief dpm model loading
 * @note type 2 model (hierarchical skeletal pose)
 * within this specification, int is assumed to be 32bit, float is assumed to be 32bit, char is assumed to be 8bit, text is assumed to be an array of chars with NULL termination
 * all values are big endian (also known as network byte ordering), NOT x86 little endian
 * general notes:
 * a pose is a 3x4 matrix (rotation matrix, and translate vector)
 * parent bones must always be lower in number than their children, models will be rejected if this is not obeyed (can be fixed by modelling utilities)
 * @note utility notes:
 * if a hard edge is desired (faceted lighting, or a jump to another set of skin coordinates), vertices must be duplicated
 * ability to visually edit groupids of triangles is highly recommended
 * bones should be markable as 'attach' somehow (up to the utility) and thus protected from culling of unused resources
 * frame 0 is always the base pose (the one the skeleton was built for)
 * @note game notes:
 * the loader should be very thorough about error checking, all vertex and bone indices should be validated, etc
 * the gamecode can look up bone numbers by name using a builtin function, for use in attachment situations (the client should have the same model as the host of the gamecode in question - that is to say if the server gamecode is setting the bone number, the client and server must have vaguely compatible models so the client understands, and if the client gamecode is setting the bone number, the server could have a completely different model with no harm done)
 * the triangle groupid values are up to the gamecode, it is recommended that gamecode process this in an object-oriented fashion (I.E. bullet hits entity, call that entity's function for getting properties of that groupid)
 * frame 0 should be usable, not skipped
 * speed optimizations for the saver to do:
 * remove all unused data (unused bones, vertices, etc, be sure to check if bones are used for attachments however)
 * sort triangles into strips
 * sort vertices according to first use in a triangle (caching benefits) after sorting triangles
 * speed optimizations for the loader to do:
 * if the model only has one frame, process it at load time to create a simple static vertex mesh to render (this is a hassle, but it is rewarding to optimize all such models)
 * @note rendering process:
 * 1*. one or two poses are looked up by number
 * 2*. boneposes (matrices) are interpolated, building bone matrix array
 * 3.  bones are parsed sequentially, each bone's matrix is transformed by it's parent bone (which can be -1; the model to world matrix)
 * 4.  meshs are parsed sequentially, as follows:
 *     1. vertices are parsed sequentially and may be influenced by more than one bone (the results of the 3x4 matrix transform will be added together - weighting is already built into these)
 *     2. shader is looked up and called, passing vertex buffer (temporary) and triangle indices (which are stored in the mesh)
 * 5.  rendering is complete
 * - these stages can be replaced with completely dynamic animation instead of pose animations.
 */


/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

void R_ModLoadAliasDPMModel(struct model_s *mod, byte *buffer, int bufSize);
void R_ModelLoadDPMVertsForFrame(struct model_s *mod, int frame);
