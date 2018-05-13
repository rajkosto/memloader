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

#include "btn.h"
#include "gpio.h"
#include "pinmux.h"
#include "max7762x.h"
#include "max77620.h"

u32 btn_read()
{
	u32 res = 0;
	if (!gpio_read(GPIO_BY_NAME(BUTTON_VOL_DOWN)))
		res |= BTN_VOL_DOWN;
	if (!gpio_read(GPIO_BY_NAME(BUTTON_VOL_UP)))
		res |= BTN_VOL_UP;
	if (max77620_recv_byte(MAX77620_REG_ONOFFSTAT) & 0x4)
		res |= BTN_POWER;
	return res;
}

u32 btn_wait()
{
	u32 res = 0, btn = btn_read();
	do
	{
		res = btn_read();
	} while (btn == res);
	return res;
}
