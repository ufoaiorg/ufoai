/**
 * @file
 * @brief Aircraft and item components
 */

/*
Copyright (C) 2002-2018 UFO: Alien Invasion.

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
#include "../../../shared/parse.h"
#include "cp_campaign.h"

/** components type parsing helper */
struct component_type_data_t {
	char id[MAX_VAR];			/**< id of the campaign */
	char amount[MAX_VAR];			/**< placeholder for gettext stuff */
	char numbercrash[MAX_VAR];			/**< geoscape map */
};

/** components type values */
static const value_t components_type_vals[] = {
	{"id", V_STRING, offsetof(component_type_data_t, id), 0},
	{"amount", V_STRING, offsetof(component_type_data_t, amount), 0},
	{"numbercrash", V_STRING, offsetof(component_type_data_t, numbercrash), 0},
	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Parses one "components" entry in a .ufo file and writes it into the next free entry in xxxxxxxx (components_t).
 * @param[in] name The unique id of a components_t array entry.
 * @param[in] text the whole following text after the "components" definition.
 * @sa CP_ParseScriptFirst
 */
void COMP_ParseComponents (const char* name, const char** text)
{
	components_t* comp;
	const char* errhead = "COMP_ParseComponents: unexpected end of file.";
	const char* token;

	/* get body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		cgi->Com_Printf("COMP_ParseComponents: \"%s\" components def without body ignored.\n", name);
		return;
	}
	if (ccs.numComponents >= MAX_ASSEMBLIES) {
		cgi->Com_Printf("COMP_ParseComponents: too many technology entries. limit is %i.\n", MAX_ASSEMBLIES);
		return;
	}

	/* New components-entry (next free entry in global comp-list) */
	comp = &ccs.components[ccs.numComponents];
	ccs.numComponents++;

	OBJZERO(*comp);

	/* name is not used */

	do {
		/* get the name type */
		token = cgi->Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* get values */
		if (Q_streq(token, "aircraft")) {
			token = cgi->Com_EParse(text, errhead, name);
			if (!*text)
				break;

			/* set standard values */
			Q_strncpyz(comp->assemblyId, token, sizeof(comp->assemblyId));
			comp->assemblyItem = INVSH_GetItemByIDSilent(comp->assemblyId);
			if (comp->assemblyItem)
				cgi->Com_DPrintf(DEBUG_CLIENT, "COMP_ParseComponents: linked item: %s with components: %s\n", token, comp->assemblyId);
		} else if (Q_streq(token, "item")) {
			/* Defines what items need to be collected for this item to be researchable. */
			if (comp->numItemtypes < MAX_COMP) {
				/* Parse block */
				component_type_data_t itemTokens;
				OBJZERO(itemTokens);
				if (cgi->Com_ParseBlock ("item", text, &itemTokens, components_type_vals, nullptr)) {
					if (itemTokens.id[0] == '\0')
						cgi->Com_Error(ERR_DROP, "COMP_ParseComponents: \"item\" token id is missing.\n");
					if (itemTokens.amount[0] == '\0')
						cgi->Com_Error(ERR_DROP, "COMP_ParseComponents: \"amount\" token id is missing.\n");
					if (itemTokens.numbercrash[0] == '\0')
						cgi->Com_Error(ERR_DROP, "COMP_ParseComponents: \"numbercrash\" token id is missing.\n");

					comp->items[comp->numItemtypes] = INVSH_GetItemByID(itemTokens.id);	/* item id -> item pointer */

					/* Parse number of items. */
					comp->itemAmount[comp->numItemtypes] = atoi(itemTokens.amount);
					/* If itemcount needs to be scaled */
					if (itemTokens.numbercrash[0] == '%')
						comp->itemAmount2[comp->numItemtypes] = COMP_ITEMCOUNT_SCALED;
					else
						comp->itemAmount2[comp->numItemtypes] = atoi(itemTokens.numbercrash);

					/** @todo Set item links to NONE if needed */
					/* comp->item_idx[comp->numItemtypes] = xxx */

					comp->numItemtypes++;
				}
			} else {
				cgi->Com_Printf("COMP_ParseComponents: \"%s\" Too many 'items' defined. Limit is %i - ignored.\n", name, MAX_COMP);
			}
		} else if (Q_streq(token, "time")) {
			/* Defines how long disassembly lasts. */
			token = Com_Parse(text);
			comp->time = atoi(token);
		} else {
			cgi->Com_Printf("COMP_ParseComponents: Error in \"%s\" - unknown token: \"%s\".\n", name, token);
		}
	} while (*text);

	if (comp->assemblyId[0] == '\0') {
		cgi->Com_Error(ERR_DROP, "COMP_ParseComponents: component \"%s\" is not applied to any aircraft.\n", name);
	}
}

/**
 * @brief Returns components definition by ID.
 * @param[in] id assemblyId of the component definition.
 * @return Pointer to @c components_t definition.
 */
components_t* COMP_GetComponentsByID (const char* id)
{
	for (int i = 0; i < ccs.numComponents; i++) {
		components_t* comp = &ccs.components[i];
		if (Q_streq(comp->assemblyId, id)) {
			return comp;
		}
	}
	cgi->Com_Error(ERR_DROP, "COMP_GetComponentsByID: could not find components id for: %s", id);
}
