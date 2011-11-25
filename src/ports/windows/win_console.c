/**
 * @file win_console.c
 * @brief console functions for windows ports
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../../common/common.h"
#include "win_local.h"

#define CONSOLE_WINDOW_STYLE		(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN|WS_GROUP)
#define CONSOLE_WINDOW_CLASS_NAME	GAME_TITLE" Console"

#ifdef DEDICATED_ONLY
#define CONSOLE_WINDOW_NAME			GAME_TITLE" Dedicated Server Console"
#else
#define CONSOLE_WINDOW_NAME			CONSOLE_WINDOW_CLASS_NAME
#endif

#define MAX_OUTPUT					32768
#define	MAX_PRINTMSG				8192

typedef struct {
	int			outLen;					/**< To keep track of output buffer len */
	char		cmdBuffer[MAXCMDLINE];	/**< Buffered input from dedicated console */
	qboolean	timerActive;			/**< Timer is active (for fatal errors) */
	qboolean	flashColor;				/**< If true, flash error message to red */

	/* Window stuff */
	HWND		hWnd;
	HWND		hWndCopy;
	HWND		hWndClear;
	HWND		hWndQuit;
	HWND		hWndOutput;
	HWND		hWndInput;
	HWND		hWndMsg;
	HFONT		hFont;
	HFONT		hFontBold;
	HBRUSH		hBrushMsg;
	HBRUSH		hBrushOutput;
	HBRUSH		hBrushInput;
	WNDPROC		defOutputProc;
	WNDPROC		defInputProc;
} sysConsole_t;

static sysConsole_t sys_console;

int SV_CountPlayers(void);

/**
 * @brief Dispatch window messages
 */
static void Sys_ConsoleLoop (qboolean error)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
		if (!GetMessage(&msg, NULL, 0, 0)) {
			if (error)
				ExitProcess(1);
			else
				Sys_Quit();
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

/**
 * @brief Handles input for the console window
 * @returns @c NULL if there is no input in the input box
 */
const char *Sys_ConsoleInput (void)
{
	static char buffer[MAXCMDLINE];

#ifdef DEDICATED_ONLY
	/* the client console is not visible after init stage and thus this is only
	 * needed for the server */
	Sys_ConsoleLoop(qfalse);
#endif

	/* empty command buffer? */
	if (sys_console.cmdBuffer[0] == '\0')
		return NULL;

	Q_strncpyz(buffer, sys_console.cmdBuffer, sizeof(buffer));

	/* now we can clear the cmdBuffer */
	sys_console.cmdBuffer[0] = '\0';

	return buffer;
}


void Sys_ConsoleOutput (const char *text)
{
	char buffer[MAX_PRINTMSG];
	int len = 0;

	/* skip color char */
	if (*text == 1)
		text++;

	/* Change \n to \r\n so it displays properly in the edit box */
	while (*text) {
		if (*text == '\n' && len < MAX_PRINTMSG - 2) {
			buffer[len++] = '\r';
			buffer[len++] = '\n';
		} else if (len < MAX_PRINTMSG - 1) {
			buffer[len++] = *text;
		} else {
			/* truncate */
			buffer[len] = '\0';
			break;
		}

		text++;
	}
	buffer[len] = 0;

	sys_console.outLen += len;
	if (sys_console.outLen >= MAX_OUTPUT) {
		SendMessage(sys_console.hWndOutput, EM_SETSEL, 0, -1);
		sys_console.outLen = len;
	}

	/* Scroll down before adding more text */
	SendMessage(sys_console.hWndOutput, EM_LINESCROLL, 0, 0xFFFF);
	SendMessage(sys_console.hWndOutput, EM_SCROLLCARET, 0, 0);

	SendMessage(sys_console.hWndOutput, EM_REPLACESEL, FALSE, (LPARAM)buffer);

	/* Scroll down */
	SendMessage(sys_console.hWndOutput, EM_LINESCROLL, 0, 0xFFFF);
	SendMessage(sys_console.hWndOutput, EM_SCROLLCARET, 0, 0);
}

/**
 * @brief On platforms supporting it, print a backtrace.
 * @note For windows this is not handled here - see Sys_BacktraceInit
 */
void Sys_Backtrace (void)
{
}

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char text[1024];

	Sys_Backtrace();

#ifdef COMPILE_MAP
	Mem_Shutdown();
#endif

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	/* Echo to console */
	Sys_ConsoleOutput("\n");
	Sys_ConsoleOutput(text);
	Sys_ConsoleOutput("\n");

	/* Display the message and set a timer so we can flash the text */
	SetWindowText(sys_console.hWndMsg, text);
	SetTimer(sys_console.hWnd, 1, 1000, NULL);

	sys_console.timerActive = qtrue;

	/* Show/hide everything we need */
	ShowWindow(sys_console.hWndMsg, SW_SHOW);
	ShowWindow(sys_console.hWndInput, SW_HIDE);

	Sys_ShowConsole(qtrue);

	/* Wait for the user to quit */
	while (1) {
		Sys_ConsoleLoop(qtrue);
		/* Don't hog the CPU */
		Sys_Sleep(25);
	}
}


void Sys_ShowConsole (qboolean show)
{
	if (!show) {
		ShowWindow(sys_console.hWnd, SW_HIDE);
		return;
	}

	ShowWindow(sys_console.hWnd, SW_SHOW);
	UpdateWindow(sys_console.hWnd);
	SetForegroundWindow(sys_console.hWnd);
	SetFocus(sys_console.hWnd);

	/* Set the focus to the input edit box if possible */
	SetFocus(sys_console.hWndInput);

	/* Scroll down */
	SendMessage(sys_console.hWndOutput, EM_LINESCROLL, 0, 0xFFFF);
	SendMessage(sys_console.hWndOutput, EM_SCROLLCARET, 0, 0);
}

static LONG WINAPI Sys_ConsoleProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_INACTIVE) {
			SetFocus(sys_console.hWndInput);
			return 0;
		}
		break;

	case WM_CLOSE:
		if (SV_CountPlayers()) {
			const int ays = MessageBox(hWnd, "There are still players on the server! Really shut it down?", "WARNING!", MB_YESNO + MB_ICONEXCLAMATION);
			if (ays == IDNO)
				return TRUE;
		}
		Sys_Quit();
		break;

	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED) {
			if ((HWND)lParam == sys_console.hWndCopy) {
				SendMessage(sys_console.hWndOutput, EM_SETSEL, 0, -1);
				SendMessage(sys_console.hWndOutput, WM_COPY, 0, 0);
			} else if ((HWND)lParam == sys_console.hWndClear) {
				SendMessage(sys_console.hWndOutput, EM_SETSEL, 0, -1);
				SendMessage(sys_console.hWndOutput, WM_CLEAR, 0, 0);
			} else if ((HWND)lParam == sys_console.hWndQuit)
				Sys_Quit();
		} else if (HIWORD(wParam) == EN_VSCROLL)
			InvalidateRect(sys_console.hWndOutput, NULL, TRUE);
		break;

	case WM_CTLCOLOREDIT:
		if ((HWND)lParam == sys_console.hWndOutput) {
			SetBkMode((HDC)wParam, TRANSPARENT);
			SetBkColor((HDC)wParam, RGB(255, 255, 255));
			SetTextColor((HDC)wParam, RGB(0, 0, 0));
			return (LONG)sys_console.hBrushOutput;
		} else if ((HWND)lParam == sys_console.hWndInput) {
			SetBkMode((HDC)wParam, TRANSPARENT);
			SetBkColor((HDC)wParam, RGB(255, 255, 255));
			SetTextColor((HDC)wParam, RGB(0, 0, 0));
			return (LONG)sys_console.hBrushInput;
		}
		break;

	case WM_CTLCOLORSTATIC:
		if ((HWND)lParam == sys_console.hWndMsg) {
			SetBkMode((HDC)wParam, TRANSPARENT);
			SetBkColor((HDC)wParam, RGB(127, 127, 127));

			if (sys_console.flashColor)
				SetTextColor((HDC)wParam, RGB(255, 0, 0));
			else
				SetTextColor((HDC)wParam, RGB(0, 0, 0));

			return (LONG)sys_console.hBrushMsg;
		}
		break;

	case WM_TIMER:
		sys_console.flashColor = !sys_console.flashColor;
		InvalidateRect(sys_console.hWndMsg, NULL, TRUE);
		break;

	case WM_CREATE:
		SetTimer(sys_console.hWnd, 1, 500, NULL);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


static LONG WINAPI Sys_ConsoleEditProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CHAR:
		if (hWnd == sys_console.hWndInput) {
			switch (wParam) {
			case VK_RETURN:
				if (GetWindowText(sys_console.hWndInput, sys_console.cmdBuffer, sizeof(sys_console.cmdBuffer))) {
					SetWindowText(sys_console.hWndInput, "");
					Com_Printf("]%s\n", sys_console.cmdBuffer);
				}
				/* Keep it from beeping */
				return 0;
			case VK_TAB:
				/* command completion */
				if (GetWindowText(sys_console.hWndInput, sys_console.cmdBuffer, sizeof(sys_console.cmdBuffer))) {
					uint32_t inputpos = 0;
					if (Com_ConsoleCompleteCommand(sys_console.cmdBuffer, sys_console.cmdBuffer, sizeof(sys_console.cmdBuffer), &inputpos, 0)) {
						SetWindowText(sys_console.hWndInput, sys_console.cmdBuffer);
						/* reset again - we don't want to execute yet */
						sys_console.cmdBuffer[0] = '\0';
					}
				}
				/* Keep it from beeping */
				return 0;
			}
		} else if (hWnd == sys_console.hWndOutput)
			return 0;	/* Read only */
		break;

	case WM_VSCROLL:
		if (LOWORD(wParam) == SB_THUMBTRACK)
			return 0;
		break;
	}

	if (hWnd == sys_console.hWndOutput)
		return CallWindowProc(sys_console.defOutputProc, hWnd, uMsg, wParam, lParam);
	else if (hWnd == sys_console.hWndInput)
		return CallWindowProc(sys_console.defInputProc, hWnd, uMsg, wParam, lParam);
	return 0;
}


/**
 * @brief Shutdown the console
 */
void Sys_ConsoleShutdown (void)
{
	if (sys_console.timerActive)
		KillTimer(sys_console.hWnd, 1);

	if (sys_console.hBrushMsg)
		DeleteObject(sys_console.hBrushMsg);
	if (sys_console.hBrushOutput)
		DeleteObject(sys_console.hBrushOutput);
	if (sys_console.hBrushInput)
		DeleteObject(sys_console.hBrushInput);

	if (sys_console.hFont)
		DeleteObject(sys_console.hFont);
	if (sys_console.hFontBold)
		DeleteObject(sys_console.hFontBold);

	if (sys_console.defOutputProc)
		SetWindowLong(sys_console.hWndOutput, GWL_WNDPROC, (LONG)sys_console.defOutputProc);
	if (sys_console.defInputProc)
		SetWindowLong(sys_console.hWndInput, GWL_WNDPROC, (LONG)sys_console.defInputProc);

	ShowWindow(sys_console.hWnd, SW_HIDE);
	DestroyWindow(sys_console.hWnd);
	UnregisterClass(CONSOLE_WINDOW_CLASS_NAME, global_hInstance);

	Sys_BacktraceShutdown();
}

void Sys_ConsoleInit (void)
{
	WNDCLASSEX wc;
	HDC hDC;
	RECT r;
	int x, y, w, h;

	Sys_BacktraceInit();

	/* Center the window in the desktop */
	hDC = GetDC(0);
	w = GetDeviceCaps(hDC, HORZRES);
	h = GetDeviceCaps(hDC, VERTRES);
	ReleaseDC(0, hDC);

	r.left = (w - 540) / 2;
	r.top = (h - 455) / 2;
	r.right = r.left + 540;
	r.bottom = r.top + 455;

	AdjustWindowRect(&r, CONSOLE_WINDOW_STYLE, FALSE);

	x = r.left;
	y = r.top;
	w = r.right - r.left;
	h = r.bottom - r.top;

	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)Sys_ConsoleProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = global_hInstance;
	wc.hIcon = LoadIcon(global_hInstance, MAKEINTRESOURCE(101));
	wc.hIconSm = 0;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszMenuName = 0;
	wc.lpszClassName = CONSOLE_WINDOW_CLASS_NAME;
	wc.cbSize = sizeof(WNDCLASSEX);

	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, "Could not register console window class", "ERROR", MB_OK | MB_ICONERROR | MB_TASKMODAL);
		exit(0);
	}

	sys_console.hWnd = CreateWindowEx(0, CONSOLE_WINDOW_CLASS_NAME, CONSOLE_WINDOW_NAME, CONSOLE_WINDOW_STYLE, x, y, w, h, NULL, NULL, global_hInstance, NULL);
	if (!sys_console.hWnd) {
		UnregisterClass(CONSOLE_WINDOW_CLASS_NAME, global_hInstance);
		MessageBox(NULL, "Could not create console window", "ERROR", MB_OK | MB_ICONERROR | MB_TASKMODAL);
		exit(0);
	}

	sys_console.hWndMsg = CreateWindowEx(0, "STATIC", "", WS_CHILD | SS_SUNKEN, 5, 5, 530, 30, sys_console.hWnd, NULL, global_hInstance, NULL);
	sys_console.hWndOutput = CreateWindowEx(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE, 5, 40, 530, 350, sys_console.hWnd, NULL, global_hInstance, NULL);
	sys_console.hWndInput = CreateWindowEx(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 5, 395, 530, 20, sys_console.hWnd, NULL, global_hInstance, NULL);
	sys_console.hWndCopy = CreateWindowEx(0, "BUTTON", "copy", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 5, 425, 70, 25, sys_console.hWnd, NULL, global_hInstance, NULL);
	sys_console.hWndClear = CreateWindowEx(0, "BUTTON", "clear", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 80, 425, 70, 25, sys_console.hWnd, NULL, global_hInstance, NULL);
	sys_console.hWndQuit = CreateWindowEx(0, "BUTTON", "quit", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 465, 425, 70, 25, sys_console.hWnd, NULL, global_hInstance, NULL);

	/* Create and set fonts */
	sys_console.hFont = CreateFont(14, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
	sys_console.hFontBold = CreateFont(20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "System");

	SendMessage(sys_console.hWndMsg, WM_SETFONT, (WPARAM)sys_console.hFont, FALSE);
	SendMessage(sys_console.hWndOutput, WM_SETFONT, (WPARAM)sys_console.hFont, FALSE);
	SendMessage(sys_console.hWndInput, WM_SETFONT, (WPARAM)sys_console.hFont, FALSE);
	SendMessage(sys_console.hWndCopy, WM_SETFONT, (WPARAM)sys_console.hFontBold, FALSE);
	SendMessage(sys_console.hWndClear, WM_SETFONT, (WPARAM)sys_console.hFontBold, FALSE);
	SendMessage(sys_console.hWndQuit, WM_SETFONT, (WPARAM)sys_console.hFontBold, FALSE);

	/* Create brushes */
	sys_console.hBrushMsg = CreateSolidBrush(RGB(127, 127, 127));
	sys_console.hBrushOutput = CreateSolidBrush(RGB(255, 255, 255));
	sys_console.hBrushInput = CreateSolidBrush(RGB(255, 255, 255));

	/* Subclass edit boxes */
	sys_console.defOutputProc = (WNDPROC)SetWindowLong(sys_console.hWndOutput, GWL_WNDPROC, (LONG)Sys_ConsoleEditProc);
	sys_console.defInputProc = (WNDPROC)SetWindowLong(sys_console.hWndInput, GWL_WNDPROC, (LONG)Sys_ConsoleEditProc);

	/* Set text limit for input edit box */
	SendMessage(sys_console.hWndInput, EM_SETLIMITTEXT, (WPARAM)(MAXCMDLINE - 1), 0);

	/* Make visible */
	Sys_ShowConsole(qtrue);
}
