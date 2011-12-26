/**
 * @file cp_produce_callbacks.c
 * @brief Menu related callback functions used for production.
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

#include "../../cl_shared.h"
#include "../../cl_inventory.h"
#include "../../ui/ui_main.h"
#include "cp_campaign.h"
#include "cp_market.h"
#include "cp_ufo.h"
#include "cp_popup.h"
#include "cp_produce.h"
#include "cp_produce_callbacks.h"

/**
 * Holds the current active production category/filter type.
 * @sa itemFilterTypes_t
 */
static int produceCategory = FILTER_S_PRIMARY;

/** Holds the current active selected queue entry. */
static production_t *selectedProduction = NULL;

/** A list if all producable items. */
static linkedList_t *productionItemList;

/** Currently selected entry in the productionItemList (depends on content) */
static productionData_t selectedData;

/** @brief Number of blank lines between queued items and tech list. */
static const int QUEUE_SPACERS = 2;

/**
 * @brief Resets the selected item data structure. Does not reset the selected production
 * @sa PR_ClearSelected
 */
static void PR_ClearSelectedItems (void)
{
	OBJZERO(selectedData);
	selectedData.type = PRODUCTION_TYPE_MAX;
}

/**
 * @brief Resets all "selected" pointers to NULL.
 * @sa PR_ClearSelectedItems
 */
static void PR_ClearSelected (void)
{
	PR_ClearSelectedItems();
	selectedProduction = NULL;
}

/**
 * @brief update the list of queued and available items
 * @param[in] base Pointer to the base.
 */
static void PR_UpdateProductionList (const base_t* base)
{
	int i;
	linkedList_t *productionList = NULL;
	linkedList_t *productionQueued = NULL;
	linkedList_t *productionAmount = NULL;
	const production_queue_t *queue;

	assert(base);

	queue = PR_GetProductionForBase(base);

	/* First add all the queue items ... */
	for (i = 0; i < queue->numItems; i++) {
		const production_t *prod = &queue->items[i];
		if (PR_IsItem(prod)) {
			const objDef_t *od = prod->data.data.item;
			LIST_AddString(&productionList, va("%s", _(od->name)));
			LIST_AddString(&productionAmount, va("%i", B_ItemInBase(prod->data.data.item, base)));
			LIST_AddString(&productionQueued, va("%i", prod->amount));
		} else if (PR_IsAircraft(prod)) {
			const aircraft_t *aircraftTemplate = prod->data.data.aircraft;

			LIST_AddString(&productionList, va("%s", _(aircraftTemplate->name)));
			LIST_AddString(&productionAmount, va("%i", AIR_CountInBaseByTemplate(base, aircraftTemplate)));
			LIST_AddString(&productionQueued, va("%i", prod->amount));
		} else if (PR_IsDisassembly(prod)) {
			const storedUFO_t *ufo = prod->data.data.ufo;

			LIST_AddString(&productionList, va("%s (%.0f%%)", UFO_TypeToName(ufo->ufoTemplate->ufotype), ufo->condition * 100));
			LIST_AddString(&productionAmount, va("%i", US_UFOsInStorage(ufo->ufoTemplate, ufo->installation)));
			LIST_AddString(&productionQueued, "1");
		}
	}

	/* Then spacers ... */
	for (i = 0; i < QUEUE_SPACERS; i++) {
		LIST_AddString(&productionList, "");
		LIST_AddString(&productionAmount, "");
		LIST_AddString(&productionQueued, "");
	}

	LIST_Delete(&productionItemList);

	/* Then go through all object definitions ... */
	if (produceCategory == FILTER_DISASSEMBLY) {
		/** UFOs at UFO stores */
		US_Foreach(ufo) {
			/* UFO is being transported */
			if (ufo->status != SUFO_STORED)
				continue;
			/* UFO not researched */
			if (!RS_IsResearched_ptr(ufo->ufoTemplate->tech))
				continue;
			/* The UFO is being disassembled already */
			if (ufo->disassembly)
				continue;

			LIST_AddPointer(&productionItemList, ufo);
			LIST_AddString(&productionList, va("%s (%.0f%%)", UFO_TypeToName(ufo->ufoTemplate->ufotype), ufo->condition * 100));
			LIST_AddString(&productionAmount, va("%i", US_UFOsInStorage(ufo->ufoTemplate, ufo->installation)));
			LIST_AddString(&productionQueued, "");
		}
	} else if (produceCategory == FILTER_AIRCRAFT) {
		for (i = 0; i < ccs.numAircraftTemplates; i++) {
			aircraft_t *aircraftTemplate = &ccs.aircraftTemplates[i];
			/* don't allow producing ufos */
			if (AIR_IsUFO(aircraftTemplate))
				continue;
			if (!aircraftTemplate->tech) {
				Com_Printf("PR_UpdateProductionList: no technology for craft %s!\n", aircraftTemplate->id);
				continue;
			}

			Com_DPrintf(DEBUG_CLIENT, "air: %s ufotype: %i tech: %s time: %i\n", aircraftTemplate->id,
					aircraftTemplate->ufotype, aircraftTemplate->tech->id, aircraftTemplate->tech->produceTime);

			if (aircraftTemplate->tech->produceTime > 0 && RS_IsResearched_ptr(aircraftTemplate->tech)) {
				LIST_AddPointer(&productionItemList, aircraftTemplate);
				LIST_AddString(&productionList, va("%s", _(aircraftTemplate->name)));
				LIST_AddString(&productionAmount, va("%i", AIR_CountInBaseByTemplate(base, aircraftTemplate)));
				LIST_AddString(&productionQueued, "");
			}
		}
	} else {
		objDef_t *od;
		for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
			const technology_t *tech;
			if (od->isVirtual)
				continue;
			tech = RS_GetTechForItem(od);
			/* We will not show items that are not producible.
			 * We can only produce what was researched before. */
			if (RS_IsResearched_ptr(tech) && PR_ItemIsProduceable(od) && INV_ItemMatchesFilter(od, produceCategory)) {
				LIST_AddPointer(&productionItemList, od);

				LIST_AddString(&productionList, va("%s", _(od->name)));
				LIST_AddString(&productionAmount, va("%i", B_ItemInBase(od, base)));
				LIST_AddString(&productionQueued, "");
			}
		}
	}

	/* bind the menu text to our static char array */
	UI_RegisterLinkedListText(TEXT_PRODUCTION_LIST, productionList);
	/* bind the amount of available items */
	UI_RegisterLinkedListText(TEXT_PRODUCTION_AMOUNT, productionAmount);
	/* bind the amount of queued items */
	UI_RegisterLinkedListText(TEXT_PRODUCTION_QUEUED, productionQueued);
}

static void PR_RequirementsInfo (const base_t const *base, const requirements_t const *reqs) {
	const vec4_t green = {0.0f, 1.0f, 0.0f, 0.8f};
	const vec4_t yellow = {1.0f, 0.874f, 0.294f, 1.0f};
	int i;
	uiNode_t *req_root = NULL;
	uiNode_t *req_items = NULL;
#if 0
	uiNode_t *req_techs = NULL;
	uiNode_t *req_technots = NULL;
#endif

	UI_ResetData(OPTION_PRODUCTION_REQUIREMENTS);

	if (!reqs->numLinks)
		return;

	for (i = 0; i < reqs->numLinks; i++) {
		const requirement_t const *req = &reqs->links[i];

		switch (req->type) {
		case RS_LINK_ITEM: {
				uiNode_t *node = UI_AddOption(&req_items, req->link.od->id, va("%i %s", req->amount, _(req->link.od->name)), va("%i", i));
				if (B_ItemInBase(req->link.od, base) >= req->amount)
					Vector4Copy(green, node->color);
				else
					Vector4Copy(yellow, node->color);
				break;
			}
		case RS_LINK_ANTIMATTER: {
				technology_t *tech = RS_GetTechByID(ANTIMATTER_TECH_ID);
				uiNode_t *node;

				assert(tech);
				node = UI_AddOption(&req_items, ANTIMATTER_TECH_ID, va("%i %s", req->amount, _(tech->name)), va("%i", i));
				if (B_AntimatterInBase(base) >= req->amount)
					Vector4Copy(green, node->color);
				else
					Vector4Copy(yellow, node->color);
				break;
			}
#if 0
		case RS_LINK_TECH:
/*			if (node && RS_IsResearched_ptr(req->link)) ... */
			break;
		case RS_LINK_TECH_NOT:
/*			if (node && RS_IsResearched_ptr(req->link)) ... */
			break;
#endif
		default:
			break;
		}
	}
	if (req_items) {
		uiNode_t *node = UI_AddOption(&req_root, "items", "_Items", "item");
		node->firstChild = req_items;
	}
	UI_RegisterOption(OPTION_PRODUCTION_REQUIREMENTS, req_root);
}

/**
 * @brief Prints information about the selected item (no aircraft) in production.
 * @param[in] base Pointer to the base where informations should be printed.
 * @param[in] od The attributes of the item being produced.
 * @param[in] remainingHours The remaining hours until this production is finished
 * @sa PR_ProductionInfo
 */
static void PR_ItemProductionInfo (const base_t *base, const objDef_t *od, int remainingHours)
{
	static char productionInfo[512];

	assert(base);
	assert(od);

	/* Don't try to display an item which is not producible. */
	if (!PR_ItemIsProduceable(od)) {
		Com_sprintf(productionInfo, sizeof(productionInfo), _("No item selected"));
		Cvar_Set("mn_item", "");
	} else {
		const technology_t *tech = RS_GetTechForItem(od);

		Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", _(od->name));
		Q_strcat(productionInfo, va(_("Costs per item\t%i c\n"), PR_GetPrice(od)), sizeof(productionInfo));
		Q_strcat(productionInfo, va(_("Production time\t%ih\n"), remainingHours), sizeof(productionInfo));
		Q_strcat(productionInfo, va(_("Item size\t%i\n"), od->size), sizeof(productionInfo));
		Cvar_Set("mn_item", od->id);

		PR_RequirementsInfo(base, &tech->requireForProduction);
		Cmd_ExecuteString(va("show_requirements %i", tech->requireForProduction.numLinks));
	}
	UI_RegisterText(TEXT_PRODUCTION_INFO, productionInfo);
}

/**
 * @brief Prints information about the selected disassembly task
 * @param[in] ufo The UFO being disassembled.
 * @param[in] remainingHours The remaining hours until this production is finished
 * @sa PR_ProductionInfo
 */
static void PR_DisassemblyInfo (const storedUFO_t *ufo, int remainingHours)
{
	static char productionInfo[512];
	int i;

	assert(ufo);
	assert(ufo->ufoTemplate);

	Com_sprintf(productionInfo, sizeof(productionInfo), "%s (%.0f%%) - %s\n", _(UFO_TypeToName(ufo->ufoTemplate->ufotype)), ufo->condition * 100, _("Disassembly"));
	Q_strcat(productionInfo, va(_("Stored at: %s\n"), ufo->installation->name), sizeof(productionInfo));
	Q_strcat(productionInfo, va(_("Disassembly time: %ih\n"), remainingHours), sizeof(productionInfo));
	Q_strcat(productionInfo, _("Components:\n"), sizeof(productionInfo));
	/* Print components. */
	for (i = 0; i < ufo->comp->numItemtypes; i++) {
		const objDef_t *compOd = ufo->comp->items[i];
		const int amount = (ufo->condition < 1 && ufo->comp->itemAmount2[i] != COMP_ITEMCOUNT_SCALED) ? ufo->comp->itemAmount2[i] : round(ufo->comp->itemAmount[i] * ufo->condition);

		if (amount == 0)
			continue;

		assert(compOd);
		Q_strcat(productionInfo, va("  %s (%i)\n", _(compOd->name), amount), sizeof(productionInfo));
	}
	UI_RegisterText(TEXT_PRODUCTION_INFO, productionInfo);
	Cvar_Set("mn_item", ufo->id);
	Cmd_ExecuteString("show_requirements 0");
}

/**
 * @brief Prints information about the selected aircraft in production.
 * @param[in] base Pointer to the base where informations should be printed.
 * @param[in] aircraftTemplate The aircraft to print the information for
 * @param[in] remainingHours The remaining hours until this production is finished
 * @sa PR_ProductionInfo
 */
static void PR_AircraftInfo (const base_t *base, const aircraft_t *aircraftTemplate, int remainingHours)
{
	static char productionInfo[512];

	Com_sprintf(productionInfo, sizeof(productionInfo), "%s\n", _(aircraftTemplate->name));
	Q_strcat(productionInfo, va(_("Production costs\t%i c\n"), PR_GetPrice(aircraftTemplate)),
		sizeof(productionInfo));
	Q_strcat(productionInfo, va(_("Production time\t%ih\n"), remainingHours), sizeof(productionInfo));
	UI_RegisterText(TEXT_PRODUCTION_INFO, productionInfo);
	Cvar_Set("mn_item", aircraftTemplate->id);
	PR_RequirementsInfo(base, &aircraftTemplate->tech->requireForProduction);
	Cmd_ExecuteString(va("show_requirements %i", aircraftTemplate->tech->requireForProduction.numLinks));
}

/**
 * @brief Prints information about the selected item in production.
 * @param[in] base Pointer to the base where informations should be printed.
 * @sa PR_AircraftInfo
 * @sa PR_ItemProductionInfo
 * @sa PR_DisassemblyInfo
 */
static void PR_ProductionInfo (const base_t *base)
{
	if (selectedProduction) {
		const production_t *prod = selectedProduction;
		const int time = PR_GetRemainingHours(prod);
		UI_ExecuteConfunc("prod_taskselected");

		if (PR_IsAircraft(prod)) {
			PR_AircraftInfo(base, prod->data.data.aircraft, time);
			UI_ExecuteConfunc("amountsetter enable");
		} else if (PR_IsItem(prod)) {
			PR_ItemProductionInfo(base, prod->data.data.item, time);
			UI_ExecuteConfunc("amountsetter enable");
		} else if (PR_IsDisassembly(prod)) {
			PR_DisassemblyInfo(prod->data.data.ufo, time);
			UI_ExecuteConfunc("amountsetter disable");
		} else {
			Com_Error(ERR_DROP, "PR_ProductionInfo: Selected production is not item nor aircraft nor ufo.\n");
		}
		Cvar_SetValue("mn_production_amount", selectedProduction->amount);
	} else {
		if (!PR_IsDataValid(&selectedData)) {
			UI_ExecuteConfunc("prod_nothingselected");
			if (produceCategory == FILTER_AIRCRAFT)
				UI_RegisterText(TEXT_PRODUCTION_INFO, _("No aircraft selected."));
			else
				UI_RegisterText(TEXT_PRODUCTION_INFO, _("No item selected"));
			Cvar_Set("mn_item", "");
		} else {
			UI_ExecuteConfunc("prod_availableselected");
			if (PR_IsAircraftData(&selectedData)) {
				PR_AircraftInfo(base, selectedData.data.aircraft, PR_GetProductionHours(base, &selectedData));
			} else if (PR_IsItemData(&selectedData)) {
				PR_ItemProductionInfo(base, selectedData.data.item, PR_GetProductionHours(base, &selectedData));
			} else if (PR_IsDisassemblyData(&selectedData)) {
				PR_DisassemblyInfo(selectedData.data.ufo, PR_GetProductionHours(base, &selectedData));
			}
		}
	}
}

/**
 * @brief Click function for production list
 * @note Opens the UFOpaedia - by right clicking an item
 */
static void PR_ProductionListRightClick_f (void)
{
	int num;
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	/* can be called from everywhere without a base set */
	if (!base)
		return;

	queue = PR_GetProductionForBase(base);

	/* not enough parameters */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* clicked which item? */
	num = atoi(Cmd_Argv(1));

	/* Clicked the production queue or the item list? */
	if (num < queue->numItems && num >= 0) {
		production_t *prod = &queue->items[num];
		const technology_t* tech = PR_GetTech(&prod->data);
		selectedProduction = prod;
		UP_OpenWith(tech->id);
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		const int idx = num - queue->numItems - QUEUE_SPACERS;

		if (produceCategory == FILTER_AIRCRAFT) {
			const aircraft_t *aircraftTemplate = (const aircraft_t*)LIST_GetByIdx(productionItemList, idx);
			/* aircraftTemplate may be empty if rclicked below real entry.
			 * UFO research definition must not have a tech assigned,
			 * only RS_CRAFT types have */
			if (aircraftTemplate && aircraftTemplate->tech)
				UP_OpenWith(aircraftTemplate->tech->id);
		} else if (produceCategory == FILTER_DISASSEMBLY) {
			const storedUFO_t *ufo = (const storedUFO_t*)LIST_GetByIdx(productionItemList, idx);
			if (ufo && ufo->ufoTemplate && ufo->ufoTemplate->tech) {
				UP_OpenWith(ufo->ufoTemplate->tech->id);
			}
		} else {
			objDef_t *od = (objDef_t*)LIST_GetByIdx(productionItemList, idx);
			const technology_t *tech = RS_GetTechForItem(od);

			/* Open up UFOpaedia for this entry. */
			if (RS_IsResearched_ptr(tech) && INV_ItemMatchesFilter(od, produceCategory)) {
				PR_ClearSelected();
				PR_SetData(&selectedData, PRODUCTION_TYPE_ITEM, od);
				UP_OpenWith(tech->id);
				return;
			}
		}
	}
#ifdef DEBUG
	else
		Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListRightClick_f: Click on spacer %i\n", num);
#endif
}

/**
 * @brief Click function for production list.
 * @note "num" is the entry in the visible production list (includes queued entries and spaces).
 * @todo left click on spacer should either delete current selection or do nothing, not update visible selection but show old info
 */
static void PR_ProductionListClick_f (void)
{
	int num;
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	/* can be called from everywhere without a base set */
	if (!base)
		return;

	queue = PR_GetProductionForBase(base);

	/* Break if there are not enough parameters. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* Clicked which item? */
	num = atoi(Cmd_Argv(1));

	/* Clicked the production queue or the item list? */
	if (num < queue->numItems && num >= 0) {
		selectedProduction = &queue->items[num];
		PR_ProductionInfo(base);
	} else if (num >= queue->numItems + QUEUE_SPACERS) {
		/* Clicked in the item list. */
		const int idx = num - queue->numItems - QUEUE_SPACERS;

		if (produceCategory == FILTER_DISASSEMBLY) {
			storedUFO_t *ufo = (storedUFO_t*)LIST_GetByIdx(productionItemList, idx);

			PR_ClearSelected();
			PR_SetData(&selectedData, PRODUCTION_TYPE_DISASSEMBLY, ufo);

			PR_ProductionInfo(base);
		} else if (produceCategory == FILTER_AIRCRAFT) {
			aircraft_t *aircraftTemplate = (aircraft_t*)LIST_GetByIdx(productionItemList, idx);
			if (!aircraftTemplate) {
				Com_DPrintf(DEBUG_CLIENT, "PR_ProductionListClick_f: No item found at the list-position %i!\n", idx);
				return;
			}
			/* ufo research definition must not have a tech assigned
			 * only RS_CRAFT types have
			 * @sa RS_InitTree */
			if (aircraftTemplate->tech && aircraftTemplate->tech->produceTime >= 0
			 && RS_IsResearched_ptr(aircraftTemplate->tech)) {
				PR_ClearSelected();
				PR_SetData(&selectedData, PRODUCTION_TYPE_AIRCRAFT, aircraftTemplate);
				PR_ProductionInfo(base);
			}
		} else {
			objDef_t *od = (objDef_t*)LIST_GetByIdx(productionItemList, idx);
			const technology_t *tech = RS_GetTechForItem(od);

			/* We can only produce items that fulfill the following conditions... */
			if (RS_IsResearched_ptr(tech) && PR_ItemIsProduceable(od) && INV_ItemMatchesFilter(od, produceCategory)) {
				PR_ClearSelected();
				PR_SetData(&selectedData, PRODUCTION_TYPE_ITEM, od);
				PR_ProductionInfo(base);
			}
		}
	}
}

/**
 * @brief Will select a new tab on the production list.
 */
static void PR_ProductionType_f (void)
{
	int cat;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category>\n", Cmd_Argv(0));
		return;
	}

	cat = INV_GetFilterTypeID(Cmd_Argv(1));

	/* Check if the given category index is valid. */
	if (cat < 0 || cat >= MAX_FILTERTYPES)
		cat = FILTER_S_PRIMARY;

	/* Can be called from everywhere without a base set */
	if (!base)
		return;

	produceCategory = cat;
	Cvar_Set("mn_itemtype", INV_GetFilterType(produceCategory));

	/* Update list of entries for current production tab. */
	PR_UpdateProductionList(base);

	/* Reset selected entry, if it was not from the queue */
	PR_ClearSelectedItems();

	/* Select first entry in the list (if any). */
	if (LIST_Count(productionItemList) > 0) {
		if (produceCategory == FILTER_AIRCRAFT) {
			const aircraft_t *aircraft = (const aircraft_t*)LIST_GetByIdx(productionItemList, 0);
			PR_SetData(&selectedData, PRODUCTION_TYPE_AIRCRAFT, aircraft);
		} else if (produceCategory == FILTER_DISASSEMBLY) {
			const storedUFO_t *storedUFO = (const storedUFO_t*)LIST_GetByIdx(productionItemList, 0);
			PR_SetData(&selectedData, PRODUCTION_TYPE_DISASSEMBLY, storedUFO);
		} else {
			const objDef_t *item = (const objDef_t*)LIST_GetByIdx(productionItemList, 0);
			PR_SetData(&selectedData, PRODUCTION_TYPE_ITEM, item);
		}
	}
	/* update selection index if first entry of actual list was chosen */
	if (!selectedProduction) {
		const production_queue_t *prod = PR_GetProductionForBase(base);
		UI_ExecuteConfunc("prod_selectline %i", prod->numItems + QUEUE_SPACERS);
	}

	/* Update displayed info about selected entry (if any). */
	PR_ProductionInfo(base);
}

/**
 * @brief Will fill the list of producible items.
 * @note Some of Production Menu related cvars are being set here.
 */
static void PR_ProductionList_f (void)
{
	char tmpbuf[MAX_VAR];
	int numWorkshops;
	base_t *base = B_GetCurrentSelectedBase();

	/* can be called from everywhere without a started game */
	if (!base)
		return;

	numWorkshops = max(0, B_GetNumberOfBuildingsInBaseByBuildingType(base, B_WORKSHOP));

	Cvar_SetValue("mn_production_limit", MAX_PRODUCTIONS_PER_WORKSHOP * numWorkshops);
	Cvar_SetValue("mn_production_basecap", CAP_GetMax(base, CAP_WORKSPACE));

	/* Set amount of workers - all/ready to work (determined by base capacity. */
	PR_UpdateProductionCap(base);

	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i",
		CAP_GetCurrent(base, CAP_WORKSPACE), E_CountHired(base, EMPL_WORKER));
	Cvar_Set("mn_production_workers", tmpbuf);

	Com_sprintf(tmpbuf, sizeof(tmpbuf), "%i/%i",
		CAP_GetCurrent(base, CAP_ITEMS), CAP_GetMax(base, CAP_ITEMS));
	Cvar_Set("mn_production_storage", tmpbuf);

	PR_ClearSelected();
}

/**
 * @brief Increases the production amount by given parameter.
 */
static void PR_ProductionIncrease_f (void)
{
	production_t *prod;
	base_t *base = B_GetCurrentSelectedBase();
	technology_t *tech = NULL;
	int amount = 1;
	int producibleAmount;
	static char productionPopup[1024];

	if (!base)
		return;

	if (!PR_IsDataValid(&selectedData))
		return;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	if (selectedProduction) {
		prod = selectedProduction;

		/* We can disassembly UFOs only one-by-one. */
		if (PR_IsDisassembly(prod))
			return;

		if (PR_IsAircraft(prod)) {
			/* Don't allow to queue more aircraft if there is no free space. */
			if (AIR_CalculateHangarStorage(prod->data.data.aircraft, base, 0) <= 0) {
				CP_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
				Cvar_SetValue("mn_production_amount", prod->amount);
				return;
			}
		}

		/* amount limit per one production */
		if (prod->amount + amount > MAX_PRODUCTION_AMOUNT) {
			amount = max(0, MAX_PRODUCTION_AMOUNT - prod->amount);
		}
		if (amount == 0) {
			Cvar_SetValue("mn_production_amount", prod->amount);
			return;
		}

		tech = PR_GetTech(&prod->data);
		assert(tech);

		producibleAmount = PR_RequirementsMet(amount, &tech->requireForProduction, base);
		if (producibleAmount == 0) {
			CP_Popup(_("Not enough materials"), _("You don't have the materials needed for producing more of this item.\n"));
			Cvar_SetValue("mn_production_amount", prod->amount);
			return;
		} else if (amount != producibleAmount) {
			Com_sprintf(productionPopup, lengthof(productionPopup), _("You don't have enough material to produce all (%i) additional items. Only %i could be added."), amount, producibleAmount);
			CP_Popup(_("Not enough material!"), productionPopup);
		}

		PR_IncreaseProduction(prod, producibleAmount);
		Cvar_SetValue("mn_production_amount", prod->amount);
	} else {
		const char *name = NULL;

		/* no free production slot */
		if (PR_QueueFreeSpace(base) <= 0) {
			CP_Popup(_("Not enough workshops"), _("You cannot queue more items.\nBuild more workshops.\n"));
			return;
		}

		tech = PR_GetTech(&selectedData);
		name = PR_GetName(&selectedData);

		producibleAmount = PR_RequirementsMet(amount, &tech->requireForProduction, base);
		if (producibleAmount == 0) {
			CP_Popup(_("Not enough materials"), _("You don't have the materials needed for producing this item.\n"));
			return;
		} else if (amount != producibleAmount) {
			Com_sprintf(productionPopup, lengthof(productionPopup), _("You don't have enough material to produce all (%i) items. Production will continue with a reduced (%i) number."), amount, producibleAmount);
			CP_Popup(_("Not enough material!"), productionPopup);
		}
		/** @todo
		 *  -) need to popup something like: "You need the following items in order to produce more of ITEM:   x of ITEM, x of ITEM, etc..."
		 *     This info should also be displayed in the item-info.
		 *  -) can can (if possible) change the 'amount' to a vlalue that _can_ be produced (i.e. the maximum amount possible).*/

		if (PR_IsAircraftData(&selectedData) && AIR_CalculateHangarStorage(selectedData.data.aircraft, base, 0) <= 0) {
			CP_Popup(_("Hangars not ready"), _("You cannot queue aircraft.\nNo free space in hangars.\n"));
			return;
		}

		/* add production */
		prod = PR_QueueNew(base, &selectedData, producibleAmount);

		/** @todo this popup hides any previous popup, like popup created in PR_QueueNew */
		if (!prod)
			return;

		/* Now we select the item we just created. */
		selectedProduction = prod;
		UI_ExecuteConfunc("prod_selectline %i", selectedProduction->idx);

		/* messages */
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Production of %s started"), _(name));
		MSO_CheckAddNewMessage(NT_PRODUCTION_STARTED, _("Production started"), cp_messageBuffer, qfalse, MSG_PRODUCTION, tech);
	}

	PR_ProductionInfo(base);
	PR_UpdateProductionList(base);
}

/**
 * @brief Stops the current running production
 */
static void PR_ProductionStop_f (void)
{
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();
	int prodIDX;

	if (!base || !selectedProduction)
		return;

	prodIDX = selectedProduction->idx;
	queue = PR_GetProductionForBase(base);

	PR_QueueDelete(base, queue, prodIDX);

	if (queue->numItems == 0) {
		selectedProduction = NULL;
		UI_ExecuteConfunc("prod_selectline -1");
	} else if (prodIDX >= queue->numItems) {
		selectedProduction = &queue->items[queue->numItems - 1];
		UI_ExecuteConfunc("prod_selectline %i", prodIDX);
	}

	PR_ProductionInfo(base);
	PR_UpdateProductionList(base);
}

/**
 * @brief Decrease the production amount by given parameter
 */
static void PR_ProductionDecrease_f (void)
{
	int amount = 1;
	const base_t *base = B_GetCurrentSelectedBase();
	production_t *prod = selectedProduction;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

	if (!prod)
		return;

	if (prod->amount <= amount) {
		PR_ProductionStop_f();
		return;
	}

	/** @todo add (confirmaton) popup in case storage cannot take all the items we add back to it */
	PR_DecreaseProduction(prod, amount);

	if (base) {
		PR_ProductionInfo(base);
		PR_UpdateProductionList(base);
	}
}

/**
 * @brief Change the production amount by given diff.
 */
static void PR_ProductionChange_f (void)
{
	int amount;

	if (!selectedProduction)
		return;

	if (!PR_IsDataValid(&selectedData))
		return;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <diff> : change the production amount\n", Cmd_Argv(0));
		return;
	}

	amount = atoi(Cmd_Argv(1));
	if (amount > 0) {
		Cbuf_AddText(va("prod_inc %i\n", amount));
	} else {
		Cbuf_AddText(va("prod_dec %i\n", -amount));
	}
}

/**
 * @brief shift the current production up the list
 */
static void PR_ProductionUp_f (void)
{
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || !selectedProduction)
		return;

	/* first position already */
	if (selectedProduction->idx == 0)
		return;

	queue = PR_GetProductionForBase(base);
	PR_QueueMove(queue, selectedProduction->idx, -1);

	selectedProduction = &queue->items[selectedProduction->idx - 1];
	UI_ExecuteConfunc("prod_selectline %i", selectedProduction->idx);
	PR_UpdateProductionList(base);
}

/**
 * @brief shift the current production down the list
 */
static void PR_ProductionDown_f (void)
{
	production_queue_t *queue;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || !selectedProduction)
		return;

	queue = PR_GetProductionForBase(base);

	if (selectedProduction->idx >= queue->numItems - 1)
		return;

	PR_QueueMove(queue, selectedProduction->idx, 1);

	selectedProduction = &queue->items[selectedProduction->idx + 1];
	UI_ExecuteConfunc("prod_selectline %i", selectedProduction->idx);
	PR_UpdateProductionList(base);
}

void PR_InitCallbacks (void)
{
	Cmd_AddCommand("prod_init", PR_ProductionList_f, NULL);
	Cmd_AddCommand("prod_type", PR_ProductionType_f, NULL);
	Cmd_AddCommand("prod_up", PR_ProductionUp_f, "Move production item up in the queue");
	Cmd_AddCommand("prod_down", PR_ProductionDown_f, "Move production item down in the queue");
	Cmd_AddCommand("prod_change", PR_ProductionChange_f, "Change production amount");
	Cmd_AddCommand("prod_inc", PR_ProductionIncrease_f, "Increase production amount");
	Cmd_AddCommand("prod_dec", PR_ProductionDecrease_f, "Decrease production amount");
	Cmd_AddCommand("prod_stop", PR_ProductionStop_f, "Stop production");
	Cmd_AddCommand("prodlist_rclick", PR_ProductionListRightClick_f, NULL);
	Cmd_AddCommand("prodlist_click", PR_ProductionListClick_f, NULL);
}

void PR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("prod_init");
	Cmd_RemoveCommand("prod_type");
	Cmd_RemoveCommand("prod_up");
	Cmd_RemoveCommand("prod_down");
	Cmd_RemoveCommand("prod_change");
	Cmd_RemoveCommand("prod_inc");
	Cmd_RemoveCommand("prod_dec");
	Cmd_RemoveCommand("prod_stop");
	Cmd_RemoveCommand("prodlist_rclick");
	Cmd_RemoveCommand("prodlist_click");
}
