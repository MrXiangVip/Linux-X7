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

#ifndef __PER_CALC_H__
#define __PER_CALC_H__

#include <stdint.h>

void per_calc_start(void);
void per_calc_end(void);
uint32_t per_calc_get_result(void);

#endif
