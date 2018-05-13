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

#include "di.h"
#include "clock.h"
#include "gpio.h"
#include "pinmux.h"
#include "t210.h"
#include "timer.h"
#include "util.h"
#include "pmc.h"
#include "max7762x.h"
#include "max77620.h"

#include "di.inl"

static u32 _display_ver = 0;

static void _display_dsi_wait(u32 timeout, u32 off, u32 mask)
{
	u32 end = get_tmr() + timeout;
	while (get_tmr() < end && (DSI(off) & mask)) {}
	sleep(5);
}

void display_init()
{
	//Power on.
	max77620_regulator_set_voltage(REGULATOR_LDO0, 1200000); //1.2V
	max77620_regulator_enable(REGULATOR_LDO0, 1);
	max77620_send_byte(MAX77620_REG_GPIO7, 0x09);

	//Enable MIPI CAL, DSI, DISP1, HOST1X, UART_FST_MIPI_CAL, DSIA LP clocks.
	CLOCK(CLK_RST_CONTROLLER_RST_DEV_H_CLR) = CLK_H_DSI | CLK_H_MIPI_CAL;
	CLOCK(CLK_RST_CONTROLLER_CLK_ENB_H_SET) = CLK_H_DSI | CLK_H_MIPI_CAL;
	CLOCK(CLK_RST_CONTROLLER_RST_DEV_L_CLR) = CLK_L_DISP1 | CLK_L_HOST1X;
	CLOCK(CLK_RST_CONTROLLER_CLK_ENB_L_SET) = CLK_L_DISP1 | CLK_L_HOST1X;
	CLOCK(CLK_RST_CONTROLLER_CLK_ENB_X_SET) = CLK_X_UART_FST_MIPI_CAL;
	CLOCK(CLK_RST_CONTROLLER_CLK_SOURCE_UART_FST_MIPI_CAL) = 0xA;
	CLOCK(CLK_RST_CONTROLLER_CLK_ENB_W_SET) = CLK_W_DSIA_LP;
	CLOCK(CLK_RST_CONTROLLER_CLK_SOURCE_DSIA_LP) = 0xA;

	//DPD idle.
	PMC(APBDEV_PMC_IO_DPD_REQ) = 0x40000000;
	PMC(APBDEV_PMC_IO_DPD2_REQ) = 0x40000000;

	//Config pins.
	pinmux_set_config(PINMUX_GPIO_I0, pinmux_get_config(PINMUX_GPIO_I0) & (~PINMUX_TRISTATE));
	pinmux_set_config(PINMUX_GPIO_I1, pinmux_get_config(PINMUX_GPIO_I1) & (~PINMUX_TRISTATE));
	pinmux_set_config(PINMUX_LCD_BL_PWM_INDEX, pinmux_get_config(PINMUX_LCD_BL_PWM_INDEX) & (~PINMUX_TRISTATE));
	pinmux_set_config(PINMUX_LCD_BL_EN_INDEX, pinmux_get_config(PINMUX_LCD_BL_EN_INDEX) & (~PINMUX_TRISTATE));
	pinmux_set_config(PINMUX_LCD_RST_INDEX, pinmux_get_config(PINMUX_LCD_RST_INDEX) & (~PINMUX_TRISTATE));

	gpio_config(GPIO_PORT_I, GPIO_PIN_0 | GPIO_PIN_1, GPIO_MODE_GPIO);
	gpio_output_enable(GPIO_PORT_I, GPIO_PIN_0 | GPIO_PIN_1, GPIO_OUTPUT_ENABLE);
	gpio_write(GPIO_PORT_I, GPIO_PIN_0, GPIO_HIGH);

	sleep(10000);
	gpio_write(GPIO_PORT_I, GPIO_PIN_1, GPIO_HIGH);
	sleep(10000);

	gpio_config(GPIO_PORT_V, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_MODE_GPIO);
	gpio_output_enable(GPIO_PORT_V, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_OUTPUT_ENABLE);
	gpio_write(GPIO_BY_NAME(LCD_BL_EN), GPIO_HIGH); //V1

	//Config display interface and display.
	MIPI_CAL(0x60) = 0;

	exec_cfg((u32 *)CLOCK_BASE, _display_config_1, ARRAY_SIZE(_display_config_1));
	exec_cfg((u32 *)DISPLAY_A_BASE, _display_config_2, ARRAY_SIZE(_display_config_2));
	exec_cfg((u32 *)DSI_BASE, _display_config_3, ARRAY_SIZE(_display_config_3));

	sleep(10000);
	gpio_write(GPIO_BY_NAME(LCD_RST), GPIO_HIGH); //V2
	sleep(60000);

	DSI(_DSIREG(DSI_DSI_BTA_TIMING)) = 0x50204;
	DSI(_DSIREG(DSI_DSI_WR_DATA)) = 0x337;
	DSI(_DSIREG(DSI_DSI_TRIGGER)) = 0x2;
	_display_dsi_wait(250000, _DSIREG(DSI_DSI_TRIGGER), 3);

	DSI(_DSIREG(DSI_DSI_WR_DATA)) = 0x406;
	DSI(_DSIREG(DSI_DSI_TRIGGER)) = 0x2;
	_display_dsi_wait(250000, _DSIREG(DSI_DSI_TRIGGER), 3);

	DSI(_DSIREG(DSI_HOST_DSI_CONTROL)) = 0x200B;
	_display_dsi_wait(150000, _DSIREG(DSI_HOST_DSI_CONTROL), 8);

	sleep(5000);

	_display_ver = DSI(_DSIREG(DSI_DSI_RD_DATA));
	if (_display_ver == 0x10)
		exec_cfg((u32 *)DSI_BASE, _display_config_4, ARRAY_SIZE(_display_config_4));

	DSI(_DSIREG(DSI_DSI_WR_DATA)) = 0x1105;
	DSI(_DSIREG(DSI_DSI_TRIGGER)) = 0x2;

	sleep(180000);

	DSI(_DSIREG(DSI_DSI_WR_DATA)) = 0x2905;
	DSI(_DSIREG(DSI_DSI_TRIGGER)) = 0x2;

	sleep(20000);

	exec_cfg((u32 *)DSI_BASE, _display_config_5, ARRAY_SIZE(_display_config_5));
	exec_cfg((u32 *)CLOCK_BASE, _display_config_6, ARRAY_SIZE(_display_config_6));
	DISPLAY_A(_DIREG(DC_DISP_DISP_CLOCK_CONTROL)) = 4;
	exec_cfg((u32 *)DSI_BASE, _display_config_7, ARRAY_SIZE(_display_config_7));

	sleep(10000);

	exec_cfg((u32 *)MIPI_CAL_BASE, _display_config_8, ARRAY_SIZE(_display_config_8));
	exec_cfg((u32 *)DSI_BASE, _display_config_9, ARRAY_SIZE(_display_config_9));
	exec_cfg((u32 *)MIPI_CAL_BASE, _display_config_10, ARRAY_SIZE(_display_config_10));

	sleep(10000);

	exec_cfg((u32 *)DISPLAY_A_BASE, _display_config_11, ARRAY_SIZE(_display_config_11));
}

void display_end()
{
	display_enable_backlight(0);
	DSI(_DSIREG(DSI_DSI_VID_MODE_CONTROL)) = 1;
	DSI(_DSIREG(DSI_DSI_WR_DATA)) = 0x2805;

	u32 end = HOST1X(0x30A4) + 5;
	while (HOST1X(0x30A4) < end) {}

	DISPLAY_A(_DIREG(DC_CMD_STATE_ACCESS)) = 5;
	DSI(_DSIREG(DSI_DSI_VID_MODE_CONTROL)) = 0;

	exec_cfg((u32 *)DISPLAY_A_BASE, _display_config_12, ARRAY_SIZE(_display_config_12));
	exec_cfg((u32 *)DSI_BASE, _display_config_13, ARRAY_SIZE(_display_config_13));

	sleep(10000);

	if (_display_ver == 0x10)
		exec_cfg((u32 *)DSI_BASE, _display_config_14, ARRAY_SIZE(_display_config_14));

	DSI(_DSIREG(DSI_DSI_WR_DATA)) = 0x1005;
	DSI(_DSIREG(DSI_DSI_TRIGGER)) = 2;

	sleep(50000);
	gpio_write(GPIO_BY_NAME(LCD_RST), GPIO_LOW); //V2
	sleep(10000);
	gpio_write(GPIO_PORT_I, GPIO_PIN_1, GPIO_LOW);
	sleep(10000);
	gpio_write(GPIO_PORT_I, GPIO_PIN_0, GPIO_LOW);
	sleep(10000);

	//Disable clocks.
	CLOCK(CLK_RST_CONTROLLER_RST_DEV_H_SET) = CLK_H_DSI | CLK_H_MIPI_CAL;
	CLOCK(CLK_RST_CONTROLLER_CLK_ENB_H_CLR) = CLK_H_DSI | CLK_H_MIPI_CAL;
	CLOCK(CLK_RST_CONTROLLER_RST_DEV_L_SET) = CLK_L_DISP1 | CLK_L_HOST1X;
	CLOCK(CLK_RST_CONTROLLER_CLK_ENB_L_CLR) = CLK_L_DISP1 | CLK_L_HOST1X;

	DSI(_DSIREG(DSI_PAD_CONTROL)) = 0x10F010F;
	DSI(_DSIREG(DSI_DSI_POWER_CONTROL)) = 0;

	gpio_config(GPIO_BY_NAME(LCD_BL_PWM), GPIO_MODE_SPIO);

	pinmux_set_config(PINMUX_LCD_BL_PWM_INDEX, pinmux_get_config(PINMUX_LCD_BL_PWM_INDEX) | PINMUX_TRISTATE);
	pinmux_set_config(PINMUX_LCD_BL_PWM_INDEX, (pinmux_get_config(PINMUX_LCD_BL_PWM_INDEX) & (~PINMUX_FUNC_MASK)) | PINMUX_LCD_BL_PWM_FUNC_PWM0);
}

void display_color_screen(u32 color)
{
	exec_cfg((u32 *)DISPLAY_A_BASE, cfg_display_one_color, 8);

	//Configure display to show single color.
	DISPLAY_A(_DIREG(DC_WIN_AD_WIN_OPTIONS)) = 0;
	DISPLAY_A(_DIREG(DC_WIN_BD_WIN_OPTIONS)) = 0;
	DISPLAY_A(_DIREG(DC_WIN_CD_WIN_OPTIONS)) = 0;
	DISPLAY_A(_DIREG(DC_DISP_BLEND_BACKGROUND_COLOR)) = color;
	DISPLAY_A(_DIREG(DC_CMD_STATE_CONTROL)) = (DISPLAY_A(_DIREG(DC_CMD_STATE_CONTROL)) & 0xFFFFFFFE) | 1;

	sleep(35000);

	display_enable_backlight(1);
}

u32 *display_init_framebuffer(u32 *fb)
{
	//This configures the framebuffer @ 0xC0000000 with a resolution of 1280x720 (line stride 768).
	exec_cfg((u32 *)DISPLAY_A_BASE, cfg_display_framebuffer, ARRAY_SIZE(cfg_display_framebuffer));

	sleep(35000);

	return (u32 *)0xC0000000;
}

void display_enable_backlight(u32 on) 
{
	gpio_write(GPIO_BY_NAME(LCD_BL_PWM), on); //V0
}