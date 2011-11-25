/**
 * @file exec.cpp
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

#include "exec.h"
#include <glib/gprintf.h>
#include <string.h>
#include <unistd.h>

#include "sidebar/JobInfo.h"

#if defined (__FreeBSD__) || defined(__OpenBSD__) || defined(__linux__)
# if defined (__FreeBSD__) || defined(__OpenBSD__)
#  include <signal.h>
# endif
# include <sys/wait.h>
#endif

#ifdef _WIN32
#define pipe(childpipe) 0
#define waitpid(pid,exitcode,flags) 0
#define kill(pid, signal) 0
#define SIGQUIT 0
# ifndef SIGTERM
# define SIGTERM 0
# endif
#define SIGKILL 0
#define WNOHANG 0
#elif defined (__APPLE__) || defined (MACOSX)
#define waitpid(pid,exitcode,flags) 0
#define WNOHANG 0
#endif

static gint child_child_pipe[2];
static gboolean show_trace = TRUE;

static void exec_print_cmd (const ExecCmd *e)
{
	g_return_if_fail(e != NULL);
	if (show_trace) {
		guint i = 0;
		/* args are null terminated */
		for (; i < e->args->len - 1; i++)
			g_debug("- %s\n", (gchar*) g_ptr_array_index(e->args, i));
	}
}

static void exec_set_outcome (Exec *e)
{
	e->outcome = COMPLETED;
	GList *cmd = e->cmds;
	for (; cmd != NULL; cmd = cmd->next) {
		const ExecState state = exec_cmd_get_state((ExecCmd*) cmd->data);
		if (state == CANCELLED) {
			e->outcome = CANCELLED;
			break;
		} else if (state == FAILED) {
			e->outcome = FAILED;
			break;
		}
	}
}

static gboolean exec_channel_callback (GIOChannel *channel, GIOCondition condition, gpointer data)
{
	ExecCmd *cmd = (ExecCmd*) data;
	gboolean cont = TRUE;

	/* there's data to be read */
	if ((condition & G_IO_IN) || (condition & G_IO_PRI)) {
		static const gint BUFF_SIZE = 1024;
		gchar buffer[BUFF_SIZE];
		memset(buffer, 0, sizeof(buffer));
		gsize bytes = 0;
		const GIOStatus status =
				g_io_channel_read_chars(channel, buffer, (BUFF_SIZE - 1) * sizeof(gchar), &bytes, NULL);
		/* need to check what to do for again */
		if ((status == G_IO_STATUS_ERROR) || (status == G_IO_STATUS_AGAIN)) {
			g_warning("exec_channel_callback - read error [%d]\n", status);
			cont = FALSE;
		} else if (cmd->read_proc) {
			GError *error = NULL;
			const gchar* to_codeset = "UTF-8";
			const gchar* from_codeset = "ISO-8859-1";
			gchar* fallback = NULL;
			gchar *converted = g_convert_with_fallback(buffer, bytes, to_codeset, from_codeset, fallback, NULL, NULL,
					&error);
			if (converted != NULL) {
				cmd->read_proc(cmd, converted);
				g_free(converted);
			} else {
				if (error != NULL) {
					g_warning("exec_channel_callback - conversion error [%s]", error->message);
					g_error_free(error);
				} else
					g_warning("exec_channel_callback - unknown conversion error");
				cmd->read_proc(cmd, buffer);
			}
		}
	}

	if ((cont == FALSE) || (condition & G_IO_HUP) || (condition & G_IO_ERR) || (condition & G_IO_NVAL)) {
		/* We assume a failure here (even on G_IO_HUP) as exec_spawn_process will
		 * check the return code of the child to determine if it actually worked
		 * and set the correct state accordingly.*/
		exec_cmd_set_state(cmd, FAILED);
		g_debug("exec_channel_callback - condition [%d]\n", condition);
		cont = FALSE;
	}

	return cont;
}

static void exec_spawn_process (ExecCmd *e, GSpawnChildSetupFunc child_setup)
{
	/* Make sure that the args are null terminated */
	g_ptr_array_add(e->args, NULL);
	exec_print_cmd(e);
	gint std_out = 0, std_err = 0;
	GError *err = NULL;
	const GSpawnFlags flags = (GSpawnFlags) (G_SPAWN_SEARCH_PATH | G_SPAWN_LEAVE_DESCRIPTORS_OPEN
			| G_SPAWN_DO_NOT_REAP_CHILD);
	if (g_spawn_async_with_pipes(NULL, (gchar**) e->args->pdata, NULL, flags, child_setup, e, &e->pid, NULL, &std_out,
			&std_err, &err)) {
#ifdef _WIN32
		g_debug("exec_spawn_process - spawed process with pid [%p]\n", e->pid);
#else
		g_debug("exec_spawn_process - spawed process with pid [%d]\n", e->pid);
#endif
		GIOChannel *char_out = NULL, *chan_err = NULL;
		guint chan_out_id = 0, chan_err_id = 0;

		if (!e->piped) {
			char_out = g_io_channel_unix_new(std_out);
			g_io_channel_set_encoding(char_out, NULL, NULL);
			g_io_channel_set_buffered(char_out, FALSE);
			g_io_channel_set_flags(char_out, G_IO_FLAG_NONBLOCK, NULL);
			chan_out_id = g_io_add_watch(char_out,
					(GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI | G_IO_NVAL), exec_channel_callback,
					(gpointer) e);

			chan_err = g_io_channel_unix_new(std_err);
			g_io_channel_set_encoding(chan_err, NULL, NULL);
			g_io_channel_set_buffered(chan_err, FALSE);
			g_io_channel_set_flags(chan_err, G_IO_FLAG_NONBLOCK, NULL);
			chan_err_id = g_io_add_watch(chan_err,
					(GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI | G_IO_NVAL), exec_channel_callback,
					(gpointer) e);

			while (exec_cmd_get_state(e) == RUNNING)
				gtk_main_iteration();

			g_source_remove(chan_out_id);
			g_source_remove(chan_err_id);
			g_io_channel_shutdown(char_out, FALSE, NULL);
			g_io_channel_unref(char_out);
			g_io_channel_shutdown(chan_err, FALSE, NULL);
			g_io_channel_unref(chan_err);
		} else {
			while ((waitpid(e->pid, &e->exit_code, WNOHANG) != -1) && (exec_cmd_get_state(e) == RUNNING))
				g_usleep(500000);
		}

		/* If the process was cancelled then we kill off the child */
		if (exec_cmd_get_state(e) == CANCELLED) {
#ifdef _WIN32
			g_debug("exec_spawn_process - killing process with pid [%p]\n", e->pid);
#else
			g_debug("exec_spawn_process - killing process with pid [%d]\n", e->pid);
#endif
			gint ret = kill(e->pid, SIGQUIT);
			g_debug("exec_spawn_process - SIGQUIT returned [%d]\n", ret);
			if (ret != 0) {
				ret = kill(e->pid, SIGTERM);
				g_debug("exec_spawn_process - SIGTERM returned [%d]\n", ret);
				if (ret != 0) {
					ret = kill(e->pid, SIGKILL);
					g_debug("exec_spawn_process - SIGKILL returned [%d]\n", ret);
				}
			}
		}

		/* Reap the child so we don't get a zombie */
		waitpid(e->pid, &e->exit_code, 0);
		g_spawn_close_pid(e->pid);
		close(std_out);
		close(std_err);

		exec_cmd_set_state(e, (e->exit_code == 0) ? COMPLETED : FAILED);
#ifdef _WIN32
		g_debug("exec_spawn_process - child [%p] exitcode [%d]\n", e->pid, e->exit_code);
#else
		g_debug("exec_spawn_process - child [%d] exitcode [%d]\n", e->pid, e->exit_code);
#endif
	} else {
		if (err != NULL)
			g_warning("exec_spawn_process - failed to spawn process [%d] [%s]\n", err->code, err->message);
		exec_cmd_set_state(e, FAILED);
		if (err != NULL)
			g_error_free(err);
	}
}

static void exec_working_dir_setup_func (gpointer data)
{
	ExecCmd *ex = (ExecCmd*) data;
	if (ex->working_dir != NULL) {
		g_return_if_fail(chdir(ex->working_dir) == 0);
	}
}

static void exec_stdout_setup_func (gpointer data)
{
	exec_working_dir_setup_func(data);
	dup2(child_child_pipe[1], 1);
	close(child_child_pipe[0]);
}

static void exec_stdin_setup_func (gpointer data)
{
	exec_working_dir_setup_func(data);
	dup2(child_child_pipe[0], 0);
	close(child_child_pipe[1]);
}

static gpointer exec_run_remainder (gpointer data)
{
	GList *cmd = (GList*) data;
	for (; cmd != NULL; cmd = cmd->next) {
		ExecCmd *e = (ExecCmd*) cmd->data;
		if (e->lib_proc != NULL)
			e->lib_proc(e, child_child_pipe);
		else
			exec_spawn_process(e, exec_stdout_setup_func);

		ExecState state = exec_cmd_get_state(e);
		if ((state == CANCELLED) || (state == FAILED))
			break;
	}
	close(child_child_pipe[1]);
	g_debug("exec_run_remainder - thread exiting\n");
	return NULL;
}

static void exec_cmd_delete (ExecCmd *e)
{
	g_mutex_free(e->state_mutex);
	g_ptr_array_free(e->args, TRUE);
	g_free(e->working_dir);
}

ExecCmd* exec_cmd_new (Exec **exec)
{
	ExecCmd *e = g_new0(ExecCmd, 1);
	e->args = g_ptr_array_new();
	e->state = RUNNING;
	e->state_mutex = g_mutex_new();
	e->exec = *exec;
	e->parse_progress = FALSE;
	(*exec)->cmds = g_list_append((*exec)->cmds, e);
	(*exec)->cmds = g_list_first((*exec)->cmds);
	return e;
}

void exec_delete (Exec *exec)
{
	g_free(exec->process_title);
	g_free(exec->process_description);
	GList *cmd = exec->cmds;
	for (; cmd != NULL; cmd = cmd->next)
		exec_cmd_delete((ExecCmd*) cmd->data);
	g_list_free(exec->cmds);
	if (exec->err != NULL)
		g_error_free(exec->err);
	g_free(exec);
}

Exec* exec_new (const gchar *process_title, const gchar *process_description)
{
	g_return_val_if_fail(process_description != NULL, NULL);

	Exec *exec = g_new0(Exec, 1);
	g_return_val_if_fail(exec != NULL, NULL);
	exec->process_title = g_strdup(process_title);
	exec->process_description = g_strdup(process_description);
	exec->outcome = FAILED;
	exec->fraction = 0.0;
	return exec;
}

void exec_cmd_add_arg (ExecCmd *e, const gchar *format, ...)
{
	g_return_if_fail(format != NULL);

	va_list va;
	va_start(va, format);
	gchar *arg = NULL;
	g_vasprintf(&arg, format, va);
	va_end(va);
	g_ptr_array_add(e->args, arg);
}

void exec_cmd_update_arg (ExecCmd *e, const gchar *arg_start, const gchar *format, ...)
{
	g_return_if_fail(arg_start != NULL);
	g_return_if_fail(format != NULL);

	guint i = 0;
	for (; i < e->args->len; ++i) {
		gchar *arg = (gchar *) g_ptr_array_index(e->args, i);
		if (strstr(arg, arg_start) != NULL) {
			g_free(arg);
			va_list va;
			gchar *new_arg = NULL;
			va_start(va, format);
			g_vasprintf(&new_arg, format, va);
			va_end(va);
			e->args->pdata[i] = new_arg;
			g_debug("exec_cmd_update_arg - set to [%s]\n", (gchar*) g_ptr_array_index(e->args, i));
			break;
		}
	}
}

ExecState exec_cmd_get_state (ExecCmd *e)
{
	g_return_val_if_fail(e != NULL, FAILED);
	g_mutex_lock(e->state_mutex);
	ExecState ret = e->state;
	g_mutex_unlock(e->state_mutex);
	return ret;
}

ExecState exec_cmd_set_state (ExecCmd *e, ExecState state)
{
	g_mutex_lock(e->state_mutex);
	if (e->state != CANCELLED)
		e->state = state;
	ExecState ret = e->state;
	g_mutex_unlock(e->state_mutex);
	return ret;
}

static GList *exec_cmd_list = NULL;

GList* exec_get_cmd_list (void)
{
	return exec_cmd_list;
}

/**
 * @brief Runs the given command in a seperate thread
 * @sa exec_run_cmd for sync processes
 */
void exec_run (Exec *ex)
{
	ExecState state = RUNNING;
	GList *piped = NULL;
	GList *cmd = ex->cmds;

	exec_cmd_list = g_list_append(exec_cmd_list, ex);

	for (; cmd != NULL && ((state != CANCELLED) && (state != FAILED)); cmd = cmd->next) {
		ExecCmd *e = (ExecCmd*) cmd->data;
		if (e->piped) {
			piped = g_list_append(piped, e);
			piped = g_list_first(piped);
			continue;
		}

		if (e->pre_proc)
			e->pre_proc(e, NULL);

		state = exec_cmd_get_state(e);
		if (state == SKIPPED)
			continue;
		else if (state == CANCELLED)
			break;

		GThread *thread = NULL;
		if (e->lib_proc != NULL)
			e->lib_proc(e, NULL);
		else if (piped != NULL) {
			pipe(child_child_pipe);
			thread = g_thread_create(exec_run_remainder, (gpointer) piped, TRUE, NULL);
			exec_spawn_process(e, exec_stdin_setup_func);
			close(child_child_pipe[0]);
			close(child_child_pipe[1]);
		} else
			exec_spawn_process(e, exec_working_dir_setup_func);

		state = exec_cmd_get_state(e);
		/* If we have spawned off a bunch of children and something went wrong in the
		 * target process, we update the spawned children to failed so they stop. If
		 * cancel was clicked we will already have updated all of the children. */
		if ((/*(state == CANCELLED) || */(state == FAILED)) && (piped != NULL)) {
			GList *cmd = ex->cmds;
			for (; cmd != NULL; cmd = cmd->next)
				exec_cmd_set_state((ExecCmd*) cmd->data, FAILED);
		}

		if (e->post_proc)
			e->post_proc(e, NULL);

		if (thread != NULL)
			g_thread_join(thread);

		g_list_free(piped);
		piped = NULL;
	}

	exec_cmd_list = g_list_remove(exec_cmd_list, ex);
	g_list_free(piped);
	exec_set_outcome(ex);
	sidebar::JobInfo::Instance().update();
}

void exec_stop (Exec *e)
{
	GList *cmd = e->cmds;
	for (; cmd != NULL; cmd = cmd->next)
		exec_cmd_set_state((ExecCmd*) cmd->data, CANCELLED);

	g_debug("exec_stop - complete\n");
}

/**
 * @brief Runs the given command and blocks the GUI until command is finished
 * @sa exec_run for async processes
 */
gint exec_run_cmd (const std::string& cmd, gchar **output, const std::string& working_dir)
{
	gchar *std_out = NULL;
	gchar *std_err = NULL;
	gint exit_code = -1;
	GError *error = NULL;

	if (!working_dir.empty() && chdir(working_dir.c_str()) < 0)
		return -1;

	if (g_spawn_command_line_sync(cmd.c_str(), &std_out, &std_err, &exit_code, &error)) {
		*output = g_strconcat(std_out, std_err, NULL);
		g_free(std_out);
		g_free(std_err);
	} else if (error != NULL) {
		g_warning("exec_run_cmd - error [%s] spawning command [%s]", error->message, cmd.c_str());
		g_error_free(error);
	} else {
		g_warning("exec_run_cmd - Unknown error spawning command [%s]", cmd.c_str());
	}
	g_warning("exec_run_cmd - [%s] returned [%d]\n", cmd.c_str(), exit_code);
	return exit_code;
}

gint exec_count_operations (const Exec *e)
{
	gint count = 0;
	GList *cmd = e->cmds;
	for (; cmd != NULL; cmd = cmd->next) {
		if (!((ExecCmd*) cmd->data)->piped)
			++count;
	}
	g_debug("exec_count_operations - there are [%d] operations\n", count);
	return count;
}
