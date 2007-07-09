/**
 * @file cl_transfer.c
 * @brief Deals with the Transfer stuff.
 * @note Transfer menu functions prefix: TR_
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

/** @brief Current selected base for transfer. */
static base_t *transferBase = NULL;

/** @brief Current selected aircraft. */
static aircraft_t *transferAircraft = NULL;

/** @brief Current transfer type (item, employee, alien, aircraft). */
static int transferType = -1;

/** @brief Current cargo onboard. */
static transferCargo_t cargo[MAX_CARGO];

/**
 * @brief Display cargo list.
 */
static void TR_CargoList (void)
{
	int i, cnt = 0;
	static char cargoList[1024];
	transferlist_t *transferidx = NULL;
	char str[128];

	cargoList[0] = '\0';
	memset(&cargo, 0, sizeof(cargo));

	if (!transferAircraft) {
		/* Needed to clear cargo list. */
		menuText[TEXT_CARGO_LIST] = cargoList;
		return;
	}

	if (!baseCurrent)
		return;

	/* Check the transfer index in gd.alltransfers array. */
	transferidx = &gd.alltransfers[transferAircraft->idx];

	if (transferidx->type == TR_STUFF) {
		/* Show items. */
		for (i = 0; i < csi.numODs; i++) {
			if (transferidx->itemAmount[i] > 0) {
				Com_sprintf(str, sizeof(str), _("%s (%i on board)\n"),
				csi.ods[i].name, transferidx->itemAmount[i]);
				Q_strcat(cargoList, str, sizeof(cargoList));
				cargo[cnt].type = 1;
				cargo[cnt].itemidx = i;
				cnt++;
			}
		}
		/* Show employees. */
		/*    @todo     */
		/* Show aliens. */
		for (i = 0; i < numTeamDesc; i++) {
			if (transferidx->alienBodyAmount[i] > 0) {
				Com_sprintf(str, sizeof(str), _("Corpse of %s (%i on board)\n"),
				_(AL_AlienTypeToName(i)), transferidx->alienBodyAmount[i]);
				Q_strcat(cargoList, str, sizeof(cargoList));
				cargo[cnt].type = 3;
				cargo[cnt].itemidx = i;
				cnt++;
			}
		}
		for (i = 0; i < numTeamDesc; i++) {
			if (transferidx->alienLiveAmount[i] > 0) {
				Com_sprintf(str, sizeof(str), _("%s (%i on board)\n"),
				_(AL_AlienTypeToName(i)), transferidx->alienLiveAmount[i]);
				Q_strcat(cargoList, str, sizeof(cargoList));
				cargo[cnt].type = 4;
				cargo[cnt].itemidx = i;
				cnt++;
			}
		}
	} else {
		/* @todo: show aircraft info here. */
	}

	menuText[TEXT_CARGO_LIST] = cargoList;
}

/**
 * @brief Add one item to transferlist.
 * @note Filling the transfer list with proper stuff (items/employees/aliens/aircrafts) is being done here.
 * @sa TR_TransferStart_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferSelect_f (void)
{
	static char transferList[1024];
	int type;
	int i, cnt = 0;
	char str[128];
	transferlist_t *transferidx = NULL;

	if (!transferBase)
		return;

	if (Cmd_Argc() < 2)
		type = transferType;
	else
		type = atoi(Cmd_Argv(1));

	if (transferAircraft)
		transferidx = &gd.alltransfers[transferAircraft->idx];

	if (transferidx)
		transferidx->destBase = transferBase->idx;

	transferList[0] = '\0';

	switch (type) { /**< 0 - items, 1 - employees, 2 - aliens, 3 - aircrafts */
	case 0:		/**< items */
		/* @todo: check building status here as well as limits. */
		if (transferBase->hasStorage) {
			for (i = 0; i < csi.numODs; i++)
				if (baseCurrent->storage.num[i]) {
					if (transferidx && (transferidx->itemAmount[i] > 0))
						Com_sprintf(str, sizeof(str), _("%s (%i on board, %i left)\n"), csi.ods[i].name, transferidx->itemAmount[i], baseCurrent->storage.num[i]);
					else
						Com_sprintf(str, sizeof(str), _("%s (%i available)\n"), csi.ods[i].name, baseCurrent->storage.num[i]);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
			if (!cnt)
				Q_strncpyz(transferList, _("Storage is empty.\n"), sizeof(transferList));
		} else {
			/* @todo: print proper info if base has storage but not operational or limits are not sufficient. */
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have a storage building."), sizeof(transferList));
		}
		break;
	case 1:		/**< humans */
		/* @todo: check building status here as well as limits. */
		if (transferBase->hasQuarters)
			Q_strcat(transferList, "@todo: employees", sizeof(transferList));
		else
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have quarters"), sizeof(transferList));
		break;
	case 2:		/**< aliens */
		/* @todo: check building status here as well as limits. */
		if (transferBase->hasAlienCont) {
			for (i = 0; i < numTeamDesc; i++) {
				if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_dead > 0) {
					if (transferidx && (transferidx->alienBodyAmount[i] > 0))
						Com_sprintf(str, sizeof(str), _("Corpse of %s (%i on board, %i left)\n"),
						_(AL_AlienTypeToName(i)), transferidx->alienBodyAmount[i],
						baseCurrent->alienscont[i].amount_dead);
					else
						Com_sprintf(str, sizeof(str), _("Corpse of %s (%i available)\n"),
						_(AL_AlienTypeToName(i)), baseCurrent->alienscont[i].amount_dead);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
				if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_alive > 0) {
					if (transferidx && (transferidx->alienLiveAmount[i] > 0))
						Com_sprintf(str, sizeof(str), _("Alive %s (%i on board, %i left)\n"),
						_(AL_AlienTypeToName(i)), transferidx->alienLiveAmount[i],
						baseCurrent->alienscont[i].amount_alive);
					else
						Com_sprintf(str, sizeof(str), _("Alive %s (%i available)\n"),
						_(AL_AlienTypeToName(i)), baseCurrent->alienscont[i].amount_alive);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
			}
			if (!cnt)
				Q_strncpyz(transferList, _("Alien Containment is empty.\n"), sizeof(transferList));
		} else {
			/* @todo: print proper info if base has AC but not operational or limits are not sufficient. */
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have an alien containment"), sizeof(transferList));
		}
		break;
	case 3:			/**< aircrafts */
		/* @todo: check building status here as well as limits. */
		if (transferBase->hasHangar) {
			Q_strcat(transferList, "@todo: aircrafts", sizeof(transferList));
		} else {
			/* @todo: print proper info if base has hangar but not operational or limits are not sufficient. */
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have hangar"), sizeof(transferList));
		}
		break;
	default:
		Com_Printf("TR_TransferSelect_f: Unknown type id %i\n", type);
		return;
	}

	/* Update cargo list. */
	TR_CargoList();

	transferType = type;
	menuText[TEXT_TRANSFER_LIST] = transferList;
}

/**
 * @brief Unload everything from aircraft storage back to base storage.
 * @note This is being taken as a normal transfer mission, but we are
 * @note just unloading the cargo in homebase, without doing anything.
 */
static void TR_TransferEmptyAircraftStorage_f (void)
{
	transferlist_t *transferidx = NULL;

	if (!transferAircraft)
		return;

	if (!baseCurrent)
		return;

	transferidx = &gd.alltransfers[transferAircraft->idx];

	/* We can't unload if we are not in our homebase. */
	if (transferAircraft->status != AIR_HOME) {
		MN_Popup(_("Notice"), _("Can't unload while not at a base"));
		return;
	}

	/* Change destination in transferidx to current base. */
	transferidx->destBase = baseCurrent->idx;
	/* To pass the sanity check in TR_TransferEnd(). */
	transferAircraft->status = AIR_TRANSPORT;
	/* Now unload the cargo. */
	TR_TransferEnd(transferAircraft);
	transferAircraft->status = AIR_HOME;

	/* clear the command buffer
	 * needed to erase all TR_TransferListSelect_f
	 * paramaters */
	Cmd_BufClear(); /* That should not be necessary - we are emptying gd.alltransfers[idx] in TR_EmptyTransferCargo */
	TR_TransferSelect_f();
}

/**
 * @brief Unload transfer cargo when finishing the transfer.
 * @param[in] *aircraft Pointer to aircraft used in transfer.
 * @note This should be called only for unloading stuff, not TR_AIRCRAFT.
 * @sa TR_TransferEnd
 */
void TR_EmptyTransferCargo (aircraft_t *aircraft)
{
	int i;
	base_t *destination = NULL;
	transferlist_t *transferidx = NULL;

	if (aircraft)
		transferidx = &gd.alltransfers[aircraft->idx];

	if (transferidx)
		destination = &gd.bases[transferidx->destBase];

	if (transferidx->type != TR_STUFF) {
#ifdef DEBUG
		/* should never happen */
		Com_Printf("TR_EmptyTransferCargo()... called with transferidx->type != TR_STUFF\n");
#endif
		return;
	}

	/* Unload items. @todo: check building status and limits. */
	if (!destination->hasStorage) {
		/* @todo: destroy items in transfercargo and inform an user. */
	} else {
		for (i = 0; i < MAX_OBJDEFS; i++) {
			if (transferidx->itemAmount[i] > 0)
				destination->storage.num[i] += transferidx->itemAmount[i];
		}
	}
	/* Unload personel. @todo: check building status and limits. */
	if (!destination->hasQuarters) {
		/* @todo: what will we do here in such case? */
	} else {
		/* @todo: add a personel to destinationbase. */
		/* @todo: unhire personel in transfercargo and destroy it's inventory. */
		/**
		* first unhire this employee (this will also unlink the inventory from current base
		* and remove him from any buildings he is currently assigned to) and then hire him
		* again in the new base
		*/
		/*E_UnhireEmployee(homebase, ...)*/
		/*E_HireEmployee(b, ...)*/
	}
	/* Unload aliens. @todo: check building status and limits. */
	if (!destination->hasAlienCont) {
		/* @todo: destroy aliens in transfercargo and inform an user. */
	} else {
		for (i = 0; i < numTeamDesc; i++) {
			if (transferidx->alienLiveAmount[i] > 0)
				destination->alienscont[i].amount_alive += transferidx->alienLiveAmount[i];
			if (transferidx->alienBodyAmount[i] > 0)
				destination->alienscont[i].amount_dead += transferidx->alienBodyAmount[i];
		}
	}
}

/**
 * @brief Shows potential targets for aircraft on transfer mission.
 * @param[in] *aircraft Pointer to aircraft used in transfer.
 */
void TR_TransferAircraftMenu (aircraft_t* aircraft)
{
	int i;
	static char transferBaseSelectPopup[512];

	transferBaseSelectPopup[0] = '\0';
	transferAircraft = aircraft;

	for (i = 0; i < gd.numBases; i++) {
		if (!gd.bases[i].founded)
			continue;
		Q_strcat(transferBaseSelectPopup, gd.bases[i].name, sizeof(transferBaseSelectPopup));
	}
	menuText[TEXT_LIST] = transferBaseSelectPopup;
	MN_PushMenu("popup_transferbaselist");
}

/**
 * @brief Ends the transfer and let the aircraft return to homebase (if it was cargo transfer).
 * @param[in] *aircraft Pointer to aircraft used in transfer.
 */
void TR_TransferEnd (aircraft_t* aircraft)
{
	base_t* destination = NULL;
	transferlist_t *transferidx = NULL;

	if (aircraft)
		transferidx = &gd.alltransfers[aircraft->idx];

	if (transferidx)
		destination = &gd.bases[transferidx->destBase];

	if (aircraft->status != AIR_TRANSPORT)
		return;

	assert(destination);

	/* Maybe it was invaded in the meantime. */
	if (!destination->founded) {
		MN_Popup(_("Notice"), _("The base does not exist anymore."));
		return;
	}

	/* If we are transfering stuff - unload it here. */
	if (transferidx->type == TR_STUFF) {
		TR_EmptyTransferCargo (aircraft);
		if (transferidx->destBase != baseCurrent->idx)
			MN_AddNewMessage(_("Transport mission"),
			_("Transport mission ended, returning to homebase now."), qfalse, MSG_TRANSFERFINISHED, NULL);
		AIR_AircraftReturnToBase(aircraft);
	} else {
		/* Otherwise this is an aircraft transfer. */
		/* @todo: check hangar status and limits. */
		if (!destination->hasHangar) {
			/* @todo: First step: change transferidx->destination to other base.
			   Second step: use fuel/range limits when calculating this. */
		} else {
			/* @todo: stuff about changing homebase for aircraft. */
			/* This includes an update of ac->idxBase, ac->idxInBase, ac->homebase, base->numAircraft base->aircraft[] and a lot more. */
			MN_AddNewMessage(_("Transport mission"),
			_("Transport mission ended, aircraft assigned to new base."), qfalse, MSG_TRANSFERFINISHED, NULL);
		}
	}

	/* Clear this transferidx. */
	/* @todo: clear only if it was TR_STUFF, or if TR_AIRCRAFT had succed. */
	memset(&gd.alltransfers[aircraft->idx], 0, sizeof(transferlist_t));
}

/**
 * @brief Start the transfer.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferStart_f (void)
{
	base_t* destination = NULL;
	transferlist_t *transferidx = NULL;

	if (!transferAircraft) {
		Com_Printf("TR_TransferStart_f: No aircraft selected\n");
		return;
	}

	if (!transferBase) {
		Com_Printf("TR_TransferStart_f: No base selected\n");
		return;
	}

	transferidx = &gd.alltransfers[transferAircraft->idx];

	if (transferidx)
		destination = &gd.bases[transferidx->destBase];

	if (transferAircraft->status != AIR_HOME) {
		MN_Popup(_("Notice"), _("Can't start a transport mission while not at base"));
		return;
	}

	transferAircraft->status = AIR_TRANSPORT;

	MAP_MapCalcLine(transferAircraft->pos, destination->pos, &transferAircraft->route);
	/* leave to geoscape */
	MN_PopMenu(qfalse);
	MN_PopMenu(qfalse);
}

/**
 * @brief Adds a thing to aircraft for transfer by left mouseclick.
 * @sa TR_TransferSelect_f
 * @sa TR_TransferInit_f
 */
static void TR_TransferListSelect_f (void)
{
	int num, cnt = 0, i;
	transferlist_t *transferidx = NULL;

	if (Cmd_Argc() < 2)
		return;

	if (!baseCurrent)
		return;

	if (!transferBase) {
		MN_Popup(_("No target base selected"), _("Please select the target base from the list"));
		return;
	}

	if (!transferAircraft) {
		MN_Popup(_("No aircraft selected"), _("Please select the aircraft to use from the list"));
		return;
	}

	/* Prepare the transfer in gd.alltransfers array. */
	transferidx = &gd.alltransfers[transferAircraft->idx];
	if (transferidx)
		transferidx->destBase = transferBase->idx;

	num = atoi(Cmd_Argv(1));

	switch (transferType) {
	case -1:	/**< No list was inited before you call this. */
		return;
	case 0:		/**< items */
		if (!transferidx->type) {
			transferidx->type = TR_STUFF;
		} else if (transferidx->type != TR_STUFF) {
			/* @todo: Allow to transfer both aircraft transporter with cargo onboard? */
			MN_Popup(_("Notice"),
			_("You are transferring aircraft. You cannot use this aircraft to transfer things or employees."));
			return;
		}
		for (i = 0; i < csi.numODs; i++)
			if (baseCurrent->storage.num[i]) {
				if (cnt == num) {
					/* @todo: Check space in transportship. */
					transferidx->itemAmount[i]++;
					/* Remove it from base storage. */
					baseCurrent->storage.num[i]--;
					break;
				}
				cnt++;
			}
		break;
	case 1:		/**< employees */
		if (!transferidx->type) {
			transferidx->type = TR_STUFF;
		} else if (transferidx->type != TR_STUFF) {
			/* @todo: Allow to transfer both aircraft transporter with cargo onboard? */
			MN_Popup(_("Notice"),
			_("You are transferring aircraft. You cannot use this aircraft to transfer things or employees."));
			return;
		}
		/* @todo: employees here. */
		break;
	case 2:		/**< aliens */
		if (!transferidx->type) {
			transferidx->type = TR_STUFF;
		} else if (transferidx->type != TR_STUFF) {
			/* @todo: Allow to transfer both aircraft transporter with cargo onboard? */
			MN_Popup(_("Notice"),
			_("You are transferring aircraft. You cannot use this aircraft to transfer things or employees."));
			return;
		}
		for (i = 0; i < numTeamDesc; i++) {
			if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_dead > 0) {
				if (cnt == num) {
					/* @todo: Check space in transportship. */
					transferidx->alienBodyAmount[i]++;
					/* Remove the corpse from Alien Containment. */
					baseCurrent->alienscont[i].amount_dead--;
					break;
				}
				cnt++;
			}
			if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_alive > 0) {
				if (cnt == num) {
					/* @todo: Check space in transportship. */
					transferidx->alienLiveAmount[i]++;
					/* Remove an alien from Alien Containment. */
					baseCurrent->alienscont[i].amount_alive--;
					break;
				}
				cnt++;
			}
		}
		break;
	case 3:		/**< aircrafts */
		if (!transferidx->type) {
			transferidx->type = TR_AIRCRAFT;
		} else if (transferidx->type != TR_AIRCRAFT) {
			/* @todo: Allow to transfer both aircraft transporter with cargo onboard? */
			MN_Popup(_("Notice"),
			_("You are transferring stuff. You cannot use this aircraft to transfer itself."));
			return;
		}
		/* @todo: aircrafts here. */
		break;
	default:
		return;
	}

	/* clear the command buffer
	 * needed to erase all TR_TransferListSelect_f
	 * paramaters */
	Cmd_BufClear(); /* @todo: no need? */
	TR_TransferSelect_f();
}

/**
 * @brief Display current selected aircraft info in transfer menu
 * @sa TR_TransferAircraftListClick_f
 */
static void TR_TransferDisplayAircraftInfo (void)
{
	if (!transferAircraft)
		return;

	/* @todo */
}

/**
 * @brief Callback for aircraft list click.
 */
static void TR_TransferAircraftListClick_f (void)
{
	int i, j = -1, num;
	/* initialize - maybe there are no aircraft in base */
	aircraft_t* aircraft = NULL;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2)
		return;

	num = atoi(Cmd_Argv(1));

	for (i = 0; i < baseCurrent->numAircraftInBase; i++) {
		aircraft = &baseCurrent->aircraft[i];
		if (aircraft->status == AIR_HOME) {
			j++;
			if (j == num)
				break;
		}
	}

	if (j < 0)
		return;

	transferAircraft = aircraft;
	Cvar_Set("mn_trans_aircraft_name", transferAircraft->shortname);

	TR_TransferDisplayAircraftInfo();
}

/**
 * @brief Finds the destination base.
 */
static void TR_TransferBaseSelectPopup_f (void)
{
	int i, j = -1, num;
	base_t* base;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: trans_baselist_click <type>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		j++;
		if (j == num) {
			break;
		}
	}

	/* no base founded */
	if (j < 0 || i == gd.numBases)
		return;

	transferBase = base;
}

/**
 * @brief Callback for base list click.
 * @note transferBase variable is being set here.
 */
static void TR_TransferBaseSelect_f (void)
{
	static char baseInfo[1024];
	/*char str[128];*/
	int j = -1, num, i;
	base_t* base;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: trans_bases_click <type>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		j++;
		if (j == num) {
			break;
		}
	}

	/* No base founded. */
	if (j < 0 || i == gd.numBases)
		return;

	Com_sprintf(baseInfo, sizeof(baseInfo), "%s\n\n", base->name);

	if (base->hasStorage) {
		/* @todo: Check building status and whether it is free */
		Q_strcat(baseInfo, _("You can transfer equipment - this base has a storage building\n"), sizeof(baseInfo));
		/*Com_sprintf(str, sizeof(str), _(""), base->);*/
	} else {
		Q_strcat(baseInfo, _("No storage building in this base\n"), sizeof(baseInfo));
	}
	if (base->hasQuarters) {
		/* @todo: Check building status and whether it is free */
		Q_strcat(baseInfo, _("You can transfer employees - this base has quarters\n"), sizeof(baseInfo));
		/*Com_sprintf(str, sizeof(str), _(""), base->);*/
	} else {
		Q_strcat(baseInfo, _("No quarters in this base\n"), sizeof(baseInfo));
	}
	if (base->hasAlienCont) {
		/* @todo: Check building status and whether it is free */
		Q_strcat(baseInfo, _("You can transfer aliens - this base has alien containment\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No alien containment in this base\n"), sizeof(baseInfo));
	}
	/* @todo: check hangar (for aircraft transfer) */

	menuText[TEXT_BASE_INFO] = baseInfo;

	/* Set global pointer to current selected base. */
	transferBase = base;

	Cvar_Set("mn_trans_base_name", transferBase->name);

	/* Update stuff-in-base list. */
	TR_TransferSelect_f();
}

/**
 * @brief Removes single item from cargolist by click.
 */
static void TR_CargoListSelect_f (void)
{
	int num, cnt = 0, entries = 0, i;
	transferlist_t *transferidx = NULL;

	if (Cmd_Argc() < 2)
		return;

	if (!baseCurrent)
		return;

	if (!transferAircraft)
		return;

	/* Check the transfer index in gd.alltransfers array. */
	transferidx = &gd.alltransfers[transferAircraft->idx];

	num = atoi(Cmd_Argv(1));

	switch (cargo[num].type) {
	case 1:		/**< items */
		for (i = 0; i < csi.numODs; i++) {
			if (transferidx->itemAmount[i] > 0) {
				if (cnt == num) {
					transferidx->itemAmount[i]--;
					baseCurrent->storage.num[i]++;
					break;
				}
				cnt++;
			}
		}
		break;
	case 2:		/**< employees */
		/*   @todo    */
		break;
	case 3:		/**< alien bodies */
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			if (cargo[i].type == 1 || cargo[i].type == 2)
				entries++;
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < numTeamDesc; i++) {
			if (transferidx->alienBodyAmount[i] > 0) {
				if (cnt == num) {
					transferidx->alienBodyAmount[i]--;
					baseCurrent->alienscont[i].amount_dead++;
					break;
				}
				cnt++;
			}
		}
		break;
	case 4:		/**< alive aliens */
		for (i = 0; i < MAX_CARGO; i++) {
			/* Count previous types on the list. */
			if (cargo[i].type == 1 || cargo[i].type == 2 || cargo[i].type == 3)
				entries++;
		}
		/* Start increasing cnt from the amount of previous entries. */
		cnt = entries;
		for (i = 0; i < numTeamDesc; i++) {
			if (transferidx->alienLiveAmount[i] > 0) {
				if (cnt == num) {
					transferidx->alienLiveAmount[i]--;
					baseCurrent->alienscont[i].amount_alive--;
					break;
				}
				cnt++;
			}
		}
		break;
	default:
		return;
	}

	Cbuf_AddText(va("trans_select %i\n", transferType));
}

/**
 * @brief Transfer menu init function.
 * @note Command to call this: trans_init
 * @note Should be called whenever the Transfer menu gets active.
 */
static void TR_Init_f (void)
{
	static char baseList[1024];
	static char aircraftList[1024];
	int i;
	base_t* base = NULL;
	aircraft_t* aircraft = NULL;

	if (!baseCurrent)
		return;

	transferAircraft = NULL;
	transferBase = NULL;

	baseList[0] = '\0';
	aircraftList[0] = '\0';

	if (Cmd_Argc() < 2)
		Com_Printf("warning: you should call trans_init with parameter 0\n");

	/* Fill in the bases node with every founded base except current. */
	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		Q_strcat(baseList, base->name, sizeof(baseList));
		Q_strcat(baseList, "\n", sizeof(baseList));
	}

	/* Select first available item. */
	TR_TransferSelect_f();

	/* Fill in the aircrafts node with every aircraft in current base. */
	for (i = 0; i < baseCurrent->numAircraftInBase; i++) {
		aircraft = &baseCurrent->aircraft[i];
		if (aircraft->status == AIR_HOME) {
			/* First suitable aircraft will be default selection. */
			if (!transferAircraft)
				transferAircraft = aircraft;
			Q_strcat(aircraftList, aircraft->shortname, sizeof(aircraftList));
			Q_strcat(aircraftList, "\n", sizeof(aircraftList));
		}
	}

	/* Fill up the nodes in case of lack of aircraft (none at all or none suitable). */
	if (!aircraft || !transferAircraft) {
		Q_strcat(aircraftList, _("None"), sizeof(aircraftList));
		Cvar_Set("mn_trans_aircraft_name", _("None"));
	}

	/* @todo: check the transfercargo at selected aircraft and fill up the list. */

	/* Select first available base. */
	/* @todo: if the selected aircraft has it's cargo, select destination base instead. */
	TR_TransferBaseSelect_f();

	/* Set up cvars used to display transferAircraft and transferBase. */
	/* FIXME: Translate the shortname? */
	if (transferAircraft)
		Cvar_Set("mn_trans_aircraft_name", transferAircraft->shortname);
	if (transferBase)
		Cvar_Set("mn_trans_base_name", transferBase->name);

	menuText[TEXT_BASE_LIST] = baseList;
	menuText[TEXT_AIRCRAFT_LIST] = aircraftList;
}

/**
 * @brief Defines commands and cvars for the Transfer menu(s).
 * @sa MN_ResetMenus
 */
void TR_Reset (void)
{
	/* add commands */
	Cmd_AddCommand("trans_init", TR_Init_f, "Init function for Transfer menu");
	Cmd_AddCommand("trans_start", TR_TransferStart_f, "Starts the tranfer");
	Cmd_AddCommand("trans_select", TR_TransferSelect_f, "Switch between transfer types (employees, techs, items)");
	Cmd_AddCommand("trans_emptyairstorage", TR_TransferEmptyAircraftStorage_f, "Unload everything from aircraft storage back to base storage");
	Cmd_AddCommand("trans_baselist_click", TR_TransferBaseSelectPopup_f, "Callback for transfer base list popup");
	Cmd_AddCommand("trans_bases_click", TR_TransferBaseSelect_f, "Callback for base list node click");
	Cmd_AddCommand("trans_list_click", TR_TransferListSelect_f, "Callback for transfer list node click");
	Cmd_AddCommand("trans_aircraft_click", TR_TransferAircraftListClick_f, "Callback for aircraft list node click");
	Cmd_AddCommand("trans_cargolist_click", TR_CargoListSelect_f, "Callback for cargo list node click");
}


