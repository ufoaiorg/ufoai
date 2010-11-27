/**
 * @file ui_bindscript.cpp
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

static int eventTypeId;
static int nodeTypeId;
static int contextLine;
static const char* contextScript;

/**
 * @param behaviour
 * @return String inside @va() slot
 */
static const char* UI_GetScriptTypeFromBehaviour(const uiBehaviour_t *behaviour) {
	/** @todo rename abstract node and remove that */
	if (!strcmp(behaviour->name, ""))
		return NULL;
	if (!strcmp(behaviour->name, "abstractnode"))
		return "uinode";
	else
		return va("ui%s", behaviour->name);
}

extern "C" void UI_ParseActionScript (uiNode_t *node, const char *script, const char *fileName, int line)
{
	int r;

	contextScript = fileName;

	CScriptBuilder builder;
	r = builder.StartNewModule(engine, UI_GetPath(node));
	if( r < 0 ) return;

	if (node->behaviour->isFunction) {
		const char * castName = UI_GetScriptTypeFromBehaviour(node->parent->behaviour);
		script = va("%s@ get_thisNode() { return cast<%s>(__ui_thisNode); }\nvoid f() {\n%s\n}", castName, castName, script);
		contextLine = line - 3;
	} else {
		const char * castName = UI_GetScriptTypeFromBehaviour(node->behaviour);
		script = va("%s@ get_thisNode() { return cast<%s>(__ui_thisNode); }\n%s", castName, castName, script);
		contextLine = line - 2;
	}
	r = builder.AddSectionFromMemory(script);
	if( r < 0 ) return;

	r = builder.BuildModule();
	if( r < 0 )	{
		Com_Printf("UI_ParseActionScript: No way to build script module\n");
		return;
	}

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
#if 0
							Com_Printf("UI_ParseActionScript: Found %s\n", property->string);
#endif
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

	contextLine = 0;
}

extern "C" void UI_ExecuteScriptAction (const uiAction_t *action, uiCallContext_t *context)
{
	/* store thisNode in the stack */
	uiNode_t *oldNode = thisNode;
	int r;

	asIScriptContext *ctx = engine->CreateContext();
	if (ctx == NULL) {
		Com_Printf("Failed to create the context.\n");
		return;
	}

	asIScriptFunction *function = (asIScriptFunction*) action->d.terminal.d1.data;
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
			Com_Printf("func: %s ; %s ; %s\n", function->GetDeclaration(), function->GetModuleName(), function->GetScriptSectionName());
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

static uiNode_t **Node_GetNode(std::string name)
{
	/** @todo it must NOT be a global var, but i dont see a way to alloc holder in angelscript */
	static uiNode_t *holder;
	holder = UI_GetNodeByPath(name.c_str());
	return &holder;
}
static void Node_AddRef(uiNode_t *node)
{
	/** @todo count number of ref */
}
static void Node_Release(uiNode_t *node)
{
	/** @todo count number of ref */
}
static std::string Node_GetPath(const uiNode_t *node)
{
	return UI_GetPath(node);
}
static std::string Node_GetName(const uiNode_t *node)
{
	return node->name;
}
static std::string Node_GetType(const uiNode_t *node)
{
	return node->behaviour->name;
}

/** @todo must return a uiNode_t** to allow cascade uses on tmp node  */
static uiNode_t *Node_GetParent(uiNode_t *node)
{
	return node->parent;
}
/** @todo must return a uiNode_t** to allow cascade uses on tmp node  */
static uiNode_t *Node_GetFirstChild(uiNode_t *node)
{
	return node->firstChild;
}
/** @todo must return a uiNode_t** to allow cascade uses on tmp node  */
static uiNode_t *Node_GetNext(uiNode_t *node)
{
	return node->next;
}
/** @todo must return a uiNode_t** to allow cascade uses on tmp node */
static uiNode_t *Node_GetWindow(uiNode_t *node)
{
	return node->root;
}

typedef struct {
	/** @todo remove me, useless */
	const char *sanity;
	uiNode_t* node;
	uiAction_t** actions;
} uiEventHolder_t;

static void Node_GetEvent (asIScriptGeneric *gen)
{
	const int offset = (uintptr_t) gen->GetFunctionUserData();
	uiEventHolder_t *holder = (uiEventHolder_t*) engine->CreateScriptObject(eventTypeId);
	holder->sanity = "uiEventHolder_t";
	holder->node = static_cast<uiNode_t*>(gen->GetObject());
	holder->actions = (uiAction_t **) ((uintptr_t)holder->node + offset);
	gen->SetReturnAddress(holder);
}

static void Event_Construct (uiEventHolder_t* memory)
{
	memset(memory, 0, sizeof(uiEventHolder_t));
#if 0
	memory = new uiEventHolder_t;
#endif
}

static void Event_Destroy (uiEventHolder_t* memory)
{
#if 0
	delete memory;
#endif
}

static void Event_AddNode (uiEventHolder_t*holder, uiNode_t *function)
{
	uiAction_t* action;
	if (!function->behaviour->isFunction) {
		Com_Printf("Event_SetNode: Node must be a confunc/func behaviour");
		return;
	}
	action = UI_AllocListener(function);
	action->next = *(holder->actions);
	*(holder->actions) = action;
}

static void Event_RemoveNode (uiEventHolder_t*holder, uiNode_t *function)
{
	uiAction_t* action = *(holder->actions);
	if (!function->behaviour->isFunction) {
		Com_Printf("Event_SetNode: Node must be a confunc/func behaviour");
		return;
	}
	if (action && action->type == EA_LISTENER && action->d.terminal.d2.data == &function->onClick) {
		*(holder->actions) = action->next;
		UI_DeleteListener(action);
	} else {
		while (action->next) {
			if (action->next->type == EA_LISTENER && action->next->d.terminal.d2.data == &function->onClick) {
				uiAction_t *tmp = action->next;
				action->next = action->next->next;
				UI_DeleteListener(tmp);
				break;
			}
			action = action->next;
		}
	}
}

static void Event_SetNode (uiEventHolder_t*holder, uiNode_t *function)
{
	uiAction_t* action;
	if (!function->behaviour->isFunction) {
		Com_Printf("Event_SetNode: Node must be a confunc/func behaviour");
		return;
	}
	/** @todo Remember to freed the action list */
	action = UI_AllocListener(function);
	*(holder->actions) = action;
}

static void Event_Add (uiEventHolder_t*holder, asIScriptFunction *function)
{
	/** @todo Remember to freed it somewhere */
	uiAction_t* action = new uiAction_t;
	action->type = EA_SCRIPT;
	action->d.terminal.d1.data = function;
	action->next = *(holder->actions);
	*(holder->actions) = action;
}

static void Event_Remove (uiEventHolder_t*holder, asIScriptFunction *function)
{
	uiAction_t* action = *(holder->actions);
	if (action && action->type == EA_SCRIPT && action->d.terminal.d1.data == function) {
		/** freed if it is need */
		*(holder->actions) = action->next;
	} else {
		while (action->next) {
			if (action->next->type == EA_SCRIPT && action->next->d.terminal.d1.data == function) {
				/** freed if it is need */
				action->next = action->next->next;
				break;
			}
			action = action->next;
		}
	}
}

static void Event_Set (uiEventHolder_t*holder, asIScriptFunction *function)
{
	/** @todo Remember to freed the action list */
	uiAction_t* action = new uiAction_t;
	action->type = EA_SCRIPT;
	action->d.terminal.d1.data = function;
	action->next = NULL;
	*(holder->actions) = action;
}

static void Node_Execute (uiNode_t &node)
{
	if (!node.behaviour->isFunction) {
		Com_Printf("\"execute\" method is only available for confunc/func nodes");
		return;
	}
	UI_ExecuteEventActions(&node, node.onClick);
}

static void Event_Execute (const uiEventHolder_t &holder)
{
	UI_ExecuteEventActions(holder.node, *(holder.actions));
}

static void Node_OpAssignGeneric (asIScriptGeneric *gen)
{
	uiNode_t ** a = static_cast<uiNode_t **>(gen->GetArgObject(0));
	uiNode_t ** self = static_cast<uiNode_t **>(gen->GetObject());
	*self = *a;
	gen->SetReturnAddress(self);
}

static void Node_OpEqualsGeneric(asIScriptGeneric * gen)
{
	uiNode_t *a = static_cast<uiNode_t*>(gen->GetObject());
	uiNode_t **b = static_cast<uiNode_t**>(gen->GetArgAddress(0));
	*(bool*)gen->GetAddressOfReturnLocation() = (a == *b);
}

static void Node_ExecuteMethod(asIScriptGeneric *gen)
{
	uiNodeMethod_t func = (uiNodeMethod_t) ((intptr_t) gen->GetFunctionUserData());
	uiCallContext_t context;
	/** @todo fix random number of param */
	context.paramNumber = 0;

	context.params = NULL;
	context.source = (uiNode_t*) gen->GetObject();
	context.useCmdParam = qfalse;

	for (int i = 0; i < gen->GetArgCount(); i++) {
		std::string *value = static_cast<std::string*>(gen->GetArgObject(i));
		LIST_AddString(&context.params, value->c_str());
		context.paramNumber++;
	}

	func(context.source, &context);

	LIST_Delete(&context.params);
}

static void Node_Cast(asIScriptGeneric *gen)
{
	uiBehaviour_t* behaviour = (uiBehaviour_t*) gen->GetFunctionUserData();
	uiNode_t *node = (uiNode_t*) gen->GetObject();

	if (!node || !UI_NodeInstanceOfPointer(node, behaviour)) {
		gen->SetReturnAddress(NULL);
		return;
	}

	gen->SetReturnAddress(node);
}


static void MessageCallback(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if( msg->type == asMSGTYPE_WARNING )
		type = "WARN";
	else if( msg->type == asMSGTYPE_INFORMATION )
		type = "INFO";
#if 0
	Com_Printf("[ui angelscript] %s: %s (%d, %d) : %s : %s\n", contextScript, msg->section, msg->row + contextLine, msg->col, type, msg->message);
#else
	Com_Printf("[ui angelscript] %s line %d: [%s] %s\n", contextScript, msg->row + contextLine, type, msg->message);
#endif

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
	static char alreadyChecked[1024] = "|";
	char name[64] = "";
	int r;

	if (localBehaviour == NULL)
		return;

	UI_RegisterNodeBehaviour(engine, localBehaviour->super);

	const char *tmpName = UI_GetScriptTypeFromBehaviour(localBehaviour);
	if (tmpName == NULL)
		return;
	strncpy(name, tmpName, sizeof(name));

	/* check if we already compute the behaviour */
	/** @todo speed up that shit, it is a very bad way to do it */
	if (strstr(alreadyChecked, va("|%s|", name)) != NULL)
		return;
	strcat(alreadyChecked, name);
	strcat(alreadyChecked, "|");

	if (strcmp(name, "uinode") != 0) {
		r = engine->RegisterObjectType(name, 0, asOBJ_REF);
		assert(r >= 0);
	}
	r = engine->RegisterObjectBehaviour(name, asBEHAVE_ADDREF, "void f()", asFUNCTION(Node_AddRef), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectBehaviour(name, asBEHAVE_RELEASE, "void f()", asFUNCTION(Node_Release), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);

	for (const uiBehaviour_t *behaviour = localBehaviour; behaviour != NULL; behaviour = behaviour->super) {

		if (localBehaviour != behaviour) {
			asIScriptFunction *function;

			const char* superName = UI_GetScriptTypeFromBehaviour(behaviour);
			r = engine->RegisterObjectBehaviour(name, asBEHAVE_IMPLICIT_REF_CAST, va("%s@ f()", superName), asFUNCTION((Node_Cast)), asCALL_GENERIC);
			assert(r >= 0);
			function = engine->GetFunctionDescriptorById(r);
			function->SetUserData((void*)behaviour);

			r = engine->RegisterObjectBehaviour(superName, asBEHAVE_REF_CAST, va("%s@ f()", name), asFUNCTION((Node_Cast)), asCALL_GENERIC);
			assert(r >= 0);
			function = engine->GetFunctionDescriptorById(r);
			function->SetUserData((void*)localBehaviour);
		}

		const value_t *property = behaviour->properties;
		while (property && property->string != NULL) {
			switch ((int)property->type) {
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
				assert( r >= 0 );
				break;
			case V_COLOR:
				r = engine->RegisterObjectProperty(name, va("color %s", property->string), property->ofs);
				assert( r >= 0 );
				break;
			case V_UI_ACTION:
				{
					asIScriptFunction *function;
					r = engine->RegisterObjectMethod(name, va("event &get_%s()", property->string), asFUNCTION(Node_GetEvent), asCALL_GENERIC);
					assert( r >= 0 );
					function = engine->GetFunctionDescriptorById(r);
					function->SetUserData((void*)property->ofs);
				}
				break;
			case V_UI_NODEMETHOD:
				{
					asIScriptFunction *function;
					r = engine->RegisterObjectMethod(name, va("void %s()", property->string), asFUNCTION(Node_ExecuteMethod), asCALL_GENERIC);
					assert( r >= 0 );
					function = engine->GetFunctionDescriptorById(r);
					function->SetUserData((void*)property->ofs);
					r = engine->RegisterObjectMethod(name, va("void %s(const string& in)", property->string), asFUNCTION(Node_ExecuteMethod), asCALL_GENERIC);
					assert( r >= 0 );
					function = engine->GetFunctionDescriptorById(r);
					function->SetUserData((void*)property->ofs);
					r = engine->RegisterObjectMethod(name, va("void %s(const string, const string)", property->string), asFUNCTION(Node_ExecuteMethod), asCALL_GENERIC);
					assert( r >= 0 );
					function = engine->GetFunctionDescriptorById(r);
					function->SetUserData((void*)property->ofs);
					r = engine->RegisterObjectMethod(name, va("void %s(const string& in, const string& in, const string& in)", property->string), asFUNCTION(Node_ExecuteMethod), asCALL_GENERIC);
					assert( r >= 0 );
					function = engine->GetFunctionDescriptorById(r);
					function->SetUserData((void*)property->ofs);
				}
				break;
			default:
				break;
			}
			property++;
		}
	}

	r = engine->RegisterObjectMethod(name, va("%s@ &opAssign(const %s@ &in)", name, name), asFUNCTION(Node_OpAssignGeneric), asCALL_GENERIC);
	assert(r >= 0);
	r = engine->RegisterObjectMethod(name, va("bool opEquals(const %s@ &in) const", name), asFUNCTION(Node_OpEqualsGeneric), asCALL_GENERIC);
	assert(r >= 0);
	r = engine->RegisterObjectMethod(name, va("%s@ get_firstChild()", name), asFUNCTION(Node_GetFirstChild), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod(name, va("%s@ get_next()", name), asFUNCTION(Node_GetNext), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod(name, va("%s@ get_parent()", name), asFUNCTION(Node_GetParent), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod(name, va("%s@ get_window()", name), asFUNCTION(Node_GetWindow), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod(name, va("const string get_type()"), asFUNCTION(Node_GetType), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod(name, va("const string get_name()"), asFUNCTION(Node_GetName), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod(name, va("const string get_path()"), asFUNCTION(Node_GetPath), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
}

extern "C" void UI_InitBindScript (void)
{
	int r;
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
	RegisterStdString(engine);

	/* Function definition */

	r = engine->RegisterFuncdef("void eventdef()");
	assert(r >= 0);

	/* Color */

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

	r = engine->RegisterObjectType("uinode", 0, asOBJ_REF);
	assert(r >= 0);
	r = engine->RegisterGlobalFunction("uinode@ &getNode(string)", asFUNCTION(Node_GetNode), asCALL_CDECL);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("uinode", "void execute()", asFUNCTION(Node_Execute), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
	nodeTypeId = engine->GetTypeIdByDecl("uinode");
	assert( nodeTypeId >= 0 );

	/* Event */

	r = engine->RegisterObjectType("event", sizeof(uiEventHolder_t), asOBJ_VALUE);
	assert(r >= 0);
	eventTypeId = engine->GetTypeIdByDecl("event");
	assert( eventTypeId >= 0 );

	r = engine->RegisterObjectBehaviour("event", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Event_Construct), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectBehaviour("event", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Event_Destroy), asCALL_CDECL_OBJFIRST);
	assert(r >= 0);
	r = engine->RegisterObjectMethod("event", "void execute()", asFUNCTION(Event_Execute), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );

	r = engine->RegisterObjectMethod("event", "void set(eventdef@)", asFUNCTION(Event_Set), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
	r = engine->RegisterObjectMethod("event", "void add(eventdef@)", asFUNCTION(Event_Add), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
	r = engine->RegisterObjectMethod("event", "void remove(eventdef@)", asFUNCTION(Event_Remove), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
	r = engine->RegisterObjectMethod("event", "void set(uinode@)", asFUNCTION(Event_SetNode), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
	r = engine->RegisterObjectMethod("event", "void add(uinode@)", asFUNCTION(Event_AddNode), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
	r = engine->RegisterObjectMethod("event", "void remove(uinode@)", asFUNCTION(Event_RemoveNode), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );

#if 0
	/** @todo opcode overwrite dont work at all */
	r = engine->RegisterObjectMethod("event", "void opAssign(eventdef@)", asFUNCTION(Event_Set), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
	r = engine->RegisterObjectMethod("event", "void opAddAssign(eventdef@)", asFUNCTION(Event_Add), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
	r = engine->RegisterObjectMethod("event", "void opSubAssign(eventdef@)", asFUNCTION(Event_Remove), asCALL_CDECL_OBJFIRST);
	assert( r >= 0 );
#endif

	/* Node behaviour */

	for (int i = 0; i < UI_GetNodeBehaviourCount(); i++) {
		const uiBehaviour_t *behaviour = UI_GetNodeBehaviourByIndex(i);
		UI_RegisterNodeBehaviour(engine, behaviour);
	}

	/* Cvar */

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

	r = engine->RegisterGlobalProperty("uinode@ __ui_thisNode", &thisNode);
	assert( r >= 0 );
}

extern "C" void UI_ShutdownBindScript (void)
{
	engine->Release();
}
