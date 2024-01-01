/**
 * @file
 * @brief UFO recovery and storing callback functions
 * @note UFO recovery functions with UR_*
 * @note UFO storing functions with US_*
 */

/*
Copyright (C) 2002-2024 UFO: Alien Invasion.

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

#include "../../DateTime.h"
#include "../../cl_shared.h"
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_ufo.h"
#include "cp_uforecovery.h"
#include "cp_uforecovery_callbacks.h"
#include "cp_geoscape.h"
#include "cp_time.h"

#define HAPPINESS_UFO_SALE_GAIN				0.02
#define HAPPINESS_UFO_SALE_LOSS				0.005

/**
 * @brief Function to initialize list of storage locations for recovered UFO.
 * @note Command to call this: cp_uforecovery_store_init.
 * @sa UR_ConditionsForStoring
 */
static void UR_DialogInitStore_f (void)
{
	/* Check how many bases can store this UFO. */
	INS_Foreach(installation) {
		const capacities_t* capacity = &installation->ufoCapacity;
		if (capacity->max > 0 && capacity->max > capacity->cur) {
			cgi->UI_ExecuteConfunc("ui_uforecovery_ufoyards %d \"%s\" %d %d",
				installation->idx,
				installation->name,
				std::max(capacity->max - capacity->cur, 0),
				capacity->max
			);
		}
	}
}

/**
 * @brief Function to start UFO recovery process.
 * @note Command to call this: cp_uforecovery_store_start.
 */
static void UR_DialogStartStore_f (void)
{

	if (cgi->Cmd_Argc() < 4) {
		cgi->Com_Printf("Usage: %s <ufoType> <damage> <installationIDX>\n", cgi->Cmd_Argv(0));
		return;
	}

	const aircraft_t* ufo = AIR_GetAircraftSilent(cgi->Cmd_Argv(1));
	if (ufo == nullptr || !AIR_IsUFO(ufo)) {
		cgi->Com_Printf("%s Invalid UFO type\n", cgi->Cmd_Argv(0));
		return;
	}

	float condition = atof(cgi->Cmd_Argv(2));
	if (condition < 0.0f || condition > 100.0f) {
		cgi->Com_Printf("%s Invalid UFO damage value\n", cgi->Cmd_Argv(0));
		return;
	}

	installation_t* installation = INS_GetByIDX(atoi(cgi->Cmd_Argv(3)));
	if (installation == nullptr || installation->ufoCapacity.max <= 0) {
		cgi->Com_Printf("%s Invalid Installation IDX\n", cgi->Cmd_Argv(0));
		return;
	}

	if (installation->ufoCapacity.max <= installation->ufoCapacity.cur) {
		cgi->Com_Printf("%s The selected installation has no spare capacity\n", cgi->Cmd_Argv(0));
		return;
	}

	Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO is being transported to %s."),
		UFO_GetName(ufo), installation->name);
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer);

	DateTime date = DateTime(ccs.date) + DateTime((int) RECOVERY_DELAY, 0);
	US_StoreUFO(ufo, installation, date, condition);
}

/**
 * @brief Function to initialize list to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecovery_sell_init.
 */
static void UR_DialogInitSell_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <ufoType>\n", cgi->Cmd_Argv(0));
		return;
	}

	const aircraft_t* ufo = AIR_GetAircraft(cgi->Cmd_Argv(1));
	if (ufo == nullptr) {
		cgi->Com_Printf("%s Invalid ufo Type\n", cgi->Cmd_Argv(0));
		return;
	}

	NAT_Foreach(nation) {
		const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
		int price;

		price = (int) (ufo->price * (.85f + frand() * .3f));
		/* Nation will pay less if corrupted */
		price = (int) (price * exp(-stats->xviInfection / 20.0f));

		cgi->UI_ExecuteConfunc("ui_uforecovery_nations %s \"%s\" %d %s %.2f",
			nation->id,
			_(nation->name),
			price,
			NAT_GetHappinessString(stats->happiness),
			stats->happiness
		);
	}
}

/**
 * @brief Function to start UFO selling process.
 * @note Command to call this: cp_uforecovery_sell_start.
 */
static void UR_DialogStartSell_f (void)
{
	if (cgi->Cmd_Argc() < 4) {
		cgi->Com_Printf("Usage: %s <ufoID> <nationId> <price>\n", cgi->Cmd_Argv(0));
		return;
	}

	const nation_t* nation = NAT_GetNationByID(cgi->Cmd_Argv(2));
	if (nation == nullptr) {
		cgi->Com_Printf("%s: Nation not found\n", cgi->Cmd_Argv(0));
		return;
	}

	int price = atoi(cgi->Cmd_Argv(3));
	if (price <= 0) {
		cgi->Com_Printf("%s: Invalid price\n", cgi->Cmd_Argv(0));
		return;
	}

	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Recovered %s from the battlefield. UFO sold to nation %s, gained %i credits."),
		cgi->Cmd_Argv(1), _(nation->name), price);
	MS_AddNewMessage(_("UFO Recovery"), cp_messageBuffer);
	CP_UpdateCredits(ccs.credits + price);

	/* update nation happiness */
	NAT_Foreach(nat) {
		float ufoHappiness;

		assert(nat);
		if (nat == nation)
			/* nation is happy because it got the UFO */
			ufoHappiness = HAPPINESS_UFO_SALE_GAIN;
		else
			/* nation is unhappy because it wanted the UFO */
			ufoHappiness = HAPPINESS_UFO_SALE_LOSS;

		NAT_SetHappiness(ccs.curCampaign->minhappiness, nat, nat->stats[0].happiness + ufoHappiness);
	}
}


/* --- UFO storage management --- */


/**
 * @brief Returns string representation of the stored UFO's status
 * @note uses stored ufo status and disassembly
 */
const char* US_StoredUFOStatus (const storedUFO_t* ufo)
{
	assert(ufo);

	if (ufo->disassembly != nullptr)
		return "disassembling";

	switch (ufo->status) {
		case SUFO_STORED:
			return "stored";
		case SUFO_RECOVERED:
		case SUFO_TRANSFERED:
			return "transferring";
		default:
			return "unknown";
	}
}

/**
 * @brief Send Stored UFO data to the UI
 * @note it's called by 'ui_selectstoredufo' command with a parameter of the stored UFO IDX
 */
static void US_SelectStoredUfo_f (void)
{
	const storedUFO_t* ufo;

	if (cgi->Cmd_Argc() < 2 || (ufo = US_GetStoredUFOByIDX(atoi(cgi->Cmd_Argv(1)))) == nullptr) {
		cgi->UI_ExecuteConfunc("show_storedufo -");
		return;
	}

	const char* ufoName = UFO_GetName(ufo->ufoTemplate);
	const char* status = US_StoredUFOStatus(ufo);
	const char* eta;

	if (Q_streq(status, "transferring")) {
		eta = CP_SecondConvert(Date_DateToSeconds(ufo->arrive - ccs.date));
	} else {
		eta = "-";
	}

	cgi->UI_ExecuteConfunc("show_storedufo %d \"%s\" %3.0f \"%s\" \"%s\" \"%s\" \"%s\"", ufo->idx, ufoName, ufo->condition * 100, ufo->ufoTemplate->model, status, eta, ufo->installation->name);
}


/**
 * @brief Destroys a stored UFO
 * @note it's called by 'ui_destroystoredufo' command with a parameter of the stored UFO IDX and a confirmation value
 */
static void US_DestroyStoredUFO_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx> [0|1]\nWhere the second, optional parameter is the confirmation.\n", cgi->Cmd_Argv(0));
		return;
	}
	storedUFO_t* ufo = US_GetStoredUFOByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!ufo) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Stored UFO with idx: %i does not exist\n", atoi(cgi->Cmd_Argv(1)));
		return;
	}

	/* Ask 'Are you sure?' by default */
	if (cgi->Cmd_Argc() < 3 || !atoi(cgi->Cmd_Argv(2))) {
		char command[128];

		Com_sprintf(command, sizeof(command), "ui_pop; ui_destroystoredufo %d 1; mn_installation_select %d;", ufo->idx, ufo->installation->idx);
		cgi->UI_PopupButton(_("Destroy stored UFO"), _("Do you really want to destroy this stored UFO?"),
			command, _("Destroy"), _("Destroy stored UFO"),
			"ui_pop;", _("Cancel"), _("Forget it"),
			nullptr, nullptr, nullptr);
		return;
	}
	US_RemoveStoredUFO(ufo);
	cgi->Cmd_ExecuteString("mn_installation_select %d", ufo->installation->idx);
}

/**
 * @brief Fills UFO Yard UI with transfer destinations
 */
static void US_FillUFOTransfer_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx>\n", cgi->Cmd_Argv(0));
		return;
	}

	storedUFO_t* ufo = US_GetStoredUFOByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!ufo) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Stored UFO with idx: %i does not exist\n", atoi(cgi->Cmd_Argv(1)));
		return;
	}

	cgi->UI_ExecuteConfunc("ufotransferlist_clear");
	INS_ForeachOfType(ins, INSTALLATION_UFOYARD) {
		if (ins == ufo->installation)
			continue;
		nation_t* nat = GEO_GetNation(ins->pos);
		const char* nationName = nat ? _(nat->name) : "";
		const int freeSpace = std::max(0, ins->ufoCapacity.max - ins->ufoCapacity.cur);
		cgi->UI_ExecuteConfunc("ufotransferlist_addyard %d \"%s\" \"%s\" %d %d", ins->idx, ins->name, nationName, freeSpace, ins->ufoCapacity.max);
	}
}

/**
 * @brief Send Stored UFOs of the destination UFO Yard
 */
static void US_FillUFOTransferUFOs_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Usage: %s <idx>\n", cgi->Cmd_Argv(0));
		return;
	}

	installation_t* ins = INS_GetByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!ins) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Installation with idx: %i does not exist\n", atoi(cgi->Cmd_Argv(1)));
		return;
	}

	cgi->UI_ExecuteConfunc("ufotransferlist_clearufos %d", ins->idx);
	US_Foreach(ufo) {
		if (ufo->installation != ins)
			continue;
		cgi->UI_ExecuteConfunc("ufotransferlist_addufos %d %d \"%s\"", ins->idx, ufo->idx, ufo->ufoTemplate->model);
	}
}

/**
 * @brief Callback to start the transfer of a stored UFO
 */
static void US_TransferUFO_f (void)
{
	storedUFO_t* ufo;
	installation_t* ins = nullptr;

	if (cgi->Cmd_Argc() < 3) {
		cgi->Com_Printf("Usage: %s <stored-ufo-idx> <ufoyard-idx>\n", cgi->Cmd_Argv(0));
		return;
	}
	ufo = US_GetStoredUFOByIDX(atoi(cgi->Cmd_Argv(1)));
	if (ufo == nullptr) {
		cgi->Com_Printf("Stored ufo with idx %i not found.\n", atoi(cgi->Cmd_Argv(1)));
		return;
	}
	ins = INS_GetByIDX(atoi(cgi->Cmd_Argv(2)));
	if (!ins) {
		cgi->Com_Printf("Installation with idx: %i does not exist\n", atoi(cgi->Cmd_Argv(2)));
		return;
	}
	US_TransferUFO(ufo, ins);
}

static const cmdList_t ufoRecoveryCallbacks[] = {
	{"cp_uforecovery_sell_init", UR_DialogInitSell_f, "Function to initialize sell recovered UFO to desired nation."},
	{"cp_uforecovery_store_init", UR_DialogInitStore_f, "Function to initialize store recovered UFO in desired base."},
	{"cp_uforecovery_store_start", UR_DialogStartStore_f, "Function to start UFO recovery processing."},
	{"cp_uforecovery_sell_start", UR_DialogStartSell_f, "Function to start UFO selling processing."},
	{"ui_selectstoredufo", US_SelectStoredUfo_f, "Send Stored UFO data to the UI"},
	{"ui_destroystoredufo", US_DestroyStoredUFO_f, "Destroy stored UFO"},
	{"ui_fill_ufotransfer", US_FillUFOTransfer_f, "Fills UFO Yard UI with transfer destinations"},
	{"ui_selecttransferyard", US_FillUFOTransferUFOs_f, "Send Stored UFOs of the destination UFO Yard"},
	{"ui_transferufo", US_TransferUFO_f, "Transfer stored UFO to another UFO Yard"},
	{nullptr, nullptr, nullptr}
};

void UR_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(ufoRecoveryCallbacks);
}

void UR_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(ufoRecoveryCallbacks);
}
