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

#ifndef _CLOCK_H_
#define _CLOCK_H_

#include "types.h"

/*! Clock registers. */
#define CLK_RST_CONTROLLER_RST_DEVICES_L 0x4
#define CLK_RST_CONTROLLER_RST_DEVICES_H 0x8
#define CLK_RST_CONTROLLER_RST_DEVICES_U 0xC
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_L 0x10
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_H 0x14
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_U 0x18
#define CLK_RST_CONTROLLER_CCLK_BURST_POLICY 0x20
#define CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER 0x24
#define CLK_RST_CONTROLLER_SCLK_BURST_POLICY 0x28
#define CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER 0x2C
#define CLK_RST_CONTROLLER_CLK_SYSTEM_RATE 0x30
#define CLK_RST_CONTROLLER_MISC_CLK_ENB 0x48
#define CLK_RST_CONTROLLER_OSC_CTRL 0x50
#define CLK_RST_CONTROLLER_PLLM_BASE 0x90
#define CLK_RST_CONTROLLER_PLLM_OUT 0x94
#define CLK_RST_CONTROLLER_PLLM_MISC1 0x98
#define CLK_RST_CONTROLLER_PLLM_MISC2 0x9c
#define CLK_RST_CONTROLLER_PLLX_BASE 0xE0
#define CLK_RST_CONTROLLER_PLLX_MISC 0xE4
#define CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC1 0x150
#define CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC2 0x154
#define CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC4 0x164
#define CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC3 0x1BC
#define CLK_RST_CONTROLLER_CLK_SOURCE_EMC 0x19C
#define CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL 0x664
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_X 0x280
#define CLK_RST_CONTROLLER_CLK_ENB_X_SET 0x284
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_Y 0x298
#define CLK_RST_CONTROLLER_CLK_ENB_Y_SET 0x29C
#define CLK_RST_CONTROLLER_RST_DEV_L_SET 0x300
#define CLK_RST_CONTROLLER_RST_DEV_L_CLR 0x304
#define CLK_RST_CONTROLLER_RST_DEV_H_SET 0x308
#define CLK_RST_CONTROLLER_RST_DEV_H_CLR 0x30C
#define CLK_RST_CONTROLLER_RST_DEV_U_SET 0x310
#define CLK_RST_CONTROLLER_RST_DEV_U_CLR 0x314
#define CLK_RST_CONTROLLER_CLK_ENB_L_SET 0x320
#define CLK_RST_CONTROLLER_CLK_ENB_L_CLR 0x324
#define CLK_RST_CONTROLLER_CLK_ENB_H_SET 0x328
#define CLK_RST_CONTROLLER_CLK_ENB_U_SET 0x330
#define CLK_RST_CONTROLLER_CLK_ENB_U_CLR 0x334
#define CLK_RST_CONTROLLER_RST_DEVICES_V 0x358
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_V 0x360
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_W 0x364
#define CLK_RST_CONTROLLER_CPU_SOFTRST_CTRL2 0x388
#define CLK_RST_CONTROLLER_CLK_SOURCE_MSELECT 0x3B4
#define CLK_RST_CONTROLLER_CLK_ENB_V_SET 0x440
#define CLK_RST_CONTROLLER_CLK_ENB_W_CLR 0x44C
#define CLK_RST_CONTROLLER_RST_CPUG_CMPLX_CLR 0x454
#define CLK_RST_CONTROLLER_PLLX_MISC_3 0x518
#define CLK_RST_CONTROLLER_SPARE_REG0 0x55C
#define CLK_RST_CONTROLLER_PLLMB_BASE 0x5E8
#define CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC_LEGACY_TM 0x694

enum {
	CLK_L_CPU = 0x1 << 0,
	CLK_L_COP = 0x1 << 1,
	CLK_L_TRIG_SYS = 0x1 << 2,
	CLK_L_RTC = 0x1 << 4,
	CLK_L_TMR = 0x1 << 5,
	CLK_L_UARTA = 0x1 << 6,
	CLK_L_UARTB = 0x1 << 7,
	CLK_L_GPIO = 0x1 << 8,
	CLK_L_SDMMC2 = 0x1 << 9,
	CLK_L_SPDIF = 0x1 << 10,
	CLK_L_I2S2 = 0x1 << 11,
	CLK_L_I2C1 = 0x1 << 12,
	CLK_L_NDFLASH = 0x1 << 13,
	CLK_L_SDMMC1 = 0x1 << 14,
	CLK_L_SDMMC4 = 0x1 << 15,
	CLK_L_PWM = 0x1 << 17,
	CLK_L_I2S3 = 0x1 << 18,
	CLK_L_EPP = 0x1 << 19,
	CLK_L_VI = 0x1 << 20,
	CLK_L_2D = 0x1 << 21,
	CLK_L_USBD = 0x1 << 22,
	CLK_L_ISP = 0x1 << 23,
	CLK_L_3D = 0x1 << 24,
	CLK_L_DISP2 = 0x1 << 26,
	CLK_L_DISP1 = 0x1 << 27,
	CLK_L_HOST1X = 0x1 << 28,
	CLK_L_VCP = 0x1 << 29,
	CLK_L_I2S1 = 0x1 << 30,
	CLK_L_CACHE2 = 0x1 << 31,

	CLK_H_MEM = 0x1 << 0,
	CLK_H_AHBDMA = 0x1 << 1,
	CLK_H_APBDMA = 0x1 << 2,
	CLK_H_KBC = 0x1 << 4,
	CLK_H_STAT_MON = 0x1 << 5,
	CLK_H_PMC = 0x1 << 6,
	CLK_H_FUSE = 0x1 << 7,
	CLK_H_KFUSE = 0x1 << 8,
	CLK_H_SBC1 = 0x1 << 9,
	CLK_H_SNOR = 0x1 << 10,
	CLK_H_JTAG2TBC = 0x1 << 11,
	CLK_H_SBC2 = 0x1 << 12,
	CLK_H_SBC3 = 0x1 << 14,
	CLK_H_I2C5 = 0x1 << 15,
	CLK_H_DSI = 0x1 << 16,
	CLK_H_HSI = 0x1 << 18,
	CLK_H_HDMI = 0x1 << 19,
	CLK_H_CSI = 0x1 << 20,
	CLK_H_I2C2 = 0x1 << 22,
	CLK_H_UARTC = 0x1 << 23,
	CLK_H_MIPI_CAL = 0x1 << 24,
	CLK_H_EMC = 0x1 << 25,
	CLK_H_USB2 = 0x1 << 26,
	CLK_H_USB3 = 0x1 << 27,
	CLK_H_MPE = 0x1 << 28,
	CLK_H_VDE = 0x1 << 29,
	CLK_H_BSEA = 0x1 << 30,
	CLK_H_BSEV = 0x1 << 31,

	CLK_U_UARTD = 0x1 << 1,
	CLK_U_UARTE = 0x1 << 2,
	CLK_U_I2C3 = 0x1 << 3,
	CLK_U_SBC4 = 0x1 << 4,
	CLK_U_SDMMC3 = 0x1 << 5,
	CLK_U_PCIE = 0x1 << 6,
	CLK_U_OWR = 0x1 << 7,
	CLK_U_AFI = 0x1 << 8,
	CLK_U_CSITE = 0x1 << 9,
	CLK_U_PCIEXCLK = 0x1 << 10,
	CLK_U_AVPUCQ = 0x1 << 11,
	CLK_U_TRACECLKIN = 0x1 << 13,
	CLK_U_SOC_THERM = 0x1 << 14,
	CLK_U_DTV = 0x1 << 15,
	CLK_U_NAND_SPEED = 0x1 << 16,
	CLK_U_I2C_SLOW = 0x1 << 17,
	CLK_U_DSIB = 0x1 << 18,
	CLK_U_TSEC = 0x1 << 19,
	CLK_U_IRAMA = 0x1 << 20,
	CLK_U_IRAMB = 0x1 << 21,
	CLK_U_IRAMC = 0x1 << 22,

	// Clock reset.
	CLK_U_EMUCIF = 0x1 << 23,
	// Clock enable.
	CLK_U_IRAMD = 0x1 << 23,

	CLK_U_CRAM2 = 0x2 << 24,
	CLK_U_XUSB_HOST = 0x1 << 25,
	CLK_U_MSENC = 0x1 << 27,
	CLK_U_SUS_OUT = 0x1 << 28,
	CLK_U_DEV2_OUT = 0x1 << 29,
	CLK_U_DEV1_OUT = 0x1 << 30,
	CLK_U_XUSB_DEV = 0x1 << 31,

	CLK_V_CPUG = 0x1 << 0,
	CLK_V_CPULP = 0x1 << 1,
	CLK_V_3D2 = 0x1 << 2,
	CLK_V_MSELECT = 0x1 << 3,
	CLK_V_I2S4 = 0x1 << 5,
	CLK_V_I2S5 = 0x1 << 6,
	CLK_V_I2C4 = 0x1 << 7,
	CLK_V_SBC5 = 0x1 << 8,
	CLK_V_SBC6 = 0x1 << 9,
	CLK_V_AHUB = 0x1 << 10,
	CLK_V_APB2APE = 0x1 << 11,
	CLK_V_HDA2CODEC_2X = 0x1 << 15,
	CLK_V_ATOMICS = 0x1 << 16,
	CLK_V_ACTMON = 0x1 << 23,
	CLK_V_EXTPERIPH1 = 0x1 << 24,
	CLK_V_SATA = 0x1 << 28,
	CLK_V_HDA = 0x1 << 29,

	CLK_W_HDA2HDMICODEC = 0x1 << 0,
	CLK_W_SATACOLD = 0x1 << 1,
	CLK_W_CEC = 0x1 << 8,
	CLK_W_XUSB_PADCTL = 0x1 << 14,
	CLK_W_ENTROPY = 0x1 << 21,
	CLK_W_DVFS = 0x1 << 27,
	CLK_W_XUSB_SS = 0x1 << 28,

	CLK_X_GPU = 0x1 << 24,
	CLK_X_SOR1 = 0x1 << 23,
	CLK_X_SOR0 = 0x1 << 22,
	CLK_X_DPAUX = 0x1 << 21,
	CLK_X_VIC = 0x1 << 18,
	CLK_X_UART_FST_MIPI_CAL = 0x1 << 17,
	CLK_X_MIPIBIF = 0x1 << 13,
	CLK_X_I2C6 = 0x1 << 6,
	CLK_X_ETR = 0x1 << 3,
	CLK_X_SPARE = 0x1 << 0,

	CLK_Y_APE = 0x1 << 6,
	CLK_Y_DPAUX1 = 0x1 << 15,
	CLK_Y_QSPI = 0x1 << 19,
	CLK_Y_SOR_SAFE = 0x1 << 30,
};

/* PLLM specific registers */
#define PLLM_MISC1_SETUP_SHIFT		0
#define PLLM_MISC1_PD_LSHIFT_PH45_SHIFT		28
#define PLLM_MISC1_PD_LSHIFT_PH90_SHIFT		29
#define PLLM_MISC1_PD_LSHIFT_PH135_SHIFT	30
#define PLLM_MISC2_KCP_SHIFT		1
#define PLLM_MISC2_KVCO_SHIFT		0
#define PLLM_OUT1_RSTN_RESET_DISABLE	(1 << 0)
#define PLLM_EN_LCKDET          	(1 << 4)

/* CLK_RST_CONTROLLER_PLL*_BASE_0 */
#define PLL_BASE_BYPASS			(1U << 31)
#define PLL_BASE_ENABLE			(1U << 30)
#define PLL_BASE_REF_DIS		(1U << 29)
#define PLL_BASE_OVRRIDE		(1U << 28)
#define PLL_BASE_LOCK			(1U << 27)
#define PLLC_BASE_LOCK			(1U << 26)

#define PLL_BASE_DIVP_SHIFT		20
#define PLL_BASE_DIVP_MASK		(7U << PLL_BASE_DIVP_SHIFT)

#define PLL_BASE_DIVN_SHIFT		8
#define PLL_BASE_DIVN_MASK		(0x3ffU << PLL_BASE_DIVN_SHIFT)

#define PLL_BASE_DIVM_SHIFT		0
#define PLL_BASE_DIVM_MASK		(0x1f << PLL_BASE_DIVM_SHIFT)

/* SPECIAL CASE: PLLM, PLLC and PLLX use different-sized fields here */
#define PLLCX_BASE_DIVP_MASK		(0xfU << PLL_BASE_DIVP_SHIFT)
#define PLLM_BASE_DIVP_MASK			(0x1fU << PLL_BASE_DIVP_SHIFT)
#define PLLCMX_BASE_DIVN_MASK		(0xffU << PLL_BASE_DIVN_SHIFT)
#define PLLCMX_BASE_DIVM_MASK		(0xffU << PLL_BASE_DIVM_SHIFT)

#define PLLX_IDDQ_SHIFT			3
#define PLLX_IDDQ_MASK			(1U << PLLX_IDDQ_SHIFT)

#define CLK_DIVISOR_MASK		(0xffff)

#define CLK_SOURCE_SHIFT		29
#define CLK_SOURCE_MASK			(0x7 << CLK_SOURCE_SHIFT)

#define CLK_SOURCE_EMC_MC_EMC_SAME_FREQ (1 << 16)
#define EMC_2X_CLK_SRC_SHIFT		29
#define PLLM_UD				4

/* PLL stabilization delay in usec */
#define CLOCK_PLL_STABLE_DELAY_US 300

#define IO_STABILIZATION_DELAY (2)
#define LOGIC_STABILIZATION_DELAY (2)

/* Bits to enable/reset modules */
#define CLK_ENB_CPU			(1 << 0)
#define SWR_TRIG_SYS_RST		(1 << 2)
#define SWR_CSITE_RST			(1 << 9)
#define CLK_ENB_CSITE			(1 << 9)
#define CLK_ENB_EMC_DLL			(1 << 14)

/*! Generic clock descriptor. */
typedef struct _clock_t
{
	u32 reset;
	u32 enable;
	u32 source;
	u8 index;
	u8 clk_src;
	u8 clk_div;
} clock_t;

/*! Generic clock enable/disable. */
void clock_enable(const clock_t *clk);
void clock_disable(const clock_t *clk);

/*! Clock control for specific hardware portions. */
void clock_enable_fuse(u32 enable);
void clock_enable_uart(u32 idx);
void clock_enable_i2c(u32 idx);
void clock_enable_se();
void clock_enable_host1x();
void clock_disable_host1x();
void clock_enable_tsec();
void clock_disable_tsec();
void clock_enable_sor_safe();
void clock_disable_sor_safe();
void clock_enable_sor0();
void clock_disable_sor0();
void clock_enable_sor1();
void clock_disable_sor1();
void clock_enable_kfuse();
void clock_disable_kfuse();
void clock_enable_cl_dvfs();
void clock_enable_coresight();
void clock_sdmmc_config_clock_source(u32 *pout, u32 id, u32 val);
void clock_sdmmc_get_params(u32 *pout, u16 *pdivisor, u32 type);
int clock_sdmmc_is_not_reset_and_enabled(u32 id);
void clock_sdmmc_enable(u32 id, u32 val);
void clock_sdmmc_disable(u32 id);

void clock_halt_bpmp(void);

#endif
