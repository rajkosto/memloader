/*
* Copyright (c) 2018 naehrwert
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "timer.h"
#include "t210.h"

u32 get_tmr_us()
{
	return TMR(TMR_US_OFFS);
}

u32 get_tmr_s()
{
	return RTC(APBDEV_RTC_SECONDS);
}

void sleep(u32 ticks)
{
	u32 start = get_tmr_us();
	while (get_tmr_us() - start <= ticks) {}
}