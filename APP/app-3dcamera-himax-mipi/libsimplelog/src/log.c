/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>

#include "log.h"

static struct {
	void *udata;
	log_LockFn lock;
	FILE *fp;
	int32_t level;
	int32_t color;
	int32_t quiet;
	int32_t simple;
} L;

static const char *level_names[] = {
	"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char *level_colors[] = {
	"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};

static void lock(void) {
	if (L.lock) {
		L.lock(L.udata, 1);
	}
}

static void unlock(void) {
	if (L.lock) {
		L.lock(L.udata, 0);
	}
}

void log_set_udata(void *udata) {
	L.udata = udata;
}

void log_set_lock(log_LockFn fn) {
	L.lock = fn;
}

void log_set_color(int32_t enable) {
	L.color = enable;
}

void log_set_fp(FILE *fp) {
	L.fp = fp;
}

void log_set_level(int32_t level) {
	L.level = level;
}

void log_set_quiet(int32_t enable) {
	L.quiet = enable ? 1 : 0;
}

void log_set_simple(int32_t enable) {
	L.simple = enable ? 1 : 0;
}

void log_log(int32_t level, const char *file, int32_t line, const char *fmt, ...) {
	if (level < L.level) {
		return;
	}

	/* Acquire lock */
	lock();

	/* Get current time */
	time_t t = time(NULL);
	struct tm *lt = localtime(&t);

	/* Log to stderr */
	if (!L.quiet) {
		va_list args;
		char buf[16];
		buf[strftime(buf, sizeof(buf), "%H:%M:%S", lt)] = '\0';
		if (!L.simple) {
			if (L.color) {
				fprintf(
					stderr, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
					buf, level_colors[level], level_names[level], file, line);
			} else {
				fprintf(stderr, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
			}
		}
		if (L.color) {
			fprintf(stderr, "%s", level_colors[level]);
		}
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		if (L.color) {
			fprintf(stderr, "\x1b[0m");
		}

		fprintf(stderr, "\n");
	}

	/* Log to file */
	if (L.fp) {
		va_list args;
		char buf[32];
		buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
		fprintf(L.fp, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
		va_start(args, fmt);
		vfprintf(L.fp, fmt, args);
		va_end(args);
		fprintf(L.fp, "\n");
	}

	/* Release lock */
	unlock();
}

int SaveLogCfgToCfgFile(pstLogCfg pstSysLogCfg)
{
	FILE   *pFile = NULL;
	char	szBuffer[1024] = {0};
	int iBufferSize=0;				

	memset(szBuffer, 0, sizeof(szBuffer));
	iBufferSize += sprintf(szBuffer + iBufferSize, "log_level=%d\n", pstSysLogCfg->log_level);

	pFile = fopen("./log.cfg", "w");
	if (pFile)
	{
		fwrite(szBuffer, iBufferSize, 1, pFile);	
		
		/* 将文件同步,及时写入磁盘,不然，系统重启动，导致文件内容丢失 */
		fflush(pFile);		/* 刷新内存缓冲，将内容写入内核缓冲 */
		fsync(fileno(pFile));	/* 将内核缓冲写入磁盘 */
	}
	else
	{
		return -1;
	}

	if (NULL != pFile)
	{
		fclose(pFile);
		pFile = NULL;
	}

	return 0;
}

int GetSysLogCfg(pstLogCfg pstSysLogCfg)
{
	FILE   *pFile = NULL;
	char   szMsg[256] = {0};
	unsigned short len = 0;
	
	if (access("./log.cfg", F_OK) == 0)  
	{
		pFile = fopen("./log.cfg", "r");
		if (pFile == NULL)
		{
			printf("Err, can't fopen<./log.cfg>\n");
			return -1;
		}
		memset(szMsg, 0, sizeof(szMsg));

		while (fgets(szMsg, sizeof(szMsg), pFile) != NULL)
		{
			if (strstr(szMsg, "log_level=") != NULL) 
			{
				len = strlen("log_level=");
				sscanf(&szMsg[len], "%d", &(pstSysLogCfg->log_level));//skip "face_threshold="
				if((pstSysLogCfg->log_level > LOG_FATAL) || (pstSysLogCfg->log_level <LOG_TRACE))
				{
					pstSysLogCfg->log_level =  LOG_INFO;
				}
				log_info("log_level<%d>\n", pstSysLogCfg->log_level);
			}

			memset(szMsg, 0, sizeof(szMsg));
		}
	}
	else
	{
		/*如果不存在配置文件，那么用系统默认的值，并生成配置文件*/
		pstSysLogCfg->log_level =	LOG_INFO;
		printf("Warring, not exit <./log.cfg>.\n");

		printf("log<%d>\n",	 \
			pstSysLogCfg->log_level);
		
		SaveLogCfgToCfgFile(pstSysLogCfg);
	}

	if (NULL != pFile)
	{
		fclose(pFile);
		pFile = NULL;
	}
	
	return 0;
}

