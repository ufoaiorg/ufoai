/* scriplib.h */

#ifndef _SCRIPTLIB
#define _SCRIPTLIB

#ifndef __CMDLIB__
#include "cmdlib.h"
#endif

#define	MAXTOKEN	1024

extern	char	token[MAXTOKEN];
extern	char	*scriptbuffer,*script_p,*scriptend_p;
extern	int		grabbed;
extern	int		scriptline; /* extern - qdata */
extern	qboolean	endofscript; /* extern - qdata */


void LoadScriptFile(const char *filename);
void ParseFromMemory(char *buffer, int size);

qboolean GetToken(qboolean crossline);
qboolean TokenAvailable(void);


#endif /* _SCRIPTLIB */
