/**
 * @file cl_research.h
 * @brief Header for research related stuff.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_CL_RESEARCH
#define CLIENT_CL_RESEARCH

#include "cl_ufopedia.h"

#define MAX_RESEARCHDISPLAY 25	/**< Number of the available string (in the list) in the research menu */
#define MAX_RESEARCHLIST 32
#define MAX_TECHNOLOGIES 256	/**< Maximum number of technologies overall. */
#define MAX_TECHLINKS 16	/**< Maximum number of requirements in a technology (i.e in require_AND and require_OR).
				 *   Needs to be synced with MAX_TECHLINKS in q_shared.h */
#define MAX_DESCRIPTIONS 8	/**< Maximum number of descriptions (per tech and description-type). */

#define TECH_INVALID -1	/**< The value for an "undef" tech-index. */

/** @brief The status of a research (per tech) */
typedef enum researchStatus_s {
	RS_NONE,
	RS_RUNNING,
	RS_PAUSED,
	RS_FINISH
} researchStatus_t;

/** @brief Types of research topics */
typedef enum researchType_s {
	RS_TECH,
	RS_WEAPON,
	RS_ARMOUR,
	RS_CRAFT,
	RS_CRAFTITEM,
	RS_BUILDING,
	RS_ALIEN,
	RS_UGV,
	RS_NEWS,
	RS_LOGIC
} researchType_t;

typedef enum requirementType_s {
	RS_LINK_TECH,
	RS_LINK_TECH_NOT,
#if 0
	RS_LINK_TECH_BEFORE,	/**< @note I think this would be even more useful than XOR. */
	RS_LINK_TECH_XOR,
#endif
	RS_LINK_ITEM,
	RS_LINK_EVENT,
	RS_LINK_ALIEN,
	RS_LINK_ALIEN_DEAD,
	RS_LINK_ALIEN_GLOBAL,
	MAX_RS_LINKTYPES
} requirementType_t;

typedef struct requirement_s {
	requirementType_t type;	/**< What type the requ. is: item (in store/quarantine), event, etc... */
	char *id;	/**< Dependency id (text-id) */
	void *link;	/**< Dependency pointer (depends on the type)
				 * RS_LINK_TECH -> technology_t*
				 * RS_LINK_TECH_NOT -> (planned to be technology_t*)
				 * RS_LINK_TECH_BEFORE -> (planned to be technology_t*)
				 * RS_LINK_TECH_XOR -> (planned to be technology_t*)
				 * RS_LINK_ITEM -> objDef_t*
				 * RS_LINK_EVENT -> nothing yet
				 * RS_LINK_ALIEN		-> teamDef_t*
				 * RS_LINK_ALIEN_DEAD	-> teamDef_t*
				 * RS_LINK_ALIEN_GLOBAL -> NULL
				 */
#if 0
	char *id2;	/**< 2. dependency id (text-id) */
	void *link2;	/**< 2. dependency pointer (depends on the type)
				 * RS_LINK_TECH -> NULL
				 * RS_LINK_TECH_NOT -> NULL
				 * RS_LINK_TECH_BEFORE -> (planned to be technology_t*)
				 * RS_LINK_TECH_XOR -> (planned to be technology_t*)
				 * RS_LINK_ITEM -> NULL
				 * RS_LINK_EVENT -> nothing yet
				 * RS_LINK_ALIEN		-> NULL
				 * RS_LINK_ALIEN_DEAD	-> NULL
				 * RS_LINK_ALIEN_GLOBAL -> NULL
				 */
#endif
	int amount;	/**< How many items are needed for research ... if any. (fix) */
	int collected;	/**< How many items have been collected. (varies) */
} requirement_t;

typedef struct requirements_s {
	int numLinks;			/**< The number of requirements for this tech. */
	requirement_t links[MAX_TECHLINKS];
} requirements_t;

typedef struct stringlist_s {
	int numEntries;				/**< The number of used strings/techs. */
	char string[MAX_TECHLINKS][MAX_VAR];	/**< A list of strings. */
	int idx[MAX_TECHLINKS];		/**< holds the indices of the tech for faster operation after finding it once */
} stringlist_t;

typedef struct markResearched_s {
	qboolean markOnly[MAX_CAMPAIGNS];
	char *campaign[MAX_CAMPAIGNS];
	int numDefinitions;
} markResearched_t;

/** @brief Types for tech mail definitions - see tech->mail[] */
typedef enum {
	TECHMAIL_PRE,		/**< this is a mail_pre (proposal) definition */
	TECHMAIL_RESEARCHED,	/**< this is the mail definition (after research was completed) */

	TECHMAIL_MAX
} techMailType_t;

/**
 * @brief available mails for a tech - mail and mail_pre in script files
 * @sa eventMail_t
 */
typedef struct techMail_s {
	char *from;			/**< sender (_mail_from_paul_navarre, _mail_from_dr_connor) */
	char *to;			/**< recipient (_mail_to_base_commander) */
	char *subject;		/**< mail subject line - if mail and mail_pre are available
								 * this will be filled with Proposal: (mail_pre) and Re: (mail)
								 * automatically */
	char *date;			/**< date string, if empty use the date of research */
	char *icon;			/**< name of an image file to display in the mail client */
	qboolean read;				/**< already read the mail? */
} techMail_t;

typedef enum {
	MAILSENT_NONE,		/**< No mail sent yet. */
	MAILSENT_PROPOSAL,	/**< Mail for research proposal already send. */
	MAILSENT_FINISHED,	/**< Mail for finsihed research snet (and maybe the previous as well). */

	MAILSENT_MAX
} mailSentType_t;

typedef struct descriptions_s {
	int numDescriptions;	/**< The number of descriptions. */
	int usedDescription;	/**< The index of the first used description, so we do not get a different text each time it should be displayed. undef=-1
				 * @todo Check if we should set this in the function that updates the research_proposals? Currently it's only in RS_GetDescription. */
	char *text[MAX_DESCRIPTIONS];	/**< A list of descriptions (Short text-id to get the full text via gettext). */
	char *tech[MAX_DESCRIPTIONS];	/**< The technology to check (i.e. are its requirements met?) if this decription should be displayed. */
} descriptions_t;

/** @brief This is the technology parsed from research.ufo */
typedef struct technology_s {
	char *id;		/**< Short (unique) id/name. */
	int idx;			/**< Self-link in the global list */
	char *name;		/**< Full name of this technology. */
	descriptions_t description;	/**< Descriptions of researched item.  */
	descriptions_t pre_description;	/**< Descriptions of item before it's researched. */
	researchType_t type;		/**< Defines what type this tech-entry is an where to search for other information "tech", "weapon" etc... see research.ufo for more */

	struct technology_s *redirect;	/**< Set this to the entry that is supposed to get displayed instead of this one.
									 * Mopstly used for e.g ammo that doesn't need its own description but rather the one for the weapon. */

	requirements_t require_AND;	/**< A list of requirements that ALL need to be met (= AND-related) See struct above. */
	requirements_t require_OR;	/**< A list of requirements where ANY need to be met (= OR-related) See struct above. */
	qboolean statusCollected;	/**< Did we loot any items of this tech?
					 * This is updated from the info stored in the require_OR and require_AND lists.
					 * @see RS_CheckCollected. */

	char *provides;		/**< The item that this technology enables. */
	float overalltime, time;	/**< The time that is needed to research this tech. (in days).
					 * "overalltime" stays always the same,
					 * "time" will be modified when it is under research.*/
	int delay;			/**< @todo Number in days the system should wait until the tech is available for research.
					 * Starting from the first time all other dependencies have been fulfilled and
					 * counting only if a day has passed and they still are met. */

	researchStatus_t statusResearch;	/**< Current status of the research. */
	char *finishedResearchEvent;		/**< NULL or string with scriptable commands that are executed after the tech was successfully researched */

	struct base_s	*base;	/**< The base this tech is researched in. */
	int scientists;			/**< How many scientists (from "base") are researching this tech. */

	char *image;			/**< Image to display in the Ufopedia and other menues for this tech.
							 * If not set in the .ufo file this is auto-set in RS_InitTree.
							 * @sa cl_research.c: RS_InitTree */
	char *mdl;				/**< Same as "image" but it's a 3d model.. */

	int statusResearchable;		/**< Is this item researchable? */

	int produceTime;			/**< How many days the production of this items runs. */
	qboolean pushnews;			/**< Push news about this tech when researched. Note: usually you should use this only for tech with time = 0. */
	mailSentType_t mailSent;		/**< Store if a Mail has been sent to the commander (player). */
	requirements_t require_for_production;	/**< A list of items that are needed (and used up) on production of _this_ item.
						 * Only "item"-type is allowed.
						 * All requirements need to be fulfilled in order for _one_ item to be produced.
						 * This check is done for each item.*/

	date_t preResearchedDate;	/**< Date for UFOpaedia. Research proposal. */
	date_t researchedDate;		/**< Date for UFOpaedia. Finished research. */

	markResearched_t markResearched;	/**< Mark as researched at parsing state - but i only know the date if we already started a campaign. */

	/* Pedia info */
	pediaChapter_t *upChapter;					/**< UFOpaedia chapter as stored in research.ufo. */
	struct technology_s *upPrev;	/**< Previous tech in pedia. */
	struct technology_s *upNext;	/**< Next tech in pedia. */

	techMail_t mail[TECHMAIL_MAX];	/**< UFOpaedia mails. See techMailType_t for the different array-entries. */
	int numTechMails;				/**< Used to store which "mail" entry to display in pedia. if It's equalt to TECHMAIL_MAX both mailtypes are available. */

	struct technology_s *hashNext;
	struct technology_s *hashProvidedNext;
} technology_t;

void RS_InitStartup(void);
void RS_ResetTechs(void);
void RS_ResearchRun(void);
void RS_ParseTechnologies(const char *name, const char **text);
#if 0
qboolean RS_ItemIsResearched(const char *idProvided);
#endif
qboolean RS_IsResearched_idx(int techIdx);
qboolean RS_IsResearched_ptr(const technology_t *tech);
int RS_Collected_(const technology_t * tech);

void RS_AddObjectTechs(void);
void RS_RequiredLinksAssign(void);
void RS_InitTree(qboolean load);
const char *RS_GetDescription(descriptions_t *desc);
void RS_MarkCollected(technology_t *tech) __attribute__((nonnull));
void RS_MarkResearchable(qboolean init, const struct base_s *base);
void RS_ResearchFinish(technology_t* tech);
void RS_MarkOneResearchable(technology_t *tech);
void RS_MarkResearchedAll(void);

void RS_AssignScientist(technology_t* tech, struct base_s *base);
void RS_RemoveScientist(technology_t* tech, struct employee_s *employee);
void RS_RemoveFiredScientist(struct base_s *base, struct employee_s *employee);
void RS_RemoveScientistsExceedingCapacity(struct base_s *base);

technology_t *RS_GetTechByID(const char *id);
technology_t *RS_GetTechByProvided(const char *idProvided);
technology_t* RS_GetTechByIDX(int tech_idx);
technology_t *RS_GetTechWithMostScientists(const struct base_s *base);
int RS_GetTechIdxByName(const char *name);
int RS_CountInBase(const struct base_s *base);
qboolean RS_ScriptSanityCheck(void);

/* UFOpaedia function - but needs technology_t */
void UP_Article(technology_t* tech, eventMail_t *mail);
void UP_AircraftDescription(const technology_t* t);
void UP_UGVDescription(const ugv_t *ugvType);

qboolean RS_RequirementsMet(const requirements_t *required_AND, const requirements_t *required_OR, const struct base_s *base);

#endif /* CLIENT_CL_RESEARCH_H */
