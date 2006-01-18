#ifndef UFOPEDIA_DEFINED
#define UFOPEDIA_DEFINED 1
#include "client.h"

#define MAX_PEDIACHAPTERS	16
#define MAX_PEDIAENTRIES	256

void MN_ResetUfopedia( void );
void MN_ParseUpEntry( char *title, char **text );
void MN_ParseUpChapters( char *title, char **text );
void UP_OpenWith ( char *name );
#endif

