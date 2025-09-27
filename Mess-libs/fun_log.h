// Stolen and heavily modified from:
// https://github.com/BogdanTheGeek/CCCCPPPS/blob/main/firmware/ch32-supply/log.c

// MIT License

// Copyright (c) 2024 Bogdan Ionescu

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//------------------------------------------------------------------------------
// Module includes
//------------------------------------------------------------------------------
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

//------------------------------------------------------------------------------
// Module exported defines
//------------------------------------------------------------------------------
#ifndef CONFIG_DEBUG_ENABLE_LOGS
#define CONFIG_DEBUG_ENABLE_LOGS (1)
#endif

//# ANSI color codes
#define LOG_COLOR_NORMAL		39
#define LOG_COLOR_BLACK			30
#define LOG_COLOR_WHITE			37
#define LOG_COLOR_RED			31
#define LOG_COLOR_GREEN			32
#define LOG_COLOR_YELLOW		33
#define LOG_COLOR_BLUE			34
#define LOG_COLOR_MAGENTA		35
#define LOG_COLOR_CYAN			36

//# Text Style
#define LOG_STYLE_NORMAL		0
#define LOG_STYLE_BOLD			1
#define LOG_STYLE_DIM			2
#define LOG_STYLE_ITALIC		3
#define LOG_STYLE_UNDERLINE		4
#define LOG_STYLE_BLINK			5
#define LOG_STYLE_REVERSE		7
#define LOG_STYLE_HIDDEN		8
#define LOG_STYLE_STRIKE		9

#if (CONFIG_DEBUG_ENABLE_LOGS)
	// Log with style
	#define SLOG_Debug(style, tag, f_, ...)		LOG(style, LOG_COLOR_NORMAL, tag, (f_), ##__VA_ARGS__)
	#define SLOG_Info(style, tag, f_, ...)		LOG(style, LOG_COLOR_CYAN, tag, (f_), ##__VA_ARGS__)
	#define SLOG_Warn(style, tag, f_, ...)		LOG(style, LOG_COLOR_YELLOW, tag, (f_), ##__VA_ARGS__)
	#define SLOG_Err(style, tag, f_, ...)		LOG(style, LOG_COLOR_RED, tag, (f_), ##__VA_ARGS__)
	#define SLOG_Ok(style, tag, f_, ...)		LOG(style, LOG_COLOR_GREEN, tag, (f_), ##__VA_ARGS__)
	#define SLOG_Notice(style, tag, f_, ...)	LOG(style, LOG_COLOR_MAGENTA, tag, (f_), ##__VA_ARGS__)

	#define LOG_Debug(f_, ...)		LOG(LOG_STYLE_NORMAL, LOG_COLOR_NORMAL, (f_), ##__VA_ARGS__)
	#define LOG_Info(f_, ...)		LOG(LOG_STYLE_NORMAL, LOG_COLOR_CYAN, (f_), ##__VA_ARGS__)
	#define LOG_Warn(f_, ...)		LOG(LOG_STYLE_ITALIC, LOG_COLOR_YELLOW, (f_), ##__VA_ARGS__)
	#define LOG_Err(f_, ...)		LOG(LOG_STYLE_BOLD, LOG_COLOR_RED ,(f_), ##__VA_ARGS__)
	#define LOG_Ok(f_, ...)			LOG(LOG_STYLE_BOLD, LOG_COLOR_GREEN, (f_), ##__VA_ARGS__)
	#define LOG_Notice(f_, ...)		LOG(LOG_STYLE_NORMAL, LOG_COLOR_MAGENTA, (f_), ##__VA_ARGS__)
#else
	#define SLOG_Debug(style, f_, ...)		((void)f_)
	#define SLOG_Info(style, f_, ...)		((void)f_)
	#define SLOG_Warn(style, f_, ...)		((void)f_)
	#define SLOG_Err(style, f_, ...)		((void)f_)
	#define SLOG_Ok(style, f_, ...)			((void)f_)
	#define SLOG_Notice(style, f_, ...)		((void)f_)

	#define LOG_Debug(f_, ...)		((void)f_)
	#define LOG_Info(f_, ...)		((void)f_)
	#define LOG_Warn(f_, ...)		((void)f_)
	#define LOG_Err(f_, ...)		((void)f_)
	#define LOG_Ok(f_, ...)			((void)f_)
	#define LOG_Notice(f_, ...)		((void)f_)
#endif


//# Reset
#define LOG_RESET "\033[0m"

//# Screen Control
#define LOG_CLEAR_SCREEN			"\033[2J"
#define LOG_CLEAR_LINE				"\033[2K"
#define LOG_CLEAR_LINE_END			"\033[0K"
#define LOG_CLEAR_LINE_START		"\033[1K"
#define LOG_CLEAR_DOWN				"\033[J"
#define LOG_CLEAR_UP				"\033[1J"
#define LOG_SCROLL_UP(n)			"\033[" #n "S"
#define LOG_SCROLL_DOWN(n)			"\033[" #n "T"

//# Cursor Control
#define LOG_CURSOR_HOME				"\033[H"
#define LOG_CURSOR_SAVE				"\033[s"
#define LOG_CURSOR_RESTORE			"\033[u"
#define LOG_CURSOR_HIDE				"\033[?25l"
#define LOG_CURSOR_SHOW				"\033[?25h"

//# Function macros for parameterized codes
#define LOG_CURSOR_UP(n)			"\033[" #n "A"
#define LOG_CURSOR_DOWN(n)			"\033[" #n "B"
#define LOG_CURSOR_RIGHT(n)			"\033[" #n "C"
#define LOG_CURSOR_LEFT(n)			"\033[" #n "D"
#define LOG_CURSOR_NEXT_LINE(n)		"\033[" #n "E"
#define LOG_CURSOR_PREV_LINE(n)		"\033[" #n "F"
#define LOG_CURSOR_COLUMN(n)		"\033[" #n "G"
#define LOG_CURSOR_POS(row,col)		"\033[" #row ";" #col "H"

//# RGB True Color (24-bit)
#define LOG_FG_RGB(r,g,b) "\033[38;2;" #r ";" #g ";" #b "m"
#define LOG_BG_RGB(r,g,b) "\033[48;2;" #r ";" #g ";" #b "m"

//# 256-color mode
#define LOG_FG256(n) "\033[38;5;" #n "m"
#define LOG_BG256(n) "\033[48;5;" #n "m"

static const uint32_t *s_systick = NULL;

void _LOG_Print_inStyle(u8 style, u8 color) {
	printf("\033[%d;%dm", style, color);
}

void LOG_Init(const uint32_t *const systick) {
	s_systick = systick;
}

void LOG_banner(const char *name) {
	printf(LOG_BG_RGB(70, 130, 180));
	printf(name);
	printf(LOG_RESET "\n");
}

void LOG_listUsages() {
	LOG_Debug("Tag", "LOG_Debug");
	LOG_Info("Tag", "LOG_Info");
	LOG_Warn("Tag", "LOG_Warn");
	LOG_Err("Tag", "LOG_Err");
	LOG_Ok("Tag", "LOG_Ok");
	LOG_Notice("Tag", "LOG_Notice");

	// printf("\n");
	// SLOG_Debug(LOG_STYLE_DIM, "Tag", "LOG_Debug");
	// SLOG_Info(LOG_STYLE_ITALIC,"Tag", "LOG_Info");
	// SLOG_Warn(LOG_STYLE_UNDERLINE, "Tag", "LOG_Warn");
	// SLOG_Err(LOG_STYLE_REVERSE, "Tag", "LOG_Err");
	// SLOG_Ok(LOG_STYLE_STRIKE, "Tag", "LOG_Ok");
}

#if (CONFIG_DEBUG_ENABLE_LOGS)
void LOG(u8 style, u8 color, const char *tag, char *format, ...) {
    _LOG_Print_inStyle(style, color);
    printf("[%s] %lu: ", tag, *s_systick);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\033[0;39m");  // Reset color
    printf("\n");  // Just newline, not \r\n
}
#endif