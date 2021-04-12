/*
 * Copyright 2019 NXP Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <stdint.h>
#include <sys/time.h>
#include <signal.h>
#include "sig_timer.h"

static struct itimerval gtimer;

int32_t sig_timer_init(void)
{
	return 0;
}

int32_t sig_timer_start(int32_t ms, sig_timer_cb callback)
{
    time_t secs, usecs;
    secs = ms / 1000;
    usecs = ms % 1000 * 1000;

    gtimer.it_interval.tv_sec = secs;
    gtimer.it_interval.tv_usec = usecs;
    gtimer.it_value.tv_sec = secs;
    gtimer.it_value.tv_usec = usecs;

    setitimer(ITIMER_REAL, &gtimer, NULL);

	signal(SIGALRM, (__sighandler_t)callback);

    return 0;
}


