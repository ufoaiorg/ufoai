/* cmdlib.h */

#ifndef __CMDLIB__
#define __CMDLIB__

#include "../../../shared/byte.h"

char *strlower(char *in);
int Q_strncasecmp(const char *s1, const char *s2, int n);
int Q_strcasecmp(const char *s1, const char *s2);
void Q_getwd(char *out);

int Q_filelength(qFILE *f);

void SetQdirFromPath(char *path);
char *ExpandArg(const char *path);	/* from cmd line */
char *ExpandPath(const char *path);	/* from scripts */

double I_FloatTime(void);

qFILE *SafeOpenWrite(const char *filename, qFILE *f);
qFILE *SafeOpenRead(const char *filename, qFILE *f);
void SafeRead(qFILE *f, void *buffer, int count);
void SafeWrite(qFILE *f, void *buffer, int count);

int LoadFile(const char *filename, void **bufferptr);
void CloseFile(qFILE *f);
void FreeFile(void *buffer);
int TryLoadFile(const char *filename, void **bufferptr);
void SaveFile(const char *filename, void *buffer, int count);
qboolean FileExists(const char *filename);

void DefaultExtension(char *path, const char *extension);

char *copystring(const char *s);

/* for compression routines */
typedef struct
{
	byte	*data;
	int		count;
} cblock_t;

#endif
