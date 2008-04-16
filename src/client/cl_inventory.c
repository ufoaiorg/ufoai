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

static equipDef_t eTempEq;		/**< Used to count ammo in magazines. */
static int eTempCredits;		/**< Used to count temporary credits for item selling. */

const int UGV_SIZE = 300;	/**< Size of a UGV in hangar capacity */

/**
 * @brief Count and collect ammo from gun magazine.
 * @param[in] magazine Pointer to invList_t being magazine.
 * @param[in] aircraft Pointer to aircraft used in this mission.
 * @sa INV_CollectingItems
 */
static void INV_CollectingAmmo (const invList_t *magazine, aircraft_t *aircraft)
{
	int i;
	itemsTmp_t *cargo;

	cargo = aircraft->itemcargo;

	/* Let's add remaining ammo to market. */
	eTempEq.numLoose[magazine->item.m->idx] += magazine->item.a;
	if (eTempEq.numLoose[magazine->item.m->idx] >= magazine->item.t->ammo) {
		/* There are more or equal ammo on the market than magazine needs - collect magazine. */
		eTempEq.numLoose[magazine->item.m->idx] -= magazine->item.t->ammo;
		for (i = 0; i < aircraft->itemtypes; i++) {
			if (cargo[i].item == magazine->item.m) {
				cargo[i].amount++;
				Com_DPrintf(DEBUG_CLIENT, "Collecting item in INV_CollectingAmmo(): %i name: %s amount: %i\n", cargo[i].item->idx, magazine->item.m->name, cargo[i].amount);
				break;
			}
		}
		if (i == aircraft->itemtypes) {
			cargo[i].item = magazine->item.m;
			cargo[i].amount++;
			Com_DPrintf(DEBUG_CLIENT, "Adding item in INV_CollectingAmmo(): %i, name: %s\n", cargo[i].item->idx, magazine->item.m->name);
			aircraft->itemtypes++;
		}
	}
}

/**
 * @brief Process items carried by soldiers.
 * @param[in] soldier Pointer to le_t being a soldier from our team.
 * @sa INV_CollectingItems
 */
static void INV_CarriedItems (const le_t *soldier)
{
	int container;
	invList_t *item;
	technology_t *tech;

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].temp) /* Items collected as ET_ITEM in INV_CollectingItems(). */
			continue;
		for (item = soldier->i.c[container]; item; item = item->next) {
			/* Fake item. */
			assert(item->item.t);
			/* Twohanded weapons and container is left hand container. */
			/* FIXME: */
			/* assert(container == csi.idLeft && csi.ods[item->item.t].holdTwoHanded); */

			ccs.eMission.num[item->item.t->idx]++;
			tech = item->item.t->tech;
			if (!tech)
				Sys_Error("INV_CarriedItems: No tech for %s / %s\n", item->item.t->id, item->item.t->name);
			RS_MarkCollected(tech);

			if (!item->item.t->reload || item->item.a == 0)
				continue;
			ccs.eMission.numLoose[item->item.m->idx] += item->item.a;
			if (ccs.eMission.numLoose[item->item.m->idx] >= item->item.t->ammo) {
				ccs.eMission.numLoose[item->item.m->idx] -= item->item.t->ammo;
				ccs.eMission.num[item->item.m->idx]++;
			}
			/* The guys keep their weapons (half-)loaded. Auto-reload
			 * will happen at equip screen or at the start of next mission,
			 * but fully loaded weapons will not be reloaded even then. */
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
void INV_CollectingItems (int won)
{
	int i, j;
	le_t *le;
	invList_t *item;
	itemsTmp_t *cargo;
	aircraft_t *aircraft;

	aircraft = cls.missionaircraft;

	/* Make sure itemcargo is empty. */
	memset(aircraft->itemcargo, 0, sizeof(aircraft->itemcargo));

	/* Make sure eTempEq is empty as well. */
	memset(&eTempEq, 0, sizeof(eTempEq));

	cargo = aircraft->itemcargo;
	aircraft->itemtypes = 0;
	eTempCredits = ccs.credits;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		/* Winner collects everything on the floor, and everything carried
		 * by surviving actors. Loser only gets what their living team
		 * members carry. */
		if (!le->inuse)
			continue;
		switch (le->type) {
		case ET_ITEM:
			if (won) {
				for (item = FLOOR(le); item; item = item->next) {
					for (j = 0; j < aircraft->itemtypes; j++) {
						if (cargo[j].item == item->item.t) {
							cargo[j].amount++;
							Com_DPrintf(DEBUG_CLIENT, "Collecting item: %i name: %s amount: %i\n", cargo[j].item->idx, item->item.t->name, cargo[j].amount);
							/* If this is not reloadable item, or no ammo left, break... */
							if (!item->item.t->reload || item->item.a == 0)
								break;
							/* ...otherwise collect ammo as well. */
							INV_CollectingAmmo(item, aircraft);
							break;
						}
					}
					if (j == aircraft->itemtypes) {
						cargo[j].item = item->item.t;
						cargo[j].amount++;
						Com_DPrintf(DEBUG_CLIENT, "Adding item: %i name: %s\n", cargo[j].item->idx, item->item.t->name);
						aircraft->itemtypes++;
						/* If this is not reloadable item, or no ammo left, break... */
						if (!item->item.t->reload || item->item.a == 0)
							continue;
						/* ...otherwise collect ammo as well. */
						INV_CollectingAmmo(item, aircraft);
					}
				}
			}
			break;
		case ET_ACTOR:
		case ET_ACTOR2x2:
			/* First of all collect armours of dead or stunned actors (if won). */
			if (won) {
				/* (le->state & STATE_DEAD) includes STATE_STUN */
				if (le->state & STATE_DEAD) {
					if (le->i.c[csi.idArmour]) {
						item = le->i.c[csi.idArmour];
						for (j = 0; j < aircraft->itemtypes; j++) {
							if (cargo[j].item == item->item.t) {
								cargo[j].amount++;
								Com_DPrintf(DEBUG_CLIENT, "Collecting armour: %i name: %s amount: %i\n", cargo[j].item->idx, cargo[j].item->name, cargo[j].amount);
								break;
							}
						}
						if (j == aircraft->itemtypes) {
							cargo[j].item = item->item.t;
							cargo[j].amount++;
							Com_DPrintf(DEBUG_CLIENT, "Adding item: %i name: %s\n", cargo[j].item->idx, cargo[j].item->id);
							aircraft->itemtypes++;
						}
					}
					break;
				}
			}
			/* Now, if this is dead or stunned actor, additional check. */
			if (le->state & STATE_DEAD || le->team != cls.team) {
				/* The items are already dropped to floor and are available
				 * as ET_ITEM; or the actor is not ours. */
				break;
			}
			/* Finally, the living actor from our team. */
			INV_CarriedItems(le);
			break;
		default:
			break;
		}
	}
	/* Fill the missionresults array. */
	missionresults.itemtypes = aircraft->itemtypes;
	for (i = 0; i < aircraft->itemtypes; i++)
		missionresults.itemamount += cargo[i].amount;

#ifdef DEBUG
	/* Print all of collected items. */
	for (i = 0; i < aircraft->itemtypes; i++) {
		if (cargo[i].amount > 0)
			Com_DPrintf(DEBUG_CLIENT, "Collected items: idx: %i name: %s amount: %i\n", cargo[i].item->idx, cargo[i].item->name, cargo[i].amount);
	}
#endif
}

/**
 * @brief Sell items to the market or add them to base storage.
 * @param[in] aircraft Pointer to an aircraft landing in base.
 * @sa CL_AircraftReturnedToHomeBase
 */
void INV_SellOrAddItems (aircraft_t *aircraft)
{
	int i, j, numitems = 0, gained = 0, sold = 0;
	char str[128];
	itemsTmp_t *cargo;
	technology_t *tech;
	base_t *base;
	qboolean notenoughspace = qfalse;

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	eTempCredits = ccs.credits;
	cargo = aircraft->itemcargo;

	for (i = 0; i < aircraft->itemtypes; i++) {
		tech = cargo[i].item->tech;
		if (!tech)
			Sys_Error("INV_SellOrAddItems: No tech for %s / %s\n", cargo[i].item->id, cargo[i].item->name);
		/* If the related technology is NOT researched, don't sell items. */
		if (!RS_IsResearched_ptr(tech)) {
			/* Items not researched cannot be thrown out even if not enough space in storage. */
			B_UpdateStorageAndCapacity(base, cargo[i].item, cargo[i].amount, qfalse, qtrue);
			if (cargo[i].amount > 0)
				RS_MarkCollected(tech);
			continue;
		} else {
			/* If the related technology is researched, check the autosell option. */
			if (gd.autosell[cargo[i].item->idx]) { /* Sell items if autosell is enabled. */
				ccs.eMarket.num[cargo[i].item->idx] += cargo[i].amount;
				eTempCredits += (cargo[i].item->price * cargo[i].amount);
				numitems += cargo[i].amount;
			} else {
				/* Check whether there is enough space for adding this item.
				 * If yes - add. If not - sell. */
				for (j = 0; j < cargo[i].amount; j++) {
					if (!B_UpdateStorageAndCapacity(base, cargo[i].item, 1, qfalse, qfalse)) {
						/* Not enough space, sell item. */
						notenoughspace = qtrue;
						sold++;
						ccs.eMarket.num[cargo[i].item->idx]++;
						eTempCredits += cargo[i].item->price;
					}
				}
			}
			continue;
		}
	}

	gained = eTempCredits - ccs.credits;
	if (gained > 0) {
		Com_sprintf(str, sizeof(str), _("By selling %s you gathered %i credits."),
			va(ngettext("%i collected item", "%i collected items", numitems), numitems), gained);
		MN_AddNewMessage(_("Notice"), str, qfalse, MSG_STANDARD, NULL);
	}
	if (notenoughspace) {
		Com_sprintf(str, sizeof(str), _("Not enough storage space in base %s. %s"),
			base->name, va(ngettext("%i item was sold.", "%i items were sold.", sold), sold));
		MN_AddNewMessage(_("Notice"), str, qfalse, MSG_STANDARD, NULL);
	}
	CL_UpdateCredits(ccs.credits + gained);

	/* ship no longer has cargo aboard */
	aircraft->itemtypes = 0;
}

/**
 * @brief Enable autosell option.
 * @param[in] tech Pointer to newly researched technology.
 * @sa RS_MarkResearched
 */
void INV_EnableAutosell (const technology_t *tech)
{
	int i = 0, j;
	technology_t *ammotech;

	/* If the tech leads to weapon or armour, find related item and enable autosell. */
	if ((tech->type == RS_WEAPON) || (tech->type == RS_ARMOUR)) {
		for (i = 0; i < csi.numODs; i++) {
			if (!Q_strncmp(tech->provides, csi.ods[i].id, MAX_VAR)) {
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
			if (INVSH_LoadableInWeapon(&csi.ods[j], &csi.ods[i])) {
				ammotech = RS_GetTechByProvided(csi.ods[j].id);
				/* If the ammo is not producible, don't enable autosell. */
				if (ammotech && (ammotech->produceTime < 0))
					continue;
				gd.autosell[j] = qtrue;
			}
		}
	}
}

/**
 * @brief Prepares initial equipment for first base at the beginning of the campaign.
 * @param[in] base Pointer to first base.
 * @param[in] campaign The current running campaign
 * @sa B_BuildBase_f
 * @todo Make sure all equipment including soldiers equipment is added to capacity.cur.
 */
void INV_InitialEquipment (base_t *base, const campaign_t* campaign, qboolean assignInitial)
{
	int i, price = 0;
	const equipDef_t *ed;
	const char *eqname = cl_initial_equipment->string;

	assert(campaign);
	assert(base);

	/* Find the initial equipment definition for current campaign. */
	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++) {
		if (!Q_strncmp(campaign->equipment, ed->name, sizeof(campaign->equipment)))
			break;
	}

	/* Copy it to base storage. */
	if (i != csi.numEDs)
		base->storage = *ed;

	/* Initial soldiers and their equipment. */
	if (assignInitial) {
		Cbuf_AddText("assign_initial;");
	} else {
		for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++) {
			if (!Q_strncmp(eqname, ed->name, MAX_VAR))
				break;
		}
		if (i == csi.numEDs) {
			Com_DPrintf(DEBUG_CLIENT, "B_BuildBase_f: Initial Phalanx equipment %s not found.\n", eqname);
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
 * @sa CL_ParseScriptFirst
 */
void INV_ParseComponents (const char *name, const char **text)
{
	components_t *comp;
	const char *errhead = "INV_ParseComponents: unexpected end of file.";
	const char *token;

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
	Q_strncpyz(comp->asId, name, sizeof(comp->asId));
	comp->asItem = INVSH_GetItemByID(comp->asId);
	Com_DPrintf(DEBUG_CLIENT, "INV_ParseComponents: linked item: %s with components: %s\n", name, comp->asId);

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

				comp->items[comp->numItemtypes] = INVSH_GetItemByID(token);	/* item id -> item pointer */

				/* Parse number of items. */
				token = COM_Parse(text);
				comp->item_amount[comp->numItemtypes] = atoi(token);
				token = COM_Parse(text);
				comp->item_amount2[comp->numItemtypes] = atoi(token);

				/** @todo Set item links to NONE if needed */
				/* comp->item_idx[comp->numItemtypes] = xxx */

				comp->numItemtypes++;
			} else {
				Com_Printf("INV_ParseComponents: \"%s\" Too many 'items' defined. Limit is %i - ignored.\n", name, MAX_COMP);
			}
		} else if (!Q_strncmp(token, "time", MAX_VAR)) {
			/* Defines how long disassembly lasts. */
			token = COM_Parse(text);
			comp->time = atoi(token);
		} else {
			Com_Printf("INV_ParseComponents: Error in \"%s\" - unknown token: \"%s\".\n", name, token);
		}
	} while (*text);
}

/**
 * @brief Returns components definition for an item.
 * @param[in] item Item to search the components for.
 * @return comp Pointer to components_t definition.
 */
components_t *INV_GetComponentsByItem (const objDef_t *item)
{
	int i;

	for (i = 0; i < gd.numComponents; i++) {
		components_t *comp = &gd.components[i];
		if (comp->asItem == item) {
			Com_DPrintf(DEBUG_CLIENT, "INV_GetComponentsByItem()... found components id: %s\n", comp->asId);
			return comp;
		}
	}
	Sys_Error("INV_GetComponentsByItem()... could not find components id for: %s", item->id);
	return NULL;
}

/**
 * @brief Disassembles item, adds components to base storage and calculates all components size.
 * @param[in] base Pointer to base where the disassembling is being made.
 * @param[in] comp Pointer to components definition.
 * @param[in] calculate True if this is only calculation of item size, false if this is real disassembling.
 * @return Size of all components in this disassembling.
 */
int INV_DisassemblyItem (base_t *base, components_t *comp, qboolean calculate)
{
	int i, size = 0;

	assert(comp);
	if (!calculate && !base)	/* We need base only if this is real disassembling. */
		Sys_Error("INV_DisassemblyItem: No base given");

	for (i = 0; i < comp->numItemtypes; i++) {
		const objDef_t *compOd = comp->items[i];
		assert(compOd);
		size += compOd->size * comp->item_amount[i];
		/* Add to base storage only if this is real disassembling, not calculation of size. */
		if (!calculate) {
			if (!Q_strncmp(compOd->id, "antimatter", 10))
				INV_ManageAntimatter(base, comp->item_amount[i], qtrue);
			else
				B_UpdateStorageAndCapacity(base, compOd, comp->item_amount[i], qfalse, qfalse);
			Com_DPrintf(DEBUG_CLIENT, "INV_DisassemblyItem()... added %i amounts of %s\n", comp->item_amount[i], compOd->id);
		}
	}
	return size;
}

/**
 * @brief Update Storage Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
void INV_UpdateStorageCap (base_t *base)
{
	int i;

	base->capacities[CAP_ITEMS].cur = 0;

	for (i = 0; i < csi.numODs; i++) {
		/* don't count antimatter */
		if (!Q_strncmp(csi.ods[i].id, "antimatter", 10))
			continue;

		/* Don't count aircraft */
		assert(csi.ods[i].tech);
		if (csi.ods[i].tech->type == RS_CRAFT) {
			continue;
		}

		base->capacities[CAP_ITEMS].cur += base->storage.num[i] * csi.ods[i].size;
	}

	/* UGV takes room in storage capacity */
	base->capacities[CAP_ITEMS].cur += UGV_SIZE * E_CountHired(base, EMPL_ROBOT);
}

/**
 * @brief Update Antimatter Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
void INV_UpdateAntimatterCap (base_t *base)
{
	int i;

	for (i = 0; i < csi.numODs; i++) {
		if (!Q_strncmp(csi.ods[i].id, "antimatter", 10)) {
			base->capacities[CAP_ANTIMATTER].cur = (base->storage.num[i] * ANTIMATTER_SIZE);
			return;
		}
	}
}

/**
 * @brief Manages Antimatter (adding, removing) through Antimatter Storage Facility.
 * @param[in] base Pointer to the base.
 * @param[in] amount quantity of antimatter to add/remove (> 0 even if antimatter is removed)
 * @param[in] add True if we are adding antimatter, false when removing.
 * @note This function should be called whenever we add or remove antimatter from Antimatter Storage Facility.
 * @note Call with amount = 0 if you want to remove ALL antimatter from given base.
 */
void INV_ManageAntimatter (base_t *base, int amount, qboolean add)
{
	int i, j;
	objDef_t *od;

	assert(base);

	if (!B_GetBuildingStatus(base, B_ANTIMATTER) && add) {
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer),
			_("Base %s does not have Antimatter Storage Facility. %i units of Antimatter got removed."),
			base->name, amount);
		MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
		return;
	}

	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		if (!Q_strncmp(od->id, "antimatter", 10))
			break;
	}
	assert(od);

	if (add) {	/* Adding. */
		if (base->capacities[CAP_ANTIMATTER].cur + (amount * ANTIMATTER_SIZE) <= base->capacities[CAP_ANTIMATTER].max) {
			base->storage.num[i] += amount;
			base->capacities[CAP_ANTIMATTER].cur += (amount * ANTIMATTER_SIZE);
		} else {
			for (j = 0; j < amount; j++) {
				if (base->capacities[CAP_ANTIMATTER].cur + ANTIMATTER_SIZE <= base->capacities[CAP_ANTIMATTER].max) {
					base->storage.num[i]++;
					base->capacities[CAP_ANTIMATTER].cur += ANTIMATTER_SIZE;
				} else
					break;
			}
		}
	} else {	/* Removing. */
		if (amount == 0) {
			base->capacities[CAP_ANTIMATTER].cur = 0;
			base->storage.num[i] = 0;
		} else {
			base->capacities[CAP_ANTIMATTER].cur -= amount * ANTIMATTER_SIZE;
			base->storage.num[i] -= amount;
		}
	}
}

#ifdef DEBUG
/**
 * @brief Lists all object definitions.
 * @note called with debug_listinventory
 */
void INV_InventoryList_f (void)
{
	int i;

	for (i = 0; i < csi.numODs; i++)
		INVSH_PrintItemDescription(&csi.ods[i]);
}
#endif

/**
 * @param[in] base the base all this happens into
 * @param[in] inv list where the item is currently in
 * @param[in] toContainer target container to place the item in
 * @param[in] px target x position in the toContainer container
 * @param[in] py target y position in the toContainer container
 * @param[in] fromContainer Container the item is in
 * @param[in] fromX X position of the item to move (in container fromContainer)
 * @param[in] fromY y position of the item to move (in container fromContainer)
 * @note If you set px or py to -1 the item is automatically placed on a free
 * spot in the targetContainer
 * @return qtrue if the move was successful
 */
qboolean INV_MoveItem (base_t* base, inventory_t* inv, int toContainer, int px, int py,
	int fromContainer, int fromX, int fromY)
{
	invList_t *i = NULL;
	int et = -1;
	int moved;

	assert(base);

	if (px >= SHAPE_BIG_MAX_WIDTH || py >= SHAPE_BIG_MAX_HEIGHT)
		return qfalse;

	/* FIXME / @todo: this case should be removed as soon as right clicking in equip container
	 * will try to put the item in a reasonable container automatically */
	if ((px == -1 || py == -1) && toContainer == fromContainer)
		return qtrue;

	if (toContainer == csi.idEquip) {
		/* a hack to add the equipment correctly into buy categories;
		 * it is valid only due to the following property: */
		assert(MAX_CONTAINERS >= BUY_AIRCRAFT);

		i = Com_SearchInInventory(inv, fromContainer, fromX, fromY);
		if (i) {
			et = i->item.t->buytype;
			if (et == BUY_MULTI_AMMO) {
				et = (base->equipType == BUY_WEAP_SEC)
					? BUY_WEAP_SEC
					: BUY_WEAP_PRI;
			}

			/* If the 'to'-container is not the one that is currently shown or auto-placement is wanted ...*/
			if (!BUYTYPE_MATCH(et, base->equipType) || px == -1 || py == -1) {
				inv->c[csi.idEquip] = base->equipByBuyType.c[et];
				Com_FindSpace(inv, &i->item, csi.idEquip, &px, &py);
				if (px >= SHAPE_BIG_MAX_WIDTH && py >= SHAPE_BIG_MAX_HEIGHT) {
					inv->c[csi.idEquip] = base->equipByBuyType.c[base->equipType];
					return qfalse;
				}
			}
		}
	}

	/* move the item */
	moved = Com_MoveInInventory(inv, fromContainer, fromX, fromY, toContainer, px, py, NULL, NULL);

	if (moved == IA_RELOAD_SWAP) {
		/**
		 * The ammo in the target-weapon was swapped out of it and added to the current container.
		 * If the container is of type idEquip and the last added (by the swap) item in it has a wrong buytype then move it to the correct one.
		 * Currently it assumes that there is only ONE item of this type (since weapons can only use one ammo which is swapped out).
		 * @sa Com_MoveInInventoryIgnore (return IA_RELOAD_SWAP)
		 */
		if (fromContainer == csi.idEquip) {
			/* Check buytype of first (last added) item */
			if (!BUYTYPE_MATCH(et, base->equipType)) {
				item_t item_temp = {NONE_AMMO, NULL, inv->c[csi.idEquip]->item.t, 0, 0};
				et = inv->c[csi.idEquip]->item.t->buytype;
				if (BUY_PRI(et)) {
					Com_RemoveFromInventory(inv, fromContainer, inv->c[csi.idEquip]->x, inv->c[csi.idEquip]->y);
					Com_TryAddToBuyType(&base->equipByBuyType, item_temp, BUY_WEAP_PRI, 1);
				} else if (BUY_SEC(et)) {
					Com_RemoveFromInventory(inv, fromContainer, inv->c[csi.idEquip]->x, inv->c[csi.idEquip]->y);
					Com_TryAddToBuyType(&base->equipByBuyType, item_temp, BUY_WEAP_SEC, 1);
				}
			}
		}
	}

	/* end of hack */
	if (i && !BUYTYPE_MATCH(et, base->equipType)) {
		base->equipByBuyType.c[et] = inv->c[csi.idEquip];
		inv->c[csi.idEquip] = base->equipByBuyType.c[base->equipType];
	} else {
		base->equipByBuyType.c[base->equipType] = inv->c[csi.idEquip];
	}

	switch (moved) {
	case IA_MOVE:
	case IA_ARMOUR:
		return qtrue;
	default:
		return qfalse;
	}
}
