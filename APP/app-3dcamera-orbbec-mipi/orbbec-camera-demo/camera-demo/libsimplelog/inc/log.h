/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#define LOG_VERSION "0.1.0"

#define ANSI_RESET  = "\u001B[0m";
#define ANSI_BLACK  = "\u001B[30m";
#define ANSI_RED    = "\u001B[31m";
#define ANSI_GREEN  = "\u001B[32m";
#define ANSI_YELLOW = "\u001B[33m";
#define ANSI_BLUE   = "\u001B[34m";
#define ANSI_PURPLE = "\u001B[35m";
#define ANSI_CYAN   = "\u001B[36m";
#define ANSI_WHITE  = "\u001B[37m";

// Reset
#define RESET = "\033[0m";  // Text Reset

// Regular Colors
#define BLACK  = "\033[0;30m";   // BLACK
#define RED    = "\033[0;31m";   // RED
#define GREEN  = "\033[0;32m";   // GREEN
#define YELLOW = "\033[0;33m";   // YELLOW
#define BLUE   = "\033[0;34m";   // BLUE
#define PURPLE = "\033[0;35m";   // PURPLE
#define CYAN   = "\033[0;36m";   // CYAN
#define WHITE  = "\033[0;37m";   // WHITE

// Bold
#define BLACK_BOLD  = "\033[1;30m";  // BLACK
#define RED_BOLD    = "\033[1;31m";  // RED
#define GREEN_BOLD  = "\033[1;32m";  // GREEN
#define YELLOW_BOLD = "\033[1;33m";  // YELLOW
#define BLUE_BOLD   = "\033[1;34m";  // BLUE
#define PURPLE_BOLD = "\033[1;35m";  // PURPLE
#define CYAN_BOLD   = "\033[1;36m";  // CYAN
#define WHITE_BOLD  = "\033[1;37m";  // WHITE

// Underline
#define BLACK_UNDERLINED  = "\033[4;30m";  // BLACK
#define RED_UNDERLINED    = "\033[4;31m";  // RED
#define GREEN_UNDERLINED  = "\033[4;32m";  // GREEN
#define YELLOW_UNDERLINED = "\033[4;33m";  // YELLOW
#define BLUE_UNDERLINED   = "\033[4;34m";  // BLUE
#define PURPLE_UNDERLINED = "\033[4;35m";  // PURPLE
#define CYAN_UNDERLINED   = "\033[4;36m";  // CYAN
#define WHITE_UNDERLINED  = "\033[4;37m";  // WHITE

//配置参数结构体
typedef struct 
{
	unsigned char log_level;			/*log级别*/
}stLogCfg, *pstLogCfg;

typedef void (*log_LockFn)(void *udata, int32_t lock);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

void log_set_udata(void *udata);
void log_set_lock(log_LockFn fn);
void log_set_fp(FILE *fp);
void log_set_level(int32_t level);
void log_set_quiet(int32_t enable);
void log_set_color(int32_t enable);
void log_set_simple(int32_t enable);

void log_log(int32_t level, const char *file, int32_t line, const char *fmt, ...);
int SaveLogCfgToCfgFile(pstLogCfg pstSysLogCfg);
int GetSysLogCfg(pstLogCfg pstSysLogCfg);

#ifdef __cplusplus
  }
#endif /* end of __cplusplus */

#endif
