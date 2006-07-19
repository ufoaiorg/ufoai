/*
==============================================================

CMD

Command text buffering and command execution

==============================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/

#define	EXEC_NOW	0			/* don't return until completed */
#define	EXEC_INSERT	1			/* insert at current position, but don't run yet */
#define	EXEC_APPEND	2			/* add to end of the command buffer */

void Cbuf_Init(void);

/* allocates an initial text buffer that will grow as needed */

void Cbuf_AddText(char *text);

/* as new commands are generated from the console or keybindings, */
/* the text is added to the end of the command buffer. */

void Cbuf_InsertText(char *text);

/* when a command wants to issue other commands immediately, the text is */
/* inserted at the beginning of the buffer, before any remaining unexecuted */
/* commands. */

void Cbuf_ExecuteText(int exec_when, char *text);

/* this can be used in place of either Cbuf_AddText or Cbuf_InsertText */

void Cbuf_AddEarlyCommands(qboolean clear);

/* adds all the +set commands from the command line */

qboolean Cbuf_AddLateCommands(void);

/* adds all the remaining + commands from the command line */
/* Returns true if any late commands were added, which */
/* will keep the demoloop from immediately starting */

void Cbuf_Execute(void);

/* Pulls off \n terminated lines of text from the command buffer and sends */
/* them through Cmd_ExecuteString.  Stops when the buffer is empty. */
/* Normally called once per frame, but may be explicitly invoked. */
/* Do not call inside a command function! */

void Cbuf_CopyToDefer(void);
void Cbuf_InsertFromDefer(void);

/* These two functions are used to defer any pending commands while a map */
/* is being loaded */

/*=========================================================================== */

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void (*xcommand_t) (void);

typedef struct cmdList_s {
	char *name;
	xcommand_t function;
} cmdList_t;

void Cmd_Init(void);

void Cmd_AddCommand(char *cmd_name, xcommand_t function);

/* called by the init functions of other parts of the program to */
/* register commands and functions to call for them. */
/* The cmd_name is referenced later, so it should not be in temp memory */
/* if function is NULL, the command will be forwarded to the server */
/* as a clc_stringcmd instead of executed locally */
void Cmd_RemoveCommand(char *cmd_name);

qboolean Cmd_Exists(char *cmd_name);

/* used by the cvar code to check for cvar / command name overlap */

char *Cmd_CompleteCommand(char *partial);

/* attempts to match a partial command for automatic command line completion */
/* returns NULL if nothing fits */

int Cmd_Argc(void);
char *Cmd_Argv(int arg);
char *Cmd_Args(void);

/* The functions that execute commands get their parameters with these */
/* functions. Cmd_Argv () will return an empty string, not a NULL */
/* if arg > argc, so string operations are always safe. */

void Cmd_TokenizeString(char *text, qboolean macroExpand);

/* Takes a null terminated string.  Does not need to be /n terminated. */
/* breaks the string up into arg tokens. */

void Cmd_ExecuteString(char *text);

/* Parses a single line of text into arguments and tries to execute it */
/* as if it was typed at the console */

void Cmd_ForwardToServer(void);

/* adds the current command line as a clc_stringcmd to the client message. */
/* things like godmode, noclip, etc, are commands directed to the server, */
/* so when they are typed in at the console, they will need to be forwarded. */


