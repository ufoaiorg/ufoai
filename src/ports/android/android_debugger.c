/* This function will dump stack trace to logcat and continue running on Android OS */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <android/log.h>
#include "android_debugger.h"

/* TODO: it does not work */

static sighandler_t debuggerHandler;

static void mySignalHandler (int sig)
{
	debuggerHandler(sig);
	signal( sig, debuggerHandler );
}

void androidDumpBacktrace (FILE * out)
{
	debuggerHandler = signal(SIGSEGV, mySignalHandler);
	__android_log_print(ANDROID_LOG_INFO, "UFOAI", "Backtrace (run command \"arm-eabi-gdb libapplication.so -ex 'list *0xaabbccdd'\" where aabbccdd is a stack address below):");
	if (debuggerHandler == SIG_IGN || debuggerHandler == SIG_DFL || debuggerHandler == SIG_ERR) {
		__android_log_print(ANDROID_LOG_INFO, "UFOAI", "Getting backtrace failed, you have very custom or broken Android firmware");
		signal(SIGSEGV, debuggerHandler);
		return;
	}
	raise(SIGSEGV);
}
