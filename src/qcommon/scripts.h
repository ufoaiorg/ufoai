/*
==============================================================

SCRIPT PARSING

==============================================================
*/

#define MAX_TEAMDEFS	128

#define LASTNAME	3
typedef enum {
	NAME_NEUTRAL,
	NAME_FEMALE,
	NAME_MALE,

	NAME_LAST,
	NAME_FEMALE_LAST,
	NAME_MALE_LAST,

	NAME_NUM_TYPES
} nametypes_t;

typedef struct teamDesc_s {
	char id[MAX_VAR];
	qboolean alien;			/**< is this an alien teamdesc definition */
	char name[MAX_VAR];
	char tech[MAX_VAR]; /**< tech id from research.ufo */
} teamDesc_t;

extern teamDesc_t teamDesc[MAX_TEAMDEFS];
extern int numTeamDesc;

extern const char *name_strings[NAME_NUM_TYPES];

char *Com_GiveName(int gender, char *category);
char *Com_GiveModel(int type, int gender, char *category);
int Com_GetModelAndName(char *team, character_t * chr);

void Com_AddObjectLinks(void);
void Com_ParseScripts(void);
