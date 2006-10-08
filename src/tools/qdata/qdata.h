/* qdata.h */


#include <sys/stat.h>
#include <stdint.h>

#include "../ufo2map/common/scriplib.h"
#include "../ufo2map/common/mathlib.h"
#include "../ufo2map/common/lbmlib.h"
#include "../ufo2map/common/threads.h"
#include "../ufo2map/common/bspfile.h"
#include "common/trilib.h"
#include "common/l3dslib.h"

void Cmd_Modelname (void);
void Cmd_Base (void);
void Cmd_Cd (void);
void Cmd_Origin (void);
void Cmd_ScaleUp (void);
void Cmd_Frame (void);
void Cmd_Modelname (void);
void Cmd_Skin (void);
void Cmd_Skinsize (void);
void FinishModel (void);

void Cmd_Inverse16Table( void );

void Cmd_SpriteName (void);
void Cmd_Load (void);
void Cmd_SpriteFrame (void);
void FinishSprite (void);

void Cmd_Grab (void);
void Cmd_Raw (void);
void Cmd_Mip (void);
void Cmd_Environment (void);
void Cmd_Colormap (void);

void Cmd_File (void);
void Cmd_Dir (void);
void Cmd_StartWad (void);
void Cmd_EndWad (void);
void Cmd_Mippal (void);
void Cmd_Mipdir (void);
void Cmd_Alphalight (void);

void Cmd_Video (void);

void RemapZero (byte *pixels, byte *palette, int width, int height);

void ReleaseFile (char *filename);

extern	byte		*byteimage, *lbmpalette;
extern	int			byteimagewidth, byteimageheight;

extern	qboolean	g_release;			/* don't grab, copy output data to new tree */
extern	char		g_releasedir[1024];	/* c:\quake2\baseq2, etc */
extern	qboolean	g_archive;			/* don't grab, copy source data to new tree */
extern	qboolean	do3ds;
extern	char		g_only[256];		/* if set, only grab this cd */
extern	qboolean	g_skipmodel;		/* set true when a cd is not g_only */

extern	char		*trifileext;
