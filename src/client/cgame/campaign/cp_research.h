/**
 * @file cp_research.h
 * @brief Header for research related stuff.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#define MAX_RESEARCHDISPLAY 22	/**< Number of the available string (in the list) in the research menu */
#define MAX_RESEARCHLIST 32
#define MAX_TECHNOLOGIES 256	/**< Maximum number of technologies overall. */
#define MAX_TECHLINKS 16	/**< Maximum number of requirements in a technology (i.e in requireAND and requireOR). */
#define MAX_DESCRIPTIONS 8	/**< Maximum number of descriptions (per tech and description-type). */

#define TECH_INVALID -1	/**< The value for an "undef" tech-index. */

#define ANTIMATTER_TECH_ID "antimatter"

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
	RS_LINK_ITEM,
	RS_LINK_ALIEN,
	RS_LINK_ALIEN_DEAD,
	RS_LINK_ALIEN_GLOBAL,
	RS_LINK_UFO,
	RS_LINK_ANTIMATTER,
	MAX_RS_LINKTYPES
} requirementType_t;

typedef struct requirement_s {
	requirementType_t type;	/**< What type the requ. is: item (in store/quarantine), event, etc... */
	char *id;	/**< Dependency id (text-id) */
	union typelink_t {
		const struct technology_s *tech;
		const struct objDef_s *od;
		const struct teamDef_s *td;
		const struct aircraft_s *aircraft;
	} link;
	int amount;	/**< How many items are needed for research ... if any. (fix) */
	int collected;	/**< How many items have been collected. (varies) */
} requirement_t;

typedef struct requirements_s {
	int numLinks;			/**< The number of requirements for this tech. */
	requirement_t links[MAX_TECHLINKS];
} requirements_t;

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
	const char *to;		/**< recipient (_mail_to_base_commander) */
	const char *subject;	/**< mail subject line - if mail and mail_pre are available
								 * this will be filled with Proposal: (mail_pre) and Re: (mail)
								 * automatically */
	const char *date;		/**< date string, if empty use the date of research */
	const char *icon;		/**< name of an image file to display in the mail client */
	const char *model;		/**< model name of the sender */
	qboolean read;		/**< already read the mail? */
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
	descriptions_t preDescription;	/**< Descriptions of item before it's researched. */
	researchType_t type;		/**< Defines what type this tech-entry is an where to search for other information "tech", "weapon" etc... see research.ufo for more */

	struct technology_s *redirect;	/**< Set this to the entry that is supposed to get displayed instead of this one.
									 * Mostly used for e.g ammo that doesn't need its own description but rather the one for the weapon. */

	requirements_t requireAND;	/**< A list of requirements that ALL need to be met (= AND-related) See struct above. */
	requirements_t requireOR;	/**< A list of requirements where ANY need to be met (= OR-related) See struct above. */
	qboolean statusCollected;	/**< Did we loot any items of this tech?
					 * This is updated from the info stored in the requireOR and requireAND lists.
					 * @see RS_CheckCollected. */

	char *provides;		/**< The item that this technology enables. */

	float overallTime, time;	/**< The time that is needed to research this tech. (in days).
					 * "overallTime" stays always the same,
					 * "time" will be modified when it is under research.*/
	int delay;			/**< @todo Number in days the system should wait until the tech is available for research.
					 * Starting from the first time all other dependencies have been fulfilled and
					 * counting only if a day has passed and they still are met. */

	researchStatus_t statusResearch;	/**< Current status of the research. */
	char *finishedResearchEvent;		/**< NULL or string with scriptable commands that are executed after the tech was successfully researched */

	struct base_s	*base;	/**< The base this tech is researched in. */
	int scientists;			/**< How many scientists (from "base") are researching this tech. */

	char *image;			/**< Image to display in the Ufopedia and other menus for this tech.
							 * If not set in the .ufo file this is auto-set in RS_InitTree.
							 * @sa cl_research.c: RS_InitTree */
	char *mdl;				/**< Same as "image" but it's a 3d model.. */

	qboolean statusResearchable;		/**< Is this item researchable? */

	int produceTime;			/**< How many hours the production of this items runs. */
	mailSentType_t mailSent;		/**< Store if a Mail has been sent to the commander (player). */
	requirements_t requireForProduction;	/**< A list of items that are needed (and used up) on production of _this_ item.
						 * Only "item"-type is allowed.
						 * All requirements need to be fulfilled in order for _one_ item to be produced.
						 * This check is done for each item.*/

	date_t preResearchedDate;	/**< Date for UFOpaedia. Research proposal. */
	date_t researchedDate;		/**< Date for UFOpaedia. Finished research. */

	markResearched_t markResearched;	/**< Mark as researched at parsing state - but i only know the date if we already started a campaign. */

	/* Pedia info */
	struct pediaChapter_s *upChapter;					/**< UFOpaedia chapter as stored in research.ufo. */
	struct technology_s *upPrev;	/**< Previous tech in pedia. */
	struct technology_s *upNext;	/**< Next tech in pedia. */

	techMail_t mail[TECHMAIL_MAX];	/**< UFOpaedia mails. See techMailType_t for the different array-entries. */
	int numTechMails;				/**< Used to store which "mail" entry to display in pedia. if It's equal to TECHMAIL_MAX both mailtypes are available. */

	struct technology_s *hashNext;
	struct technology_s *hashProvidedNext;
} technology_t;

#define RS_Collected_(tech) ((tech)->statusCollected)

void RS_InitStartup(void);
void RS_ResetTechs(void);
int RS_ResearchRun(void);
void RS_ParseTechnologies(const char *name, const char **text);
qboolean RS_IsResearched_idx(int techIdx);
qboolean RS_IsResearched_ptr(const technology_t *tech);

technology_t* RS_GetTechForItem(const objDef_t *item);
void RS_AddObjectTechs(void);
void RS_RequiredLinksAssign(void);
void RS_InitTree(const struct campaign_s *campaign, qboolean load);
const char *RS_GetDescription(descriptions_t *desc);
void RS_MarkCollected(technology_t *tech) __attribute__((nonnull));
void RS_MarkResearchable(qboolean init, const struct base_s *base);
qboolean RS_MarkStoryLineEventResearched(const char *techID);
void RS_ResearchFinish(technology_t* tech);
void RS_StopResearch(technology_t* tech);
void RS_MarkOneResearchable(technology_t *tech);

void RS_AssignScientist(technology_t* tech, struct base_s *base, struct employee_s *employee);
void RS_RemoveScientist(technology_t* tech, struct employee_s *employee);
void RS_RemoveFiredScientist(struct base_s *base, struct employee_s *employee);
void RS_RemoveScientistsExceedingCapacity(struct base_s *base);

technology_t *RS_GetTechByID(const char *id);
technology_t *RS_GetTechByProvided(const char *idProvided);
technology_t* RS_GetTechByIDX(int tech_idx);
technology_t *RS_GetTechWithMostScientists(const struct base_s *base);
int RS_GetTechIdxByName(const char *name);
int RS_CountScientistsInBase(const struct base_s *base);
qboolean RS_ScriptSanityCheck(void);

/* UFOpaedia function - but needs technology_t */
void UP_AircraftDescription(const technology_t* t);
void UP_UGVDescription(const struct ugv_s *ugvType);

qboolean RS_RequirementsMet(const requirements_t *requiredAND, const requirements_t *requiredOR, const struct base_s *base);

#endif /* CLIENT_CL_RESEARCH_H */
