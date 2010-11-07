/**
 * @file ui_bindscript.cpp
 * @todo a way to cast nodes by reusing know inheritances
 * @todo a way to execute functions/events
 * @todo a way to redefine functions/events
 * @todo a way to add/remove listener functions/events
 * @todo a way to execute node methods (optional, but can be useful)
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../../common/angelscript/angelscript.h"
#include "../../common/angelscript/addons/scriptbuilder.h"
#include "../../common/angelscript/addons/scriptstdstring.h"

extern "C" {

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_bindscript.h"
#include "ui_parse.h"
#include "node/ui_node_abstractnode.h"

#include "../client.h"

}/* extern "C" */


static asIScriptEngine *engine;
static uiNode_t *thisNode;

/**
 * @param behaviour
 * @return String inside @va() slot
 */
static const char* UI_GetScriptTypeFromBehaviour(const uiBehaviour_t *behaviour) {
	/** @todo rename abstract node and remove that */
	if (!strcmp(behaviour->name, ""))
		return NULL;
	if (!strcmp(behaviour->name, "abstractnode"))
		return "node";
	else
		return va("wg%s", behaviour->name);
}

extern "C" void UI_ParseActionScript (uiNode_t *node, const char *script)
{
	int r;

	CScriptBuilder builder;
	r = builder.StartNewModule(engine, UI_GetPath(node));
	if( r < 0 ) return;

	if (node->behaviour->isFunction) {
		script = va("%s@ thisnode;\nvoid f() {\n%s\n}", UI_GetScriptTypeFromBehaviour(node->behaviour), script);
	} else {
		script = va("%s@ thisnode;\n%s", UI_GetScriptTypeFromBehaviour(node->behaviour), script);
	}
	r = builder.AddSectionFromMemory(script);
	if( r < 0 ) return;

	r = builder.BuildModule();
	if( r < 0 )	{
		Com_Printf("UI_ParseActionScript: No way to build script module\n");
		return;
	}

	Com_Printf("UI_ParseActionScript: Script successfully built\n");
	asIScriptModule *module = engine->GetModule(UI_GetPath(node));

	/* link all node event to relative scripts */
	const uiBehaviour_t *behaviour = node->behaviour;
	if (node->behaviour->isFunction) {
		const int functionId = module->GetFunctionIdByName("f");
		asIScriptFunction *function = module->GetFunctionDescriptorById(functionId);
		if (function != NULL) {
			uiAction_t *action = UI_AllocStaticAction();
			action->type = EA_SCRIPT;
			action->d.terminal.d1.data = function;
			node->onClick = action;
		}
	} else {
		while (behaviour) {
			const value_t *property = behaviour->properties;
			while(property && property->string != NULL) {
				if (property->type == V_UI_ACTION) {
					const int functionId = module->GetFunctionIdByName(property->string);
					if (functionId >= 0) {
						asIScriptFunction *function = module->GetFunctionDescriptorById(functionId);
						if (function != NULL) {
							Com_Printf("UI_ParseActionScript: Found %s\n", property->string);
							uiAction_t *action = UI_AllocStaticAction();
							action->type = EA_SCRIPT;
							action->d.terminal.d1.data = function;
							uiAction_t **event = (uiAction_t **) (((uintptr_t)node) + property->ofs);
							if (*event) {
								Com_Printf("UI_ParseActionScript: %s function not linked\n", property->string);
							} else {
								*event = action;
							}
						}
					}
				}
				property++;
			}
			behaviour = behaviour->super;
		}
	}
}

extern "C" void UI_ExecuteScriptAction (const uiAction_t *action, uiCallContext_t *context)
{
	uiNode_t *oldNode = thisNode;
	int r;
	asIScriptFunction *function = (asIScriptFunction*) action->d.terminal.d1.data;

	asIScriptContext *ctx = engine->CreateContext();
	if (ctx == NULL) {
		Com_Printf("Failed to create the context.\n");
		return;
	}

	r = ctx->Prepare(function->GetId());
	if (r < 0) {
		Com_Printf("Failed to prepare the context.\n");
		ctx->Release();
		return;
	}

	/** allow the script to access to the current node, node (con)func is func of a node (it use parent node) */
	if (context->source->behaviour->isFunction) {
		thisNode = context->source->parent;
	} else {
		thisNode = context->source;
	}

	r = ctx->Execute();
	if (r != asEXECUTION_FINISHED) {
		// The execution didn't finish as we had planned. Determine why.
		if (r == asEXECUTION_ABORTED) {
			Com_Printf("The script was aborted before it could finish. Probably it timed out.\n");
		} else if (r == asEXECUTION_EXCEPTION) {
			Com_Printf("The script ended with an exception.\n");

			// Write some information about the script exception
			/*
			cout << "func: " << function->GetDeclaration() << endl;
			cout << "modl: " << function->GetModuleName() << endl;
			cout << "sect: " << function->GetScriptSectionName() << endl;
			 */
			Com_Printf("Description: %s (line: %i)\n", ctx->GetExceptionString(), ctx->GetExceptionLineNumber());
		} else {
			Com_Printf("The script ended for some unforeseen reason (%i)\n", r);
		}
	}

	thisNode = oldNode;

	// We must release the contexts when no longer using them
	ctx->Release();
}

static void CvarRef_SetString(cvar_t *cvar, std::string value) {
	Cvar_Set(cvar->name, value.c_str());
}
static const std::string CvarRef_GetString(cvar_t *cvar) {
	return cvar->string;
}
static int CvarRef_GetInteger(cvar_t *cvar) {
	return cvar->integer;
}
static float CvarRef_GetNumber(cvar_t *cvar) {
	return cvar->value;
}
static void CvarRef_SetInteger(cvar_t *cvar, int value) {
	Cvar_SetValue(cvar->name, value);
}
static void CvarRef_SetNumber(cvar_t *cvar, float value) {
	Cvar_SetValue(cvar->name, value);
}
static cvar_t *CvarRef_GetCvar(std::string name) {
	return Cvar_Get(name.c_str(), "", 0, "Script generated");
}
static void CvarRef_AddRef(cvar_t *cvar) {
	/** @todo count number of ref */
}
static void CvarRef_Release(cvar_t *cvar) {
	/** @todo count number of ref */
}


static uiNode_t *Node_GetNode(std::string name) {
	return UI_GetNodeByPath(name.c_str());
}
static void Node_AddRef(uiNode_t *node) {
	/** @todo count number of ref */
}
static void Node_Release(uiNode_t *node) {
	/** @todo count number of ref */
}
static std::string Node_GetPath(const uiNode_t *node) {
	return UI_GetPath(node);
}
static std::string Node_GetName(const uiNode_t *node) {
	return node->name;
}
static std::string Node_GetType(const uiNode_t *node) {
	return node->behaviour->name;
}
static uiNode_t *Node_GetParent(uiNode_t *node) {
	return node->parent;
}
static uiNode_t *Node_GetFirstChild(uiNode_t *node) {
	return node->firstChild;
}
static uiNode_t *Node_GetNext(uiNode_t *node) {
	return node->next;
}
static uiNode_t *Node_GetWindow(uiNode_t *node) {
	return node->root;
}

static void Node_OpAssignGeneric(asIScriptGeneric *gen) {
	uiNode_t ** a = static_cast<uiNode_t **>(gen->GetArgObject(0));
	uiNode_t ** self = static_cast<uiNode_t **>(gen->GetObject());
	*self = *a;
	gen->SetReturnAddress(self);
}

static void Node_OpEqualsGeneric(asIScriptGeneric * gen) {
	uiNode_t *a = static_cast<uiNode_t*>(gen->GetObject());
	uiNode_t **b = static_cast<uiNode_t**>(gen->GetArgAddress(0));
	*(bool*)gen->GetAddressOfReturnLocation() = (a == *b);
}

template<uiBehaviour_t *NEW>
uiNode_t *Node_Cast(uiNode_t* a)
{
	if (!a)
		return NULL;

	if (!UI_NodeInstanceOfPointer(a, NEW))
		return NULL;

	/** @todo Here we must count another AddRef */

	return a;
}


static void MessageCallback(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if( msg->type == asMSGTYPE_WARNING )
		type = "WARN";
	else if( msg->type == asMSGTYPE_INFORMATION )
		type = "INFO";
	Com_Printf("AngelScript: %s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

/**
 * Convert a string to a color
 * The script should copy the result to a value then we can use static here
 */
static vec4_t &AssignStringToColor(vec4_t &color, const std::string &s)
{
	int result;
	size_t byte;
	result = Com_ParseValue(color, s.c_str(), V_COLOR, 0, sizeof(color), &byte);
	if (result != RESULT_OK) {
		Com_Printf("AssignStringToColor: Color unset (%s)\n", s.c_str());
	}
	return color;
}

static void UI_RegisterNodeBehaviour(asIScriptEngine *engine, const uiBehaviour_t *localBehaviour)
{
	char name[64] = "";
	int r;

	if (localBehaviour == NULL)
		return;

	UI_RegisterNodeBehaviour(engine, localBehaviour->super);

	const char *tmpName = UI_GetScriptTypeFromBehaviour(localBehaviour);
	if (tmpName == NULL)
		return;
	strncpy(name, tmpName, sizeof(name));

	/** @todo remove it, it create warnings on logs */
	if (engine->GetTypeIdByDecl(name) != asINVALID_TYPE)
		return;

	r = engine->RegisterObjectType(name, 0, asOBJ_REF);
	assert(r >= 0);
	r = engine->RegisterObjectBehaviour(name, asBEHAVE_ADDREF, "void f()", asFUNCTION(Node_AddRef), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectBehaviour(name, asBEHAVE_RELEASE, "void f()", asFUNCTION(Node_Release), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);

	for (const uiBehaviour_t *behaviour = localBehaviour; behaviour != NULL; behaviour = behaviour->super) {

		if (localBehaviour != behaviour) {
/*			const char* superName = UI_GetScriptTypeFromBehaviour(behaviour);
			r = engine->RegisterObjectBehaviour(name, asBEHAVE_IMPLICIT_REF_CAST, va("%s@ f()", superName), asFUNCTION((Node_Cast<behaviour>)), asCALL_CDECL_OBJLAST);
			assert(r >= 0);
			r = engine->RegisterObjectBehaviour(superName, asBEHAVE_REF_CAST, va("%s@ f()", name), asFUNCTION((Node_Cast<localBehaviour>)), asCALL_CDECL_OBJLAST);
			assert(r >= 0);
*/		}

		const value_t *property = behaviour->properties;
		while (property && property->string != NULL) {
			switch (property->type) {
			case V_BOOL:
				/** @todo anbiguous, out bool type is bigger than 8bits; maybe we can't use prop here */
				r = engine->RegisterObjectProperty(name, va("bool %s", property->string), property->ofs);
				assert( r >= 0 );
				break;
			case V_INT:
				r = engine->RegisterObjectProperty(name, va("int %s", property->string), property->ofs);
				assert( r >= 0 );
				break;
			case V_FLOAT:
				r = engine->RegisterObjectProperty(name, va("float %s", property->string), property->ofs);
				Com_Printf("--> %s", property->string);
				assert( r >= 0 );
				break;
			case V_COLOR:
				r = engine->RegisterObjectProperty(name, va("color %s", property->string), property->ofs);
				assert( r >= 0 );
				break;
			default:
				break;
			}
			property++;
		}
	}
}

extern "C" void UI_InitBindScript (void)
{
	int r;
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
	RegisterStdString(engine);

	/* color */

	r = engine->RegisterObjectType("color", sizeof(vec4_t), asOBJ_VALUE | asOBJ_POD);
	assert(r >= 0);
	r = engine->RegisterObjectProperty("color", "float red", sizeof(vec_t)*0);
	assert( r >= 0 );
	r = engine->RegisterObjectProperty("color", "float green", sizeof(vec_t)*1);
	assert( r >= 0 );
	r = engine->RegisterObjectProperty("color", "float blue", sizeof(vec_t)*2);
	assert( r >= 0 );
	r = engine->RegisterObjectProperty("color", "float alpha", sizeof(vec_t)*3);
	assert( r >= 0 );
	r = engine->RegisterObjectMethod("color", "color &opAssign(const string &in)", asFUNCTION(AssignStringToColor), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );

	/* Node */

	for (int i = 0; i < UI_GetNodeBehaviourCount(); i++) {
		const uiBehaviour_t *behaviour = UI_GetNodeBehaviourByIndex(i);
		UI_RegisterNodeBehaviour(engine, behaviour);
	}

	r = engine->RegisterObjectMethod("node", "node@ &opAssign(const node@ &in)", asFUNCTION(Node_OpAssignGeneric), asCALL_GENERIC);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("node", "bool opEquals(const node@ &in) const", asFUNCTION(Node_OpEqualsGeneric), asCALL_GENERIC);
	assert(r >= 0);
	r = engine->RegisterGlobalFunction("node@ getNode(string)", asFUNCTION(Node_GetNode), asCALL_CDECL);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("node", "node@ get_firstChild()", asFUNCTION(Node_GetFirstChild), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("node", "node@ get_next()", asFUNCTION(Node_GetNext), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("node", "node@ get_parent()", asFUNCTION(Node_GetParent), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("node", "node@ get_window()", asFUNCTION(Node_GetWindow), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("node", "const string get_type()", asFUNCTION(Node_GetType), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("node", "const string get_name()", asFUNCTION(Node_GetName), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("node", "const void get_path(string)", asFUNCTION(Node_GetPath), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);

	/* CVAR */

	r = engine->RegisterObjectType("cvar", 0, asOBJ_REF);
	assert(r >= 0);
	r = engine->RegisterObjectBehaviour("cvar", asBEHAVE_ADDREF, "void f()", asFUNCTION(CvarRef_AddRef), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectBehaviour("cvar", asBEHAVE_RELEASE, "void f()", asFUNCTION(CvarRef_Release), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterGlobalFunction("cvar@ getCvar(string)", asFUNCTION(CvarRef_GetCvar), asCALL_CDECL);
	assert(r >= 0);

	r = engine->RegisterObjectMethod("cvar", "const string get_string()", asFUNCTION(CvarRef_GetString), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("cvar", "void set_string(string)", asFUNCTION(CvarRef_SetString), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("cvar", "int get_integer()", asFUNCTION(CvarRef_GetInteger), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("cvar", "float get_number()", asFUNCTION(CvarRef_GetNumber), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("cvar", "void set_integer(int)", asFUNCTION(CvarRef_SetInteger), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("cvar", "void set_number(float)", asFUNCTION(CvarRef_SetNumber), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);

	r = engine->RegisterGlobalProperty("node@ thisNode", &thisNode);
	assert( r >= 0 );
}

extern "C" void UI_ShutdownBindScript (void)
{
	engine->Release();
}
