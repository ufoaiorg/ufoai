#ifndef R_FONTS_H
#define R_FONTS_H

#define MAX_HASH_STRING		128
#define MAX_FONT_CACHE		1024
#define MAX_FONTS		16
#define	MAX_FONTNAME		32

/* starting offset for font texture numbers (used in font-cache) */
#define TEXNUM_FONTS        TEXNUM_IMAGES + MAX_GLTEXTURES

typedef struct font_s {
	const char *name;
	TTF_Font *font;
	SDL_RWops *rw;				/**< ttf font reading structure for SDL_RWops */
	byte *buffer;				/**< loaded file */
	int style;					/**< see also fontRenderStyle_t */
	int lineSkip;				/**< TTF_FontLineSkip */
	int height;
} font_t;

/* font definitions */
typedef struct fontCache_s {
	char string[MAX_HASH_STRING];	/** hash id */
	struct fontCache_s *next;	/**< next hash entry in case of collision */
	vec2_t size;				/**< real width and height */
	vec2_t texsize;				/**< texture width and height */
	GLuint texPos;				/**< the bound texture number/identifier*/
} fontCache_t;

typedef struct {
	const char *name;
	int renderStyle;
} fontRenderStyle_t;

#define NUM_FONT_STYLES (sizeof(fontStyle) / sizeof (fontRenderStyle_t))

/* public */
void R_FontCleanCache(void);
void R_FontShutdown(void);
void R_FontInit(void);
void R_FontListCache_f(void);

#endif
