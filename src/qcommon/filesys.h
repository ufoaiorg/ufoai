/*
==============================================================
FILESYSTEM
==============================================================
*/
#define MAX_MAPS 400
extern char *maps[MAX_MAPS];
extern int anzInstalledMaps;
extern qboolean mapsInstalledInit;
extern int mapInstalledIndex;

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

int FS_FOpenFileWrite(const char *filename, FILE ** f);
int FS_Seek(FILE * f, long offset, int origin);
int FS_WriteFile(const void *buffer, int len, const char *filename);
int FS_Write(const void *buffer, int len, FILE * f);
void FS_GetMaps(void);
void FS_InitFilesystem(void);
void FS_SetGamedir(char *dir);
char *FS_Gamedir(void);
char *FS_NextPath(char *prevpath);
void FS_ExecAutoexec(void);
char *FS_GetCwd(void);
void FS_NormPath(char *path);
qboolean FS_FileExists(char *filename);
char* FS_GetBasePath(char* filename);

void FS_GetMaps(void);

int FS_FOpenFile(const char *filename, FILE ** file);
void FS_FCloseFile(FILE * f);

/* note: this can't be called from another DLL, due to MS libc issues */

int FS_LoadFile(char *path, void **buffer);

/* a null buffer will just return the file length without loading */
/* a -1 length is not present */

void FS_Read(void *buffer, int len, FILE * f);

/* properly handles partial reads */

void FS_FreeFile(void *buffer);

int FS_CheckFile(const char *path);

void FS_BuildFileList(char *files);
char *FS_NextScriptHeader(char *files, char **name, char **text);
void FS_CreatePath(char *path);


