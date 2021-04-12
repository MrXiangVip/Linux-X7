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

typedef void (*sig_timer_cb)(void);

int32_t sig_timer_init(void);
int32_t sig_timer_start(int32_t ms, sig_timer_cb callback);
