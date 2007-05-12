/**
 * @file cl_inventory.c
 * @brief Actor related inventory function.
 * @note Inventory functions prefix: INV_
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

#include "client.h"
#include "cl_global.h"

/**
 * Collecting items functions.
 */

static equipDef_t eTempEq;	/**< Used to count ammo in magazines. */
static int eTempCredits;	/**< Used to count temporary credits for item selling. */

/**
 * @brief Count and collect ammo from gun magazine.
 * @param[in] *magazine Pointer to invList_t being magazine.
 * @param[in] *aircraft Pointer to aircraft used in this mission.
 * @sa INV_CollectingItems
 */
static void INV_CollectingAmmo (invList_t *magazine, aircraft_t *aircraft)
{
	int i;
	itemsTmp_t *cargo = NULL;

	if (!aircraft) {
#ifdef DEBUG
		/* should never happen */
		Com_Printf("INV_CollectingAmmo()... no aircraft!\n");
#endif
		return;
	} else {
		cargo = aircraft->itemcargo;
	}

	/* Let's add remaining ammo to market. */
	eTempEq.num_loose[magazine->item.m] += magazine->item.a;
	if (eTempEq.num_loose[magazine->item.m] >= csi.ods[magazine->item.t].ammo) {
		/* There are more or equal ammo on the market than magazine needs - collect magazine. */
		eTempEq.num_loose[magazine->item.m] -= csi.ods[magazine->item.t].ammo;
		for (i = 0; i < aircraft->itemtypes; i++) {
			if (cargo[i].idx == magazine->item.m) {
				cargo[i].amount++;
				Com_DPrintf("Collecting item in INV_CollectingAmmo(): %i name: %s amount: %i\n", cargo[i].idx, csi.ods[magazine->item.m].name, cargo[i].amount);
				break;
			}
		}
		if (i == aircraft->itemtypes) {
			cargo[i].idx = magazine->item.m;
			cargo[i].amount++;
			Com_DPrintf("Adding item in INV_CollectingAmmo(): %i, name: %s\n", cargo[i].idx, csi.ods[magazine->item.m].name);
			aircraft->itemtypes++;
		}
	}
}

/**
 * @brief Process items carried by soldiers.
 * @param[in] *soldier Pointer to le_t being a soldier from our team.
 * @sa INV_CollectingItems
 */
static void INV_CarriedItems (le_t *soldier)
{
	int container;
	invList_t *item = NULL;
	technology_t *tech = NULL;

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].temp) /* Items collected as ET_ITEM in INV_CollectingItems(). */
			break;
		for (item = soldier->i.c[container]; item; item = item->next) {
			/* Fake item. */
			assert(item->item.t != NONE);
			/* Twohanded weapons and container is left hand container. */
			/* FIXME: */
			/* assert(container == csi.idLeft && csi.ods[item->item.t].holdtwohanded); */

			ccs.eMission.num[item->item.t]++;
			tech = csi.ods[item->item.t].tech;
			if (!tech)
				Sys_Error("INV_CarriedItems: No tech for %s / %s\n", csi.ods[item->item.t].id, csi.ods[item->item.t].name);
			RS_MarkCollected(tech);

			if (!csi.ods[item->item.t].reload || item->item.a == 0)
				continue;
			ccs.eMission.num_loose[item->item.m] += item->item.a;
			if (ccs.eMission.num_loose[item->item.m] >= csi.ods[item->item.t].ammo) {
				ccs.eMission.num_loose[item->item.m] -= csi.ods[item->item.t].ammo;
				ccs.eMission.num[item->item.m]++;
			}
			/* The guys keep their weapons (half-)loaded. Auto-reload
			   will happen at equip screen or at the start of next mission,
			   but fully loaded weapons will not be reloaded even then. */
		}
	}
}

/**
 * @brief Collect items from the battlefield.
 * @note The way we are collecting items:
 * @note 1. Grab everything from the floor and add it to the aircraft cargo here.
 * @note 2. When collecting gun, collect it's remaining ammo as well in CL_CollectingAmmo
 * @note 3. Sell everything or add to base storage in CL_SellingAmmo, when dropship lands in base.
 * @note 4. Items carried by soldiers are processed in CL_CarriedItems, nothing is being sold.
 * @sa CL_ParseResults
 * @sa INV_CollectingAmmo
 * @sa INV_SellorAddAmmo
 * @sa INV_CarriedItems
 * @todo remove baseCurrent from this function - replace with aircraft->homebase
 */
extern void INV_CollectingItems (int won)
{
	int i, j;
	le_t *le;
	invList_t *item;
	itemsTmp_t *cargo;
	aircraft_t *aircraft;

	if (!baseCurrent) {
#ifdef DEBUG
		/* should never happen */
		Com_Printf("INV_CollectingItems()... no base selected!\n");
#endif
		return;
	}
	if ((baseCurrent->aircraftCurrent >= 0) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase)) {
		aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	} else {
#ifdef DEBUG
		/* should never happen */
		Com_Printf("INV_CollectingItems()... no aircraft selected!\n");
#endif
		return;
	}

	/* Make sure itemcargo is empty. */
	memset(aircraft->itemcargo, 0, sizeof(aircraft->itemcargo));

	/* Make sure eTempEq is empty as well. */
	memset(&eTempEq, 0, sizeof(eTempEq));

	cargo = aircraft->itemcargo;
	aircraft->itemtypes = 0;
	eTempCredits = ccs.credits;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		/* Winner collects everything on the floor, and everything carried
		   by surviving actors. Loser only gets what their living team
		   members carry. */
		if (!le->inuse)
			continue;
		switch (le->type) {
		case ET_ITEM:
			if (won) {
				for (item = FLOOR(le); item; item = item->next) {
					if ((item->item.a <= 0) /* No ammo left, and... */
					&& csi.ods[item->item.t].oneshot /* ... oneshot weapon, and... */
					&& csi.ods[item->item.t].deplete) { /* ... useless after ammo is gone. */
						Com_DPrintf("INV_CollectItems: depletable item not collected: %s\n", csi.ods[item->item.t].name);
						continue;
					}
					for (j = 0; j < aircraft->itemtypes; j++) {
						if (cargo[j].idx == item->item.t) {
							cargo[j].amount++;
							Com_DPrintf("Collecting item: %i name: %s amount: %i\n", cargo[j].idx, csi.ods[item->item.t].name, cargo[j].amount);
							/* If this is not reloadable item, or no ammo left, break... */
							if (!csi.ods[item->item.t].reload || item->item.a == 0)
								break;
							/* ...otherwise collect ammo as well. */
							INV_CollectingAmmo(item, aircraft);
							break;
						}
					}
					if (j == aircraft->itemtypes) {
						cargo[j].idx = item->item.t;
						cargo[j].amount++;
						Com_DPrintf("Adding item: %i name: %s\n", cargo[j].idx, csi.ods[item->item.t].name);
						aircraft->itemtypes++;
						/* If this is not reloadable item, or no ammo left, break... */
						if (!csi.ods[item->item.t].reload || item->item.a == 0)
							continue;
						/* ...otherwise collect ammo as well. */
						INV_CollectingAmmo(item, aircraft);
					}
				}
			}
			break;
		case ET_ACTOR:
		case ET_UGV:
			/* First of all collect armours of dead or stunned actors (if won). */
			if (won) {
				/* (le->state & STATE_DEAD) includes STATE_STUN */
				if (le->state & STATE_DEAD) {
					if (le->i.c[csi.idArmor]) {
						item = le->i.c[csi.idArmor];
						for (j = 0; j < aircraft->itemtypes; j++) {
							if (cargo[j].idx == item->item.t) {
								cargo[j].amount++;
								Com_DPrintf("Collecting armour: %i name: %s amount: %i\n", cargo[j].idx, csi.ods[item->item.t].name, cargo[j].amount);
								break;
							}
						}
						if (j == aircraft->itemtypes) {
							cargo[j].idx = item->item.t;
							cargo[j].amount++;
							Com_DPrintf("Adding item: %i name: %s\n", cargo[j].idx, csi.ods[item->item.t].name);
							aircraft->itemtypes++;
						}
					}
					break;
				}
			}
			/* Now, if this is dead or stunned actor, additional check. */
			if (le->state & STATE_DEAD || le->team != cls.team) {
				/* The items are already dropped to floor and are available
				   as ET_ITEM; or the actor is not ours. */
				break;
			}
			/* Finally, the living actor from our team. */
			INV_CarriedItems(le);
			break;
		default:
			break;
		}
	}
#ifdef DEBUG
	/* Print all of collected items. */
	for (i = 0; i < aircraft->itemtypes; i++) {
		if (cargo[i].amount > 0)
			Com_DPrintf("Collected items: idx: %i name: %s amount: %i\n", cargo[i].idx, csi.ods[cargo[i].idx].name, cargo[i].amount);
	}
#endif
}

/**
 * @brief Sell items to the market or add them to base storage.
 * @param[in] *aircraft Pointer to an aircraft landing in base.
 * @sa CL_DropshipReturned
 */
void INV_SellOrAddItems (aircraft_t *aircraft)
{
	int i, j, numitems = 0, gained = 0, sold = 0;
	char str[128];
	itemsTmp_t *cargo = NULL;
	technology_t *tech = NULL;
	base_t *base;
	qboolean notenoughspace = qfalse;

	assert (aircraft);
	base = aircraft->homebase;
	assert (base);

	eTempCredits = ccs.credits;
	cargo = aircraft->itemcargo;

	for (i = 0; i < aircraft->itemtypes; i++) {
		tech = csi.ods[cargo[i].idx].tech;
		if (!tech)
			Sys_Error("INV_SellOrAddItems: No tech for %s / %s\n", csi.ods[cargo[i].idx].id, csi.ods[cargo[i].idx].name);
		/* If the related technology is NOT researched, don't sell items. */
		if (!RS_IsResearched_ptr(tech)) {
			base->storage.num[cargo[i].idx] += cargo[i].amount;
			/* Items not researched cannot be thrown out even if not enough space in storage. */
			base->capacities[CAP_ITEMS].cur += (cargo[i].amount * csi.ods[cargo[i].idx].size);
			if (cargo[i].amount > 0)
				RS_MarkCollected(tech);
			continue;
		}
		/* If the related technology is researched, check the autosell option. */
		if (RS_IsResearched_ptr(tech)) {
			if (gd.autosell[cargo[i].idx]) { /* Sell items if autosell is enabled. */
				ccs.eMarket.num[cargo[i].idx] += cargo[i].amount;
				eTempCredits += (csi.ods[cargo[i].idx].price * cargo[i].amount);
				numitems += cargo[i].amount;
			} else {
				/* Check whether there is enough space for adding this item. */
				/* If yes - add. If not - sell. */
				for (j = 0; j < cargo[i].amount; j++) {
					if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur >= csi.ods[cargo[i].idx].size) {
						/* There is a space, add to base storage and increase capacity. */
						base->storage.num[cargo[i].idx]++;
						base->capacities[CAP_ITEMS].cur += csi.ods[cargo[i].idx].size;
					} else {
						/* Not enough space, sell item. */
						notenoughspace = qtrue;
						sold++;
						ccs.eMarket.num[cargo[i].idx]++;
						eTempCredits += csi.ods[cargo[i].idx].price;
					}
				}
				
			}
			continue;
		}
	}

	gained = eTempCredits - ccs.credits;
	if (gained > 0) {
		Com_sprintf(str, sizeof(str), _("By selling %s %s"),
		va(ngettext("%i collected item", "%i collected items", numitems), numitems),
		va(_("you gathered %i credits."), gained));
		MN_AddNewMessage(_("Notice"), str, qfalse, MSG_STANDARD, NULL);
	}
	if (notenoughspace) {
		Com_sprintf(str, sizeof(str), _("Not enough storage space in base %s. %s"),
		base->name,
		va(ngettext("%i item was sold.", "%i items were sold.", sold), sold));
		MN_AddNewMessage(_("Notice"), str, qfalse, MSG_STANDARD, NULL);
	}
	CL_UpdateCredits(ccs.credits + gained);
}

/**
 * @brief Enable autosell option.
 * @param[in] *tech Pointer to newly researched technology.
 * @sa RS_MarkResearched
 */
void INV_EnableAutosell (technology_t *tech)
{
	int i = 0, j;
	technology_t *ammotech = NULL;

	/* If the tech leads to weapon or armour, find related item and enable autosell. */
	if ((tech->type == RS_WEAPON) || (tech->type == RS_ARMOR)) {
		for (i = 0; i < csi.numODs; i++) {
			if (Q_strncmp(tech->provides, csi.ods[i].id, MAX_VAR) == 0) {
				gd.autosell[i] = qtrue;
				break;
			}
		}
		if (i == csi.numODs)
			return;
	}

	/* If the weapon has ammo, enable autosell for proper ammo as well. */
	if ((tech->type == RS_WEAPON) && (csi.ods[i].reload)) {
		for (j = 0; j < csi.numODs; j++) {
			/* Find all suitable ammos for this weapon. */
			if (INV_LoadableInWeapon(&csi.ods[j], i)) {
				ammotech = RS_GetTechByProvided(csi.ods[j].id);
				/* If the ammo is not produceable, don't enable autosell. */
				if (ammotech && (ammotech->produceTime < 0))
					continue;
				gd.autosell[j] = qtrue;
			}
		}
	}
}

/**
 * @brief Prepares initial equipment for first base at the beginning of the campaign.
 * @param[in] *base Pointer to first base.
 * @sa B_BuildBase_f
 * @todo Make sure all equipment including soldiers equipment is added to capacity.cur.
 * @todo Make sure that we do not pay double for items here and in B_PackInitialEquipment_f().
 * @todo Remove curCampaign pointer usage here. Send campaign idx as param[in] instead.
 */
extern void INV_InitialEquipment (base_t *base)
{
	int i, price = 0;
	equipDef_t *ed;
	const char *eqname = cl_initial_equipment->string;
	
	/* Find the initial equipment definition for current campaign. */
	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++) {
		if (!Q_strncmp(curCampaign->equipment, ed->name, sizeof(curCampaign->equipment)))
			break;
	}
	
	/* Copy it to base storage. */
	if (i != csi.numEDs)
		base->storage = *ed;
	
	/* Initial soldiers and their equipment. */
	if (cl_start_employees->value) {
		Cbuf_AddText("assign_initial;");
	} else {
		for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++) {
			if (!Q_strncmp(eqname, ed->name, MAX_VAR))
				break;
		}
		if (i == csi.numEDs) {
			Com_DPrintf("B_BuildBase_f: Initial Phalanx equipment %s not found.\n", eqname);
		} else {
			for (i = 0; i < csi.numODs; i++)
				base->storage.num[i] += ed->num[i] / 5;
		}
	}
	
	/* Pay for the initial equipment as well as update storage capacity. */
	for (i = 0; i < csi.numODs; i++) {
		price += base->storage.num[i] * csi.ods[i].price;
		base->capacities[CAP_ITEMS].cur += base->storage.num[i] * csi.ods[i].size;
	}
	/* Finally update credits. */
	CL_UpdateCredits(ccs.credits - price);
}

/**
 * @brief Parses one "components" entry in a .ufo file and writes it into the next free entry in xxxxxxxx (components_t).
 * @param[in] name The unique id of a components_t array entry.
 * @param[in] text the whole following text after the "components" definition.
 */
extern void INV_ParseComponents (const char *name, char **text)
{
	components_t *comp = NULL;
	const char *errhead = "INV_ParseComponents: unexpected end of file.";
	char *token = NULL;

	/* get body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("INV_ParseComponents: \"%s\" components def without body ignored.\n", name);
		return;
	}
	if (gd.numComponents >= MAX_ASSEMBLIES) {
		Com_Printf("INV_ParseComponents: too many technology entries. limit is %i.\n", MAX_ASSEMBLIES);
		return;
	}

	/* New components-entry (next free entry in global comp-list) */
	comp = &gd.components[gd.numComponents];
	gd.numComponents++;

	memset(comp, 0, sizeof(components_t));

	/* set standard values */
	/** comp->assembly_idx = -1; < @todo if needed */
	Q_strncpyz(comp->assembly_id, name, sizeof(comp->assembly_id));
	
	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* get values */
		if (!Q_strncmp(token, "item", MAX_VAR)) {
			/* Defines what items need to be collected for this item to be researchable. */
			if (comp->numItemtypes < MAX_COMP) {
				/* Parse item name */
				token = COM_Parse(text);
				Q_strncpyz(comp->item_id[comp->numItemtypes], token, sizeof(comp->item_id[comp->numItemtypes]));
				
				/* Parse number of items. */
				token = COM_Parse(text);
				comp->item_amount[comp->numItemtypes] = atoi(token);
				token = COM_Parse(text);
				comp->item_amount2[comp->numItemtypes] = atoi(token);
				
				/** @todo Set item links to -1 if needed */
				/* comp->item_idx[comp->numItemtypes] = xxx */
				
				comp->numItemtypes++;
			} else {
				Com_Printf("INV_ParseComponents: \"%s\" Too many 'items' defined. Limit is %i - ignored.\n", name, MAX_COMP);
			}
		} else {
			Com_Printf("INV_ParseComponents error in \"%s\" - unknown token: \"%s\".\n", name, token);
		}
	} while (*text);
}

/**
 * @brief Links components idx in gd.components[] by object id.
 * @sa CL_InitAfter()
 */
void INV_LinkComponentsWithObj (void)
{
	int i, j;
	objDef_t *od = NULL;

	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		for (j = 0; j < MAX_ASSEMBLIES; j++) {
			if ((Q_strncmp(od->id, gd.components[j].assembly_id, MAX_VAR)) == 0) {
				gd.components[j].assembly_idx = i;
				Com_DPrintf("INV_LinkComponentsWithObj()... linked item: %s idx %i with components: %s idx %i \n", od->id, i, gd.components[j].assembly_id, gd.components[j].assembly_idx);
				break;
			}
		}
	}
}

