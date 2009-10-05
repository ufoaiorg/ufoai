/**
 * @file exec.h
 * @author luke_biddell@yahoo.com
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _EXEC_H_
#define _EXEC_H_

#include <string>
#include <gtk/gtk.h>

/** @brief Callback from within the process */
typedef void (*ExecFunc) (void *, void*);

typedef enum
{
	RUNNING = 0, SKIPPED, CANCELLED, FAILED, COMPLETED
} ExecState;

typedef struct
{
		GPtrArray *args;
		GPid pid;
		gint exit_code;
		ExecState state;
		GMutex *state_mutex;
		ExecFunc lib_proc;
		ExecFunc pre_proc;
		ExecFunc read_proc; /**< read the output of the process from stdout */
		ExecFunc post_proc;
		gchar *working_dir;
		gboolean piped;
		gpointer exec;
		gboolean parse_progress;
} ExecCmd;

typedef struct
{
		gchar *process_title;
		gchar *process_description;
		GList *cmds;
		GError *err;
		gdouble fraction;
		ExecState outcome;
} Exec;

Exec *exec_new (const gchar *process_title, const gchar *process_description);
ExecCmd *exec_cmd_new (Exec **e);
void exec_delete (Exec *self);
void exec_run (Exec *e);
void exec_stop (Exec *e);
void exec_cmd_add_arg (ExecCmd *e, const gchar *format, ...);
void exec_cmd_update_arg (ExecCmd *e, const gchar *arg_start, const gchar *format, ...);
gint exec_run_cmd (const std::string& cmd, gchar **output, const std::string& working_dir);
ExecState exec_cmd_get_state (ExecCmd *e);
ExecState exec_cmd_set_state (ExecCmd *e, ExecState state);
gint exec_count_operations (const Exec *e);
GList* exec_get_cmd_list (void);

#endif	/*_EXEC_H_*/
