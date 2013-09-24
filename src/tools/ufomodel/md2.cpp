#include "md2.h"
#include "../../shared/ufotypes.h"
#include "../../common/mem.h"
#include "../../shared/shared.h"
#include "../../common/filesys.h"
#include "../../shared/typedefs.h"
#include "../../common/mem.h"
#include "../../common/common.h"
#include "../../shared/byte.h"

void MD2HeaderCheck (const dMD2Model_t* md2, const char* fileName, int bufSize)
{
	/* sanity checks */
	const uint32_t version = LittleLong(md2->version);
	const uint32_t numSkins = LittleLong(md2->num_skins);
	const uint32_t numTris = LittleLong(md2->num_tris);
	const uint32_t numVerts = LittleLong(md2->num_verts);
	const uint32_t ident = LittleLong(md2->ident);

	if (ident != IDALIASHEADER)
		Com_Error(ERR_DROP, "%s is no valid md2 file", fileName);

	if (version != MD2_ALIAS_VERSION)
		Com_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", fileName, version, MD2_ALIAS_VERSION);

	if (bufSize != LittleLong(md2->ofs_end))
		Com_Error(ERR_DROP, "model %s broken offset values (%i, %i)", fileName, bufSize, LittleLong(md2->ofs_end));

	if (numSkins == 0 || numSkins >= MD2_MAX_SKINS)
		Com_Error(ERR_DROP, "model '%s' has invalid skin number: %i", fileName, numSkins);

	if (numVerts == 0 || numVerts >= MD2_MAX_VERTS)
		Com_Error(ERR_DROP, "model %s has too many (or no) vertices (%i/%i)", fileName, numVerts, MD2_MAX_VERTS);

	if (numTris == 0 || numTris >= MD2_MAX_TRIANGLES)
		Com_Error(ERR_DROP, "model %s has too many (or no) triangles (%i/%i)", fileName, numTris, MD2_MAX_TRIANGLES);
}

void MD2SkinEdit (const byte* buf, const char* fileName, int bufSize, void* userData)
{
	uint32_t numSkins;
	byte* const copy = Mem_Dup(byte, buf, bufSize);
	dMD2Model_t* md2 = (dMD2Model_t*)copy;
	char* md2Path;

	MD2HeaderCheck(md2, fileName, bufSize);

	md2Path = (char*) copy + LittleLong(md2->ofs_skins);
	numSkins = LittleLong(md2->num_skins);

	Com_Printf("  \\ - skins %i\n", numSkins);
	for (int i = 0; i < numSkins; i++) {
		char* name = md2Path + i * MD2_MAX_SKINNAME;
		Com_Printf("  \\ - skin %i: %s\n", i + 1, name);
		printf("  \\ - new skin: ");
		fflush(stdout);
		scanf(va("%%%is", MD2_MAX_SKINNAME), name);
	}

	ScopedFile md2ModelFile;
	FS_OpenFile(fileName, &md2ModelFile, FILE_WRITE);
	if (!md2ModelFile) {
		Com_Printf("Error writing md2 file %s\n", fileName);
		Mem_Free(copy);
		return;
	}
	FS_Write(copy, bufSize, &md2ModelFile);
	Mem_Free(copy);
}

void MD2Info (const byte* buf, const char* fileName, int bufSize, void* userData)
{
	const dMD2Model_t* md2 = (const dMD2Model_t*)buf;

	MD2HeaderCheck(md2, fileName, bufSize);

	const uint32_t numSkins = LittleLong(md2->num_skins);
	const uint32_t numVerts = LittleLong(md2->num_verts);
	const uint32_t numST = LittleLong(md2->num_st);
	const uint32_t numTris = LittleLong(md2->num_tris);
	const uint32_t numGLCmds = LittleLong(md2->num_glcmds);
	const uint32_t numFrames = LittleLong(md2->num_frames);
	const uint32_t frameSize = LittleLong(md2->framesize);
	const uint32_t skinHeight = LittleLong(md2->skinheight);
	const uint32_t skinWidth = LittleLong(md2->skinwidth);
	const uint32_t version = LittleLong(md2->version);

	const uint32_t ofsST = LittleLong(md2->ofs_st);
	const uint32_t ofsTris = LittleLong(md2->ofs_tris);
	const uint32_t ofsFrames = LittleLong(md2->ofs_frames);
	const uint32_t ofsGLCmds = LittleLong(md2->ofs_glcmds);
	const uint32_t ofsEnd = LittleLong(md2->ofs_end);
	const uint32_t ofsSkins = LittleLong(md2->ofs_skins);

	const char* md2Path = (const char*) md2 + LittleLong(md2->ofs_skins);

	Com_Printf("  \\ - skins %i\n", numSkins);
	for (int i = 0; i < numSkins; i++) {
		const char* name = md2Path + i * MD2_MAX_SKINNAME;
		Com_Printf("    \\ -> skin %i: %s\n", i + 1, name);
	}
	Com_Printf("  \\ - vertices %i\n", numVerts);
	Com_Printf("  \\ - texcoords %i\n", numST);
	Com_Printf("  \\ - tris %i\n", numTris);
	Com_Printf("  \\ - glcmds %i\n", numGLCmds);
	Com_Printf("  \\ - frames %i\n", numFrames);
	Com_Printf("  \\ - frameSize %i\n", frameSize);
	Com_Printf("  \\ - skinHeight %i\n", skinHeight);
	Com_Printf("  \\ - skinWidth %i\n", skinWidth);
	Com_Printf("  \\ - version %i\n", version);
	Com_Printf("  \\ - offsets\n");
	Com_Printf("    \\ - ofsST %i\n", ofsST);
	Com_Printf("    \\ - ofsTris %i\n", ofsTris);
	Com_Printf("    \\ - ofsFrames %i\n", ofsFrames);
	Com_Printf("    \\ - ofsGLCmds %i\n", ofsGLCmds);
	Com_Printf("    \\ - ofsSkins %i\n", ofsSkins);
	Com_Printf("    \\ - ofsEnd %i\n", ofsEnd);
}

void MD2SkinNum (const byte* buf, const char* fileName, int bufSize, void* userData)
{
	byte* copy = Mem_Dup(byte, buf, bufSize);
	dMD2Model_t* md2 = (dMD2Model_t*)copy;

	MD2HeaderCheck(md2, fileName, bufSize);

	const uint32_t numSkins = LittleLong(md2->num_skins);

	const uint32_t ofsST = LittleLong(md2->ofs_st);
	const uint32_t ofsTris = LittleLong(md2->ofs_tris);
	const uint32_t ofsFrames = LittleLong(md2->ofs_frames);
	const uint32_t ofsGLCmds = LittleLong(md2->ofs_glcmds);
	const uint32_t ofsEnd = LittleLong(md2->ofs_end);
	const uint32_t ofsSkins = LittleLong(md2->ofs_skins);

	uint32_t moveOffset = ofsEnd;
#define CHECKMAX(val) 	if ((val) > ofsSkins && (val) < moveOffset) moveOffset = (val);
	CHECKMAX(ofsST);
	CHECKMAX(ofsTris);
	CHECKMAX(ofsFrames);
	CHECKMAX(ofsGLCmds);
	CHECKMAX(ofsSkins);
#undef CHECKMAX

	Com_Printf("  \\ - skins %i\n", numSkins);
	int newSkins = 0;
	printf("  \\ - new skin number: ");
	fflush(stdout);
	scanf("%i", &newSkins);
	if (newSkins <= 0) {
		Com_Printf("A model must have a skin\n");
		Mem_Free(copy);
		return;
	}
	if (newSkins > MD2_MAX_SKINS) {
		Com_Printf("Only %i skins are allowed\n", MD2_MAX_SKINS);
		Mem_Free(copy);
		return;
	}

	if (newSkins == numSkins) {
		Mem_Free(copy);
		return;
	}

	const int32_t deltaSkins = newSkins - numSkins;
	const int32_t offsetDelta = deltaSkins * MD2_MAX_SKINNAME;
	if (ofsST > ofsSkins)
		md2->ofs_st = LittleLong(ofsST + offsetDelta);
	if (ofsTris > ofsSkins)
		md2->ofs_tris = LittleLong(ofsTris + offsetDelta);
	if (ofsFrames > ofsSkins)
		md2->ofs_frames = LittleLong(ofsFrames + offsetDelta);
	if (ofsGLCmds > ofsSkins)
		md2->ofs_glcmds = LittleLong(ofsGLCmds + offsetDelta);
	md2->ofs_end = LittleLong(ofsEnd + offsetDelta);
	md2->num_skins = LittleLong(newSkins);

	Com_Printf("change to %i skins\n", newSkins);
	if (deltaSkins > 0) {
		copy = (byte*)Mem_ReAlloc(copy, md2->ofs_end);
		md2 = (dMD2Model_t*)copy;
	}

	const int n = ofsEnd - moveOffset;
	byte* from = copy + moveOffset;
	byte* to = from + offsetDelta;
	memmove(to, from, n);

	if (deltaSkins > 0) {
		char*  md2Path = (char*) copy + LittleLong(md2->ofs_skins);
		for (int i = numSkins; i < numSkins + deltaSkins; i++) {
			char* name = md2Path + i * MD2_MAX_SKINNAME;
			memset(name, 0, MD2_MAX_SKINNAME);
			strcpy(name, ".none");
			Com_Printf("  \\ - skin %i: %s\n", i + 1, name);
			printf("  \\ - new skin: ");
			fflush(stdout);
			scanf(va("%%%is", MD2_MAX_SKINNAME), name);
		}
	}
	ScopedFile md2ModelFile;
	FS_OpenFile(fileName, &md2ModelFile, FILE_WRITE);
	if (!md2ModelFile) {
		Com_Printf("Error writing md2 file %s\n", fileName);
		Mem_Free(copy);
		return;
	}
	FS_Write(copy, md2->ofs_end, &md2ModelFile);
	Mem_Free(copy);
}

void MD2GLCmdsRemove (const byte* buf, const char* fileName, int bufSize, void* userData)
{
	uint32_t numGLCmds;
	const dMD2Model_t* md2 = (const dMD2Model_t*)buf;

	MD2HeaderCheck(md2, fileName, bufSize);

	numGLCmds = LittleLong(md2->num_glcmds);

	if (numGLCmds > 0) {
		byte        *const copy     = Mem_Dup(byte, buf, bufSize);
		dMD2Model_t* const fixedMD2 = (dMD2Model_t*)copy;
		const size_t delta = numGLCmds * sizeof(uint32_t);
		const uint32_t offset = LittleLong(fixedMD2->ofs_glcmds);

		if (LittleLong(fixedMD2->ofs_skins) > offset || LittleLong(fixedMD2->ofs_frames) > offset
		 || LittleLong(fixedMD2->ofs_st) > offset || LittleLong(fixedMD2->ofs_tris) > offset) {
			Com_Error(ERR_DROP, "Unexpected order of the different data lumps");
		}

		fixedMD2->ofs_end = LittleLong(fixedMD2->ofs_end - delta);
		fixedMD2->ofs_glcmds = 0;
		fixedMD2->num_glcmds = 0;

		bufSize -= delta;

		FS_WriteFile(fixedMD2, bufSize, fileName);

		Mem_Free(copy);

		*(size_t*)userData += delta;
		Com_Printf("  \\ - removed %i glcmds from '%s' (save " UFO_SIZE_T " bytes)\n",
				numGLCmds, fileName, delta);
	}
}
