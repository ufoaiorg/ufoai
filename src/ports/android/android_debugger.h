#ifndef __ANDROID_DEBUGGER_H__
#define __ANDROID_DEBUGGER_H__
/* This function will dump stack trace to logcat and continue running on Android OS */
#include <stdio.h>

extern void androidDumpBacktrace(FILE * out);

#endif
