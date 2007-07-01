/* cmdlib.h */

#ifndef __CMDLIB__
#define __CMDLIB__

/* to support the gnuc __attribute__ command */
#ifndef __GNUC__
#  define  __attribute__(x)  /*NOTHING*/
#endif


#ifdef _MSC_VER
#  define inline __inline
#  pragma warning(disable : 4244)     /* MIPS */
#  pragma warning(disable : 4136)     /* X86 */
#  pragma warning(disable : 4051)     /* ALPHA */
#  pragma warning(disable : 4018)     /* signed/unsigned mismatch */
#  pragma warning(disable : 4305)     /* truncate from double to float */
#endif	/* _MSC_VER */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <stddef.h>
#include <assert.h>

#ifndef errno
extern int errno;
#endif

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
typedef enum {qfalse, qtrue} qboolean;
typedef unsigned char byte;
#endif

#include "qfiles.h"

#if defined(_WIN32)
# ifndef snprintf
#  define snprintf _snprintf
# endif
#endif

extern	qboolean verbose;

#define SYS_VRB 0 /* verbose support (on/off) */
#define SYS_STD 1 /* standard print level */
#define SYS_WRN 2 /* warnings */
#define SYS_ERR 3 /* error */

/* the dec offsetof macro doesnt work very well... */
#define myoffsetof(type,identifier) ((size_t)&((type *)0)->identifier)


/* set these before calling CheckParm */
extern int myargc;
extern char **myargv;

char *strupr(char *in);
char *strlower(char *in);
int Q_strncasecmp(const char *s1, const char *s2, int n);
int Q_strcasecmp(const char *s1, const char *s2);
void Q_getwd(char *out);

int Q_filelength(qFILE *f);
int FileTime(const char *path);

void Q_mkdir(const char *path);

extern char qdir[1024];
extern char gamedir[1024];
void SetQdirFromPath(char *path);
char *ExpandArg(const char *path);	/* from cmd line */
char *ExpandPath(const char *path);	/* from scripts */
char *ExpandPathAndArchive(const char *path);

double I_FloatTime(void);

int CheckParm(char *check);

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

void DefaultExtension(char *path, char *extension);
void DefaultPath(char *path, char *basepath);
void StripFilename(char *path);
void StripExtension(char *path);

void ExtractFilePath(const char *path, char *dest);
void ExtractFileBase(const char *path, char *dest);
void ExtractFileExtension(const char *path, char *dest);

int ParseNum(char *str);

short BigShort(short l);
short LittleShort(short l);
int BigLong(int l);
int LittleLong(int l);
float BigFloat(float l);
float LittleFloat(float l);


char *COM_Parse(char *data);

char *copystring(const char *s);

void CreatePath(char *path);
void QCopyFile(const char *from, char *to);

extern qboolean archive;
extern char archivedir[1024];

void Error(const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Sys_Printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
void Sys_FPrintf(int flag, const char *text, ...) __attribute__((format(printf, 2, 3)));

void ExpandWildcards(int *argc, char ***argv);

/* for compression routines */
typedef struct
{
	byte	*data;
	int		count;
} cblock_t;

#endif
