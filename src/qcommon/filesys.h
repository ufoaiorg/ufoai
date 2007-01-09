/*
==============================================================
FILESYSTEM
==============================================================
*/
#define MAX_MAPS 400
extern char *maps[MAX_MAPS];
extern int anzInstalledMaps;
extern int mapInstalledIndex;

typedef struct qFILE_s {
	void *z; /* in case of the file being a zip archive */
	FILE *f; /* in case the file being part of a pak or the actual file */
	char name[MAX_OSPATH];
	unsigned long filepos;
} qFILE;

typedef enum {
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

typedef enum {
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;

int FS_FileLength(qFILE * f);
void FS_FOpenFileWrite(const char *filename, qFILE * f);
int FS_Seek(qFILE * f, long offset, int origin);
int FS_WriteFile(const void *buffer, size_t len, const char *filename);
int FS_Write(const void *buffer, int len, qFILE * f);
void FS_InitFilesystem(void);
void FS_SetGamedir(const char *dir);
char *FS_Gamedir(void);
char *FS_NextPath(const char *prevpath);
void FS_ExecAutoexec(void);
char *FS_GetCwd(void);
void FS_NormPath(char *path);
qboolean FS_FileExists(const char *filename);
char* FS_GetBasePath(char* filename);

void FS_GetMaps(qboolean reset);

int FS_FOpenFile(const char *filename, qFILE * file);
void FS_FCloseFile(qFILE * f);

/* note: this can't be called from another DLL, due to MS libc issues */

int FS_LoadFile(const char *path, void **buffer);

/* a null buffer will just return the file length without loading */
/* a -1 length is not present */

#ifdef DEBUG
#define FS_Read(buffer, len, f) FS_ReadDebug(buffer, len, f, __FILE__, __LINE__)
int FS_ReadDebug(void *buffer, int len, qFILE * f, char* file, int line);
#else
int FS_Read(void *buffer, int len, qFILE * f);
#endif
/* properly handles partial reads */

void FS_FreeFile(void *buffer);

int FS_CheckFile(const char *path);

void FS_BuildFileList(char *files);
char *FS_NextScriptHeader(const char *files, char **name, char **text);
void FS_CreatePath(char *path);


