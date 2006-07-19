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
	char name[MAX_VAR];
	char combat_notes[MAX_VAR];
	char autopsy[MAX_VAR];
	char xenobiology[MAX_VAR];
	char interrogation[MAX_VAR];
	char interrogation_com[MAX_VAR];
} teamDesc_t;

extern teamDesc_t teamDesc[MAX_TEAMDEFS];
extern int numTeamDesc;

extern char *name_strings[NAME_NUM_TYPES];

char *Com_GiveName(int gender, char *category);
char *Com_GiveModel(int type, int gender, char *category);
int Com_GetModelAndName(char *team, char *path, char *body, char *head, char *name);

void Com_ParseScripts(void);


