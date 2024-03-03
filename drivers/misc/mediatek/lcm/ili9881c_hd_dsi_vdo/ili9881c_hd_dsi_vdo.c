/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#define LCM_ID_NT35695 (0xf5)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;


#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#ifdef GPIO_LCM_PWR_EN
#define GPIO_LCD_PWR_EN      GPIO_LCM_PWR_EN
#else
#define GPIO_LCD_PWR_EN      0xFFFFFFFF
#endif

#ifdef GPIO_LCM_RST
#define GPIO_LCD_RST_EN     GPIO_LCM_RST
#else
#define GPIO_LCD_RST_EN     0xFFFFFFFF
#endif
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH		(720)
#define FRAME_HEIGHT	(1440)
//#define VIRTUAL_WIDTH	(1080)
//#define VIRTUAL_HEIGHT	(1920)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH (68000)
#define LCM_PHYSICAL_HEIGHT (136000)
#define LCM_DENSITY	(320)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	    0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
{0xFF, 0x03, {0x98, 0x81, 0x03}},
{0x01, 0x01, {0x00}},
{0x02, 0x01, {0x00}},
{0x03, 0x01, {0x53}},
{0x04, 0x01, {0x53}},
{0x05, 0x01, {0x13}},
{0x06, 0x01, {0x04}},
{0x07, 0x01, {0x02}},
{0x08, 0x01, {0x02}},
{0x09, 0x01, {0x00}},
{0x0a, 0x01, {0x00}},
{0x0b, 0x01, {0x00}},
{0x0c, 0x01, {0x00}},
{0x0d, 0x01, {0x00}},
{0x0e, 0x01, {0x00}},
{0x0f, 0x01, {0x00}},
{0x10, 0x01, {0x00}},
{0x11, 0x01, {0x00}},
{0x12, 0x01, {0x00}},
{0x13, 0x01, {0x00}},
{0x14, 0x01, {0x00}},
{0x15, 0x01, {0x00}},
{0x16, 0x01, {0x00}},
{0x17, 0x01, {0x00}},
{0x18, 0x01, {0x00}},
{0x19, 0x01, {0x00}},
{0x1a, 0x01, {0x00}},
{0x1b, 0x01, {0x00}},
{0x1c, 0x01, {0x00}},
{0x1d, 0x01, {0x00}},
{0x1e, 0x01, {0xC0}},
{0x1f, 0x01, {0x80}},
{0x20, 0x01, {0x02}},
{0x21, 0x01, {0x09}},
{0x22, 0x01, {0x00}},
{0x23, 0x01, {0x00}},
{0x24, 0x01, {0x00}},
{0x25, 0x01, {0x00}},
{0x26, 0x01, {0x00}},
{0x27, 0x01, {0x00}},
{0x28, 0x01, {0x55}},
{0x29, 0x01, {0x03}},
{0x2a, 0x01, {0x00}},
{0x2b, 0x01, {0x00}},
{0x2c, 0x01, {0x00}},
{0x2d, 0x01, {0x00}},
{0x2e, 0x01, {0x00}},
{0x2f, 0x01, {0x00}},
{0x30, 0x01, {0x00}},
{0x31, 0x01, {0x00}},
{0x32, 0x01, {0x00}},
{0x33, 0x01, {0x00}},
{0x34, 0x01, {0x03}},
{0x35, 0x01, {0x00}},
{0x36, 0x01, {0x05}},
{0x37, 0x01, {0x00}},
{0x38, 0x01, {0x3C}},
{0x39, 0x01, {0x00}},
{0x3a, 0x01, {0x00}},
{0x3b, 0x01, {0x00}},
{0x3c, 0x01, {0x00}},
{0x3d, 0x01, {0x00}},
{0x3e, 0x01, {0x00}},
{0x3f, 0x01, {0x00}},
{0x40, 0x01, {0x00}},
{0x41, 0x01, {0x00}},
{0x42, 0x01, {0x00}},
{0x43, 0x01, {0x00}},
{0x44, 0x01, {0x00}},
{0x50, 0x01, {0x01}},
{0x51, 0x01, {0x23}},
{0x52, 0x01, {0x45}},
{0x53, 0x01, {0x67}},
{0x54, 0x01, {0x89}},
{0x55, 0x01, {0xab}},
{0x56, 0x01, {0x01}},
{0x57, 0x01, {0x23}},
{0x58, 0x01, {0x45}},
{0x59, 0x01, {0x67}},
{0x5a, 0x01, {0x89}},
{0x5b, 0x01, {0xab}},
{0x5c, 0x01, {0xcd}},
{0x5d, 0x01, {0xef}},
{0x5e, 0x01, {0x01}},
{0x5f, 0x01, {0x14}},
{0x60, 0x01, {0x15}},
{0x61, 0x01, {0x0C}},
{0x62, 0x01, {0x0D}},
{0x63, 0x01, {0x0E}},
{0x64, 0x01, {0x0F}},
{0x65, 0x01, {0x10}},
{0x66, 0x01, {0x11}},
{0x67, 0x01, {0x08}},
{0x68, 0x01, {0x02}},
{0x69, 0x01, {0x0A}},
{0x6a, 0x01, {0x02}},
{0x6b, 0x01, {0x02}},
{0x6c, 0x01, {0x02}},
{0x6d, 0x01, {0x02}},
{0x6e, 0x01, {0x02}},
{0x6f, 0x01, {0x02}},
{0x70, 0x01, {0x02}},
{0x71, 0x01, {0x02}},
{0x72, 0x01, {0x06}},
{0x73, 0x01, {0x02}},
{0x74, 0x01, {0x02}},
{0x75, 0x01, {0x14}},
{0x76, 0x01, {0x15}},
{0x77, 0x01, {0x11}},
{0x78, 0x01, {0x10}},
{0x79, 0x01, {0x0f}},
{0x7a, 0x01, {0x0e}},
{0x7b, 0x01, {0x0d}},
{0x7c, 0x01, {0x0c}},
{0x7d, 0x01, {0x06}},
{0x7e, 0x01, {0x02}},
{0x7f, 0x01, {0x0A}},
{0x80, 0x01, {0x02}},
{0x81, 0x01, {0x02}},
{0x82, 0x01, {0x02}},
{0x83, 0x01, {0x02}},
{0x84, 0x01, {0x02}},
{0x85, 0x01, {0x02}},
{0x86, 0x01, {0x02}},
{0x87, 0x01, {0x02}},
{0x88, 0x01, {0x08}},
{0x89, 0x01, {0x02}},
{0x8A, 0x01, {0x02}},
{0xFF, 0x03, {0x98, 0x81, 0x04}},
{0x38, 0x01, {0x01}},
{0x39, 0x01, {0x00}},
{0x6C, 0x01, {0x15}},
{0x6E, 0x01, {0x2F}},
{0x6F, 0x01, {0x55}},
{0x8D, 0x01, {0x1F}},
{0x87, 0x01, {0xBA}},
{0x26, 0x01, {0x76}},
{0xB2, 0x01, {0xD1}},
{0x88, 0x01, {0x0B}},
{0x35, 0x01, {0x1F}},
{0x3A, 0x01, {0x24}},
{0x33, 0x01, {0x00}},
{0x7A, 0x01, {0x0F}},
{0xB5, 0x01, {0x07}},
{0xFF, 0x03, {0x98, 0x81, 0x01}},
{0x22, 0x01, {0x0A}},
{0x31, 0x01, {0x00}},
{0x53, 0x01, {0x77}},
{0x55, 0x01, {0x6E}},
{0x50, 0x01, {0xC0}},
{0x51, 0x01, {0xC0}},
{0x60, 0x01, {0x11}},
{0x61, 0x01, {0x00}},
{0x62, 0x01, {0x20}},
{0x63, 0x01, {0x00}},
{0x2E, 0x01, {0xF0}},
{0xA0, 0x01, {0x05}},
{0xA1, 0x01, {0x0F}},
{0xA2, 0x01, {0x1A}},
{0xA3, 0x01, {0x10}},
{0xA4, 0x01, {0x13}},
{0xA5, 0x01, {0x27}},
{0xA6, 0x01, {0x1B}},
{0xA7, 0x01, {0x1E}},
{0xA8, 0x01, {0x5D}},
{0xA9, 0x01, {0x1B}},
{0xAA, 0x01, {0x27}},
{0xAB, 0x01, {0x4A}},
{0xAC, 0x01, {0x18}},
{0xAD, 0x01, {0x16}},
{0xAE, 0x01, {0x4A}},
{0xAF, 0x01, {0x20}},
{0xB0, 0x01, {0x26}},
{0xB1, 0x01, {0x48}},
{0xB2, 0x01, {0x5E}},
{0xB3, 0x01, {0x3F}},
{0xC0, 0x01, {0x04}},
{0xC1, 0x01, {0x0D}},
{0xC2, 0x01, {0x19}},
{0xC3, 0x01, {0x11}},
{0xC4, 0x01, {0x13}},
{0xC5, 0x01, {0x26}},
{0xC6, 0x01, {0x1C}},
{0xC7, 0x01, {0x1E}},
{0xC8, 0x01, {0x5C}},
{0xC9, 0x01, {0x1C}},
{0xCA, 0x01, {0x28}},
{0xCB, 0x01, {0x48}},
{0xCC, 0x01, {0x18}},
{0xCD, 0x01, {0x16}},
{0xCE, 0x01, {0x4B}},
{0xCF, 0x01, {0x20}},
{0xD0, 0x01, {0x28}},
{0xD1, 0x01, {0x47}},
{0xD2, 0x01, {0x5E}},
{0xD3, 0x01, {0x3F}},
{0xFF, 0x03, {0x98, 0x81, 0x00}},
{0x11, 1, {0x00} },
{REGFLAG_DELAY, 120, {} },
{0x29, 1, {0x00} },
{REGFLAG_DELAY, 20, {} },
{REGFLAG_END_OF_TABLE, 0x00, {} }
};

#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A, 4, {0x00, 0x00, (FRAME_WIDTH >> 8), (FRAME_WIDTH & 0xFF)} },
	{0x2B, 4, {0x00, 0x00, (FRAME_HEIGHT >> 8), (FRAME_HEIGHT & 0xFF)} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
	/* Sleep Out */
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Display off sequence */
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	/* Sleep Mode On */
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i, cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	//params->virtual_width = VIRTUAL_WIDTH;
	//params->virtual_height = VIRTUAL_HEIGHT;

	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	//params->density		   = LCM_DENSITY;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	pr_debug("[LCM]lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = 4;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 6;
	params->dsi.vertical_backporch = 4;
	params->dsi.vertical_frontporch = 14;
	//params->dsi.vertical_frontporch_for_low_power = 620;
	params->dsi.vertical_active_line = 1440;

	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 40;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = 720;
	params->dsi.ssc_disable = 0;
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 420;
#else
	params->dsi.PLL_CLOCK = 217;//440;
#endif
	params->dsi.PLL_CK_CMD = 420;
	params->dsi.PLL_CK_VDO = 217;//440;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x53;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;


	/* for ARR 2.0 */
	params->max_refresh_rate = 60;
	params->min_refresh_rate = 45;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->full_content = 0;
	params->corner_pattern_width = 1080;
	params->corner_pattern_height = 32;
	params->corner_pattern_height_bot = 32;
#endif
}
enum DTS_GPIO_STATE {
	DTS_GPIO_STATE_LCM_RST_LOW,
	DTS_GPIO_STATE_LCM_RST_HIGH,
	DTS_GPIO_STATE_LCD_IOVCC_LOW,
	DTS_GPIO_STATE_LCD_IOVCC_HIGH,
	DTS_GPIO_STATE_MAX,	/* for array size */
};

extern long disp_dts_gpio_select_state(enum DTS_GPIO_STATE s);

static void lcm_init_power(void)
{
	//display_bias_enable();
}

static void lcm_suspend_power(void)
{
    pr_info("lcm_suspend_power test\n");
	display_bias_disable();
}

static void lcm_resume_power(void)
{
	//SET_RESET_PIN(0);
	pr_info("lcm_resume_power test\n");

	display_bias_enable();
}

static void lcm_init(void)
{
	int size;
	pr_info("lcm_init test\n");


    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_LOW);
	MDELAY(15);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_HIGH);
	MDELAY(1);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_LOW);
	MDELAY(10);

    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_HIGH);
	MDELAY(10);

	if (lcm_dsi_mode == CMD_MODE) {

	} else {
		size = sizeof(init_setting_vdo) /
			sizeof(struct LCM_setting_table);
		push_table(NULL, init_setting_vdo, size, 1);
		pr_info("[LCM]nt35695----tps6132----lcm mode = vdo mode :%d----\n",
			lcm_dsi_mode);
	}
}

static void lcm_suspend(void)
{
	pr_info("lcm_suspend test\n");

	push_table(NULL, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);
	MDELAY(15);
    disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_LOW);
}

static void lcm_resume(void)
{
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
	unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static unsigned int lcm_compare_id(void)
{
		return 1;

}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		pr_debug("[LCM][LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	pr_debug("[LCM][LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	pr_debug("[LCM]ATA check size = 0x%x,0x%x,0x%x,0x%x\n",
		x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	/* read id return two byte,version and id */
	data_array[0] = 0x00043700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	pr_debug("[LCM]%s,nt35695 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value;
 * 2. config mode control register
 */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		/* mode control addr */
		lcm_switch_mode_cmd.addr = 0xBB;
		/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[0] = 0x13;
		/* disable video mode secondly */
		lcm_switch_mode_cmd.val[1] = 0x10;
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		/* disable GRAM and enable video mode */
		lcm_switch_mode_cmd.val[0] = 0x03;
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}

#if (LCM_DSI_CMD_MODE)

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty.
	 * In this case we need to use minimu roi
	 */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		pr_info("%s calc error,assign full roi:y=%d,h=%d\n",
			__func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif


struct LCM_DRIVER ili9881c_hd_dsi_vdo_lcm_drv = {
	.name = "ili9881c_hd_dsi_vdo_drv",

	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
