#ifndef _GL_FONTS
#define _GL_FONTS

#define MAX_HASH_STRING	128
#define MAX_FONT_CACHE	1024
#define MAX_FONTS	16
#define	MAX_FONTNAME	32

typedef struct font_s
{
	char	name[MAX_FONTNAME];
	TTF_Font *font;
	int style;
	int lineSkip; /* TTF_FontLineSkip */
	int height;
} font_t;

/* font definitions */
font_t	fonts[MAX_FONTS];
int	numFonts;
typedef struct fontCache_s
{
	char string[MAX_HASH_STRING];
	SDL_Surface *pixel;
	struct fontCache_s *next;
	vec2_t size; /* real width and height */
} fontCache_t;

fontCache_t fontCache[MAX_FONT_CACHE];
fontCache_t* hash[MAX_FONT_CACHE];

typedef struct
{
	char	*name;
	int	renderStyle;
} fontRenderStyle_t;

#define NUM_FONT_STYLES (sizeof(fontStyle) / sizeof (fontRenderStyle_t))

/* public */
int	Font_DrawString (char *font, int align, int x, int y, int maxWidth, char *c);
void	Font_Length( char *font, char *c, int *width, int *height );
void	Font_CleanCache( void );
void	Font_Shutdown( void );
void	Font_Init( void );
void	Font_ListCache_f( void );
void	Font_Register( char *name, int size, char* path, char* style );
void	Font_Register( char *name, int size, char* path, char* style );


#endif /* _GL_FONTS */
