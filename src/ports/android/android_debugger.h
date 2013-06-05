#pragma once

/* This function will dump stack trace to logcat and continue running on Android OS */
#include <stdio.h>

extern void androidDumpBacktrace(FILE * out);
