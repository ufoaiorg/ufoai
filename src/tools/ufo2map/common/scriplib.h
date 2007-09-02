/**
 * @file scriplib.h
 */

#ifndef _SCRIPTLIB
#define _SCRIPTLIB

#define	MAXTOKEN	1024

extern char token[MAXTOKEN];
extern char *script_p;

void LoadScriptFile(const char *filename);
void ParseFromMemory(char *buffer, int size);

qboolean GetToken(qboolean crossline);
qboolean TokenAvailable(void);

#endif /* _SCRIPTLIB */
