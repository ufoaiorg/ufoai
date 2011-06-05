/**
 * @file test_shared.h
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

#ifndef TEST_SHARED_H
#define TEST_SHARED_H

#include <CUnit/Basic.h>
#include "../common/common.h"
#include "../shared/shared.h"

#define UFO_CU_ASSERT_EQUAL_INT_MSG_FATAL(actual, expected, msg) \
  { CU_assertImplementation(((actual) == (expected)), __LINE__, msg ? msg : va("UFO_CU_ASSERT(%i but expected %i)", actual, expected), __FILE__, "", CU_TRUE); }

#define UFO_CU_ASSERT_EQUAL_INT_MSG(actual, expected, msg) \
  { CU_assertImplementation(((actual) == (expected)), __LINE__, msg ? msg : va("UFO_CU_ASSERT(%i but expected %i)", actual, expected), __FILE__, "", CU_FALSE); }

#define UFO_CU_ASSERT_NOT_EQUAL_INT_MSG(actual, expected, msg) \
  { CU_assertImplementation(((actual) != (expected)), __LINE__, msg ? msg : va("UFO_CU_ASSERT(%i equals %i)", actual, expected), __FILE__, "", CU_FALSE); }

#define UFO_CU_ASSERT_TRUE_MSG(value, msg) \
  { CU_assertImplementation((value), __LINE__, msg ? msg : ("UFO_CU_ASSERT(" #value ")"), __FILE__, "", CU_FALSE); }

#define UFO_CU_ASSERT_MSG(msg) \
  { CU_assertImplementation(CU_FALSE, __LINE__, msg, __FILE__, "", CU_FALSE); }

#define UFO_CU_FAIL_MSG(msg) \
  { CU_assertImplementation(CU_FALSE, __LINE__, va("UFO_CU_FAIL(\"%s\")", msg), __FILE__, "", CU_FALSE); }

#define UFO_CU_FAIL_MSG_FATAL(msg) \
  { CU_assertImplementation(CU_FALSE, __LINE__, va("UFO_CU_FAIL(\"%s\")", msg), __FILE__, "", CU_TRUE); }

void TEST_vPrintf(const char *fmt, va_list argptr);
void TEST_Init(void);
void TEST_Shutdown(void);

/** interface to allow to custom tests with command line */
void TEST_RegisterProperty(const char* name, const char* value);
qboolean TEST_ExistsProperty (const char* name);
int TEST_GetIntProperty(const char* name);
long TEST_GetLongProperty(const char* name);
const char* TEST_GetStringProperty(const char* name);

#endif
