/* piclib.h */


void LoadLBM (const char *filename, byte **picture, byte **palette);
void WriteLBMfile (const char *filename, byte *data, int width, int height
	, byte *palette);
void LoadPCX (const char *filename, byte **picture, byte **palette, int *width, int *height);
void WritePCXfile (const char *filename, byte *data, int width, int height
	, byte *palette);

/* loads / saves either lbm or pcx, depending on extension */
void Load256Image (const char *name, byte **pixels, byte **palette,
				   int *width, int *height);
void Save256Image (const char *name, byte *pixels, byte *palette,
				   int width, int height);


void LoadTGA (const char *filename, byte **pixels, int *width, int *height);
