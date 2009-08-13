/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "stacktrace.h"
#include "stream/textstream.h"

#include "environment.h"

#ifdef __linux__
#include <execinfo.h>

void write_stack_trace (TextOutputStream& outputStream)
{
	const unsigned int MAX_SYMBOLS = 256;
	void* symbols[MAX_SYMBOLS];

	// get return addresses
	int symbol_count = backtrace(symbols, MAX_SYMBOLS);
	if (!symbol_count)
		return;

	// resolve and print names
	char** symbol_names = backtrace_symbols(symbols, symbol_count);
	if (symbol_names) {
		for (int i = 0; (i < symbol_count); ++i)
			outputStream << symbol_names[i] << "\n";

		// not a memleak, see www.gnu.org/software/libc/manual (Debugging Support, Backtraces)
		free(symbol_names);
	}
}
#else
void write_stack_trace (TextOutputStream& outputStream)
{
	outputStream << "\nStacktrace is not implemented on this plattform\n";
}
#endif
