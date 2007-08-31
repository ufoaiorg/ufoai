/* cmdlib.h */

#ifndef __CMDLIB__
#define __CMDLIB__

char *strupr(char *in);
char *strlower(char *in);
int Q_strncasecmp(const char *s1, const char *s2, int n);
int Q_strcasecmp(const char *s1, const char *s2);
void Q_getwd(char *out);

int Q_filelength(qFILE *f);
int FileTime(const char *path);

void Q_mkdir(const char *path);

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

void DefaultExtension(char *path, const char *extension);
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


const char *COM_Parse(const char *data);

char *copystring(const char *s);

void CreatePath(char *path);
void QCopyFile(const char *from, char *to);

void ExpandWildcards(int *argc, char ***argv);

/* for compression routines */
typedef struct
{
	byte	*data;
	int		count;
} cblock_t;

#endif
