/*****************************************************************************
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
*****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>


#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_common.h"
#include "imgsensor_sensor_i2c.h"
#include "gc5025_king_sensor.h"

#ifndef TRUE
#define TRUE    (bool) 1
#endif
#ifndef FALSE
#define FALSE   (bool) 0
#endif
#define RETRY_TIMES 2

/****************************Modify Following Strings for Debug****************************/
#define DEBUG_GC5025_KING 0

#define PFX "GC5025_KING"
#define LOG_INF(fmt, args...)   if (DEBUG_GC5025_KING) pr_err(PFX "[%s] " fmt, __FUNCTION__, ##args)
#define LOG_ERR(fmt, args...)    pr_err(PFX "[%s] " fmt, __func__, ##args)

/****************************   Modify end    *******************************************/

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static kal_uint32 g_Dgain_ratio = 1;
static bool g_otp_identify_flag = FALSE;
static struct GC5025_KING_OTP gc5025_otp_info;
#define IMGSENSOR_SENSOR_I2C_READ(sensor, addr, data, data_type) do { \
	if (imgsensor_sensor_i2c_read(sensor, addr, data, data_type) != 0) { \
		LOG_ERR("Imgsensor i2c read failed.\n"); \
	} \
} while (0)

#define IMGSENSOR_SENSOR_I2C_WRITE(sensor, addr, data, data_type) do { \
	if (imgsensor_sensor_i2c_write(sensor, addr, data, data_type) != 0) { \
		LOG_ERR("Imgsensor i2c write failed.\n"); \
	} \
} while (0)

static kal_uint16 gc5025_read_otp(kal_uint16 addr)
{
	kal_uint16 value;
	kal_uint16 regd4;
	kal_uint16 realaddr = addr << 3;

	IMGSENSOR_SENSOR_I2C_READ(&imgsensor, 0xd4, &regd4, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, SENSOR_PAGE_SELECT_REG, 0x00, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xd4,
		(regd4 & 0xfc) + ((realaddr >> 8) & 0x03), IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xd5, realaddr & 0xff, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xf3, OTP_BYTE_MODE, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_READ(&imgsensor, 0xd7, &value, IMGSENSOR_I2C_BYTE_DATA);

	return value;
}

static void gc5025_read_otp_group(kal_uint16 addr, kal_uint16 *buff, int size)
{
	int i;
	kal_uint16 regd4;
	kal_uint16 realaddr = addr << 3;

	IMGSENSOR_SENSOR_I2C_READ(&imgsensor, 0xd4, &regd4, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, SENSOR_PAGE_SELECT_REG, 0x00, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xd4,
		(regd4 & 0xfc) + ((realaddr >> 8) & 0x03), IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xd5, (realaddr & 0xff), IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xf3, OTP_BYTE_MODE, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xf3, OTP_CONTINUE_MODE, IMGSENSOR_I2C_BYTE_DATA);

	for (i = 0; i < size; i++) {
		IMGSENSOR_SENSOR_I2C_READ(&imgsensor, 0xd7, &buff[i], IMGSENSOR_I2C_BYTE_DATA);
		LOG_INF("gc5025_read_otp_GROUP : buff[%d] = 0x%x\n", i, buff[i]);
	}
}

static void gc5025_select_page_otp(kal_uint16 otp_select_page)
{
	kal_uint16 page;

	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, SENSOR_PAGE_SELECT_REG, 0x00, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_READ(&imgsensor, 0xd4, &page, IMGSENSOR_I2C_BYTE_DATA);

	switch (otp_select_page) {
	case OTP_PAGE0:
		page = page & 0xfb;
		break;
	case OTP_PAGE1:
		page = page | 0x04;
		break;
	default:
		break;
	}

	/* must Don't remove,delay for sensor stable */
	mdelay(5);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xd4, page, IMGSENSOR_I2C_BYTE_DATA);
}
static void gc5025_gcore_sort_ddparams(void)
{
	kal_uint16 i;
	kal_uint16 j;
	kal_uint16 temp_x;
	kal_uint16 temp_y;
	kal_uint16 temp_type;
	kal_uint16 window_val1;
	kal_uint16 window_val2;

	/* Rearrange Pixel min to max */
	for (i = 0; i < gc5025_otp_info.dd_cnt - 1; i++) {
		window_val1 = gc5025_otp_info.dd_param[i].y * WINDOW_WIDTH +
					gc5025_otp_info.dd_param[i].x;
		for (j = i + 1; j < gc5025_otp_info.dd_cnt; j++) {
			window_val2 = gc5025_otp_info.dd_param[j].y * WINDOW_WIDTH +
						gc5025_otp_info.dd_param[j].x;
			if (window_val1 > window_val2) {
				temp_x = gc5025_otp_info.dd_param[i].x;
				gc5025_otp_info.dd_param[i].x = gc5025_otp_info.dd_param[j].x;
				gc5025_otp_info.dd_param[j].x = temp_x;
				temp_y = gc5025_otp_info.dd_param[i].y;
				gc5025_otp_info.dd_param[i].y = gc5025_otp_info.dd_param[j].y;
				gc5025_otp_info.dd_param[j].y = temp_y;
				temp_type = gc5025_otp_info.dd_param[i].t;
				gc5025_otp_info.dd_param[i].t = gc5025_otp_info.dd_param[j].t;
				gc5025_otp_info.dd_param[j].t = temp_type;
			}
		}
	}
}

static kal_uint16 gc5025_gcore_calc_ddparam(kal_uint16 size, kal_uint16 *buff)
{
	kal_uint16 i;
	kal_uint16 j;
	kal_uint16 cnt = 0;
	kal_uint16 total_number = size;

	if (!buff) {
		LOG_ERR("dd params buf is null\n");
		return 1;
	}

	total_number = total_number > MAX_DD_NUM ? MAX_DD_NUM : total_number;
	for (i = 0; i < total_number; i++) {
		LOG_INF("GC5025_OTP_DD:buff[%d] = (0x%x, 0x%x, 0x%x, 0x%x)\n",
			i, buff[4 * i], buff[4 * i + 1], buff[4 * i + 2], buff[4 * i + 3]);

		if (buff[4 * i + 3] & 0x10) {
			switch (buff[4 * i + 3] & 0x0f) {
			/* 8-badPiexls */
			case 3:
				for (j = 0; j < 4; j++) {
					gc5025_otp_info.dd_param[cnt].x =
						(((kal_uint16)buff[4 * i + 1] & 0x0f) << 8) +
						buff[4 * i] - gc5025_otp_info.ddoffsetX;
					gc5025_otp_info.dd_param[cnt].y =
						((kal_uint16)buff[4 * i + 2] << 4) +
						((buff[4 * i + 1] & 0xf0) >> 4) + j -
						gc5025_otp_info.ddoffsetY;
					gc5025_otp_info.dd_param[cnt++].t = 2;
				}
				break;
			/* 4-badPiexls */
			case 4:
				for (j = 0; j < 2; j++) {
					gc5025_otp_info.dd_param[cnt].x =
						(((kal_uint16)buff[4 * i + 1] & 0x0f) << 8) +
						buff[4 * i] - gc5025_otp_info.ddoffsetX;
					gc5025_otp_info.dd_param[cnt].y =
						((kal_uint16)buff[4 * i + 2] << 4) +
						((buff[4 * i + 1] & 0xf0) >> 4) + j -
						gc5025_otp_info.ddoffsetY;
					gc5025_otp_info.dd_param[cnt++].t = 2;
				}
				break;
			default:
				gc5025_otp_info.dd_param[cnt].x =
					(((kal_uint16)buff[4 * i + 1] & 0x0f) << 8) + buff[4 * i];
				gc5025_otp_info.dd_param[cnt].y =
					((kal_uint16)buff[4 * i + 2] << 4) +
					((buff[4 * i + 1] & 0xf0) >> 4);
				gc5025_otp_info.dd_param[cnt++].t = buff[4 * i + 3] & 0x0f;
				break;
			}
		} else {
			LOG_INF("GC5025_OTP_DD:check_id[%d] = %x,checkid error!!\n",
				i, buff[4 * i + 3] & 0xf0);
		}
	}

	gc5025_otp_info.dd_cnt = cnt;
	gc5025_gcore_sort_ddparams();

	return 0;
}

static void gc5025_gcore_set_otp_ddparam(kal_uint16 flag)
{
	kal_uint16 i;
	kal_uint16 rc;
	kal_uint16 check = 0;
	kal_uint16 ddchecksum;
	kal_uint16 total_number;
	kal_uint16 flagdd = flag;
	kal_uint16 ddtempbuff[MAX_DD_NUM * DD_INFO] = { 0 };

	/* if flagdd & bit[3:2] equal 0x04;dd offset(x,y)=(252,253) */
	if ((flagdd & DD_START_PIXEL) == 0x04) {
		gc5025_otp_info.ddoffsetX = 252;
		gc5025_otp_info.ddoffsetY = 253;
	} else {
		gc5025_otp_info.ddoffsetX = 0;
		gc5025_otp_info.ddoffsetY = 0;
	}

	/* DD TOTAL PIXEL NUMBER */
	total_number = gc5025_read_otp(0x01) + gc5025_read_otp(0x02);
	LOG_INF("GC5025_OTP : total_number = %d\n", total_number);
	total_number = total_number > MAX_DD_NUM ? MAX_DD_NUM : total_number;

	/* DD READ PIEXL INFORMANTION */
	if (total_number <= PAGE0_DD_SIZE) {
		gc5025_read_otp_group(0x03, &ddtempbuff[0], total_number * DD_INFO);
		gc5025_select_page_otp(OTP_PAGE1);
	} else {
		gc5025_read_otp_group(0x03, &ddtempbuff[0], PAGE0_DD_SIZE * DD_INFO);
		gc5025_select_page_otp(OTP_PAGE1);
		gc5025_read_otp_group(0x29, &ddtempbuff[PAGE0_DD_SIZE * DD_INFO],
			(total_number - PAGE0_DD_SIZE) * DD_INFO);
	}

	/* DD check sum read form page1 */
	ddchecksum = gc5025_read_otp(0x61);
	for (i = 0; i < DD_INFO * total_number; i++) {
		check += ddtempbuff[i];
	}
	if (((check) % 255 + 1) == ddchecksum) {
		LOG_INF("GC5025 DD check sum correct! checksum = 0x%x\n", ddchecksum);
	} else {
		LOG_INF("GC5025 DD check sum error! checksum = 0x%x, sum = 0x%x\n",
			ddchecksum, (check % 255 + 1));
	}

	/* calc badPiexl coordinate & classify badPiexl type */
	rc = gc5025_gcore_calc_ddparam(total_number, ddtempbuff);
	if (rc) {
		LOG_ERR("gc5025 calc ddparams fail!\n");
	}
}

static void gc5025_gcore_set_otp_reginfo(void)
{
	kal_uint8 i = 0;

	LOG_INF("GC55025_OTP_ CHIPVESION = 0x%x\n", gc5025_otp_info.reg_flag);

	switch ((gc5025_otp_info.reg_flag >> 4) & 0x03) {
	/* not add ic consistency */
	case 0x00:
		LOG_INF("GC5025_OTP_CHIPVERSION is Empty !!\n");
		gc5025_otp_info.reg_num = 0;
		break;
	/* add ic consistency */
	case 0x01:
		LOG_INF("GC5025_OTP_CHIPVERSION is Valid !!\n");
		gc5025_select_page_otp(OTP_PAGE1);
		do {
			gc5025_otp_info.reg_addr[i] = gc5025_read_otp(REG_ROM_START + i * 2);
			gc5025_otp_info.reg_value[i] =
				gc5025_read_otp(REG_ROM_START + i * 2 + 1);
			LOG_INF("GC5025_OTP_ CHIPVERSION reg[%d] = (0x%x, 0x%x)\n",
				i, gc5025_otp_info.reg_addr[i], gc5025_otp_info.reg_value[i]);
			i++;
		} while ((gc5025_otp_info.reg_addr[i-1] != 0) &&
				(i < MAX_CHIPVERSION_REG_NUM));
		gc5025_otp_info.reg_num = i - 1;
		break;
	/* ic consistency Invalid */
	case 0x02:
		LOG_INF("GC5025_OTP_CHIPVERSION is Invalid !!\n");
		gc5025_otp_info.reg_num = 0;
		break;
	default:
		LOG_INF("GC5025_OTP_CHIPVERSION is default !!\n");
		gc5025_otp_info.reg_num = 0;
		break;
	}
}

static void gc5025_gcore_read_otp_info(void)
{
	kal_uint16 flagdd;

	memset(&gc5025_otp_info, 0, sizeof(gc5025_otp_info));

	gc5025_select_page_otp(OTP_PAGE0);
	flagdd = gc5025_read_otp(FLAG_DD_REG);
	LOG_INF("GC5025_OTP_DD : flag_dd = 0x%x\n", flagdd);
	gc5025_otp_info.reg_flag = gc5025_read_otp(SENSOR_CHIPVERSION_FLAG);

	/* DD case 0x01~0x03 mean group */
	switch (flagdd & 0x03) {
	case 0x00:
		LOG_INF("GC5025_OTP_DD is Empty !!\n");
		gc5025_otp_info.dd_flag = 0x00;
		break;
	case 0x01:
		LOG_INF("GC5025_OTP_DD is valid !!\n");
		gc5025_otp_info.dd_flag = 0x01;
		gc5025_gcore_set_otp_ddparam(flagdd);
		break;
	case 0x02:
	case 0x03:
	default:
		LOG_INF("GC5025_OTP_DD is Invalid !!\n");
		gc5025_otp_info.dd_flag = 0x02;
		break;
	}

	/* For Chip Version */
	gc5025_gcore_set_otp_reginfo();
}

static void gc5025_gcore_enable_otp(kal_uint16 state)
{
	kal_uint16 otp_en;
	kal_uint16 otp_clk;

	IMGSENSOR_SENSOR_I2C_READ(&imgsensor, 0xfa, &otp_clk, IMGSENSOR_I2C_WORD_DATA);
	IMGSENSOR_SENSOR_I2C_READ(&imgsensor, 0xd4, &otp_en, IMGSENSOR_I2C_BYTE_DATA);

	if (state) {
		otp_clk = otp_clk | 0x10;
		otp_en = otp_en | 0x80;
		/* Don't remove; delay for sensor satble */
		mdelay(5);
		/* 0xfa[6] : OTP_CLK_en */
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xfa, otp_clk, IMGSENSOR_I2C_BYTE_DATA);
		/* 0xd4[7] : OTP_en */
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xd4, otp_en, IMGSENSOR_I2C_BYTE_DATA);
		LOG_INF("GC5025_OTP: Enable OTP!\n");
	} else {
		otp_en = otp_en & 0x7f;
		otp_clk = otp_clk & 0xef;
		/* Don't remove; delay for sensor satble */
		mdelay(5);
		/* 0xfa[6] : OTP_CLK_en */
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xfa, otp_clk, IMGSENSOR_I2C_BYTE_DATA);
		/* 0xd4[7] : OTP_en */
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xd4, otp_en, IMGSENSOR_I2C_BYTE_DATA);
		LOG_INF("GC5025_OTP: Disable OTP!\n");
	}
}

static kal_uint32 gc5025_gcore_initial_otp(void)
{
	kal_int32 rc;

	rc = imgsensor_sensor_write_setting(&imgsensor, &otp_init_setting_array);
	if (rc != 0) {
		LOG_ERR("GC8034_OTP: OTP initial failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("GC8034_OTP: OTP initial succeed!\n");
	return ERROR_NONE;
}

static void gc5025_gcore_identify_otp(void)
{
	if (!g_otp_identify_flag) {
		if((gc5025_gcore_initial_otp() == ERROR_NONE)) {
			gc5025_gcore_enable_otp(OTP_OPEN);
			gc5025_gcore_read_otp_info();
			gc5025_gcore_enable_otp(OTP_CLOSE);
			g_otp_identify_flag = TRUE;
		}
	}
}

static void gc5025_gcore_update_dd(void)
{
	kal_uint16 i;
	kal_uint16 flag;
	kal_uint16 temp_val0;
	kal_uint16 temp_val1;
	kal_uint16 temp_val2;

	if (gc5025_otp_info.dd_flag != 0x01) {
		LOG_INF("gc5025_otp_info.dd_flag = 0x%x.\n", gc5025_otp_info.dd_flag);
		return;
	}
	if (gc5025_otp_info.dd_cnt > IMGSENSOR_ARRAY_SIZE(gc5025_otp_info.dd_param)) {
		LOG_INF("gc5025_otp_info.dd_cnt = 0x%x.\n", gc5025_otp_info.dd_cnt);
		return;
	}

	/* write SRAM for init */
	(void)imgsensor_sensor_write_setting(&imgsensor,
		&otp_sram_write_init_setting_array);

	/* Internal Register Processing Bad Pixels */
	for (i = 0; i < gc5025_otp_info.dd_cnt; i++) {
		temp_val0 = gc5025_otp_info.dd_param[i].x & 0x00ff;
		temp_val1 = (((gc5025_otp_info.dd_param[i].y) << 4) & 0x00f0) +
					((gc5025_otp_info.dd_param[i].x >> 8) & 0X000f);
		temp_val2 = (gc5025_otp_info.dd_param[i].y >> 4) & 0xff;
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xaa, i, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xac,
								temp_val0, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xac,
								temp_val1, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xac,
								temp_val2, IMGSENSOR_I2C_BYTE_DATA);
		while ((i < gc5025_otp_info.dd_cnt - 1) &&
			(gc5025_otp_info.dd_param[i].x == gc5025_otp_info.dd_param[i + 1].x) &&
			(gc5025_otp_info.dd_param[i].y == gc5025_otp_info.dd_param[i + 1].y)) {
			flag = 1;
			i++;
		}
		if (flag) {
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xac,
									0x02, IMGSENSOR_I2C_BYTE_DATA);
			flag = 0;
		} else {
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xac,
						gc5025_otp_info.dd_param[i].t,IMGSENSOR_I2C_BYTE_DATA);
		}
		LOG_INF("GC5025_OTP val0 = 0x%x, val1 = 0x%x, val2 = 0x%x\n",
			temp_val0, temp_val1, temp_val2);
		LOG_INF("GC5025_OTP dd[%d] = (%d, %d);dd_type = %d\n",
			i, ((temp_val1 & 0x0f) << 8) + temp_val0, (temp_val2 << 4) +
			((temp_val1 & 0xf0) >> 4), gc5025_otp_info.dd_param[i].t);
	}
	LOG_INF("dd params update sucess!");
	/* CLOSE SRAM */
	(void)imgsensor_sensor_write_setting(&imgsensor,
		&otp_sram_write_close_setting_array);
}

static void gc5025_gcore_update_chipversion(void)
{
	kal_uint16 i;

	LOG_INF("GC5025_OTP_UPDATE_CHIPVERSION:reg_num%d\n", gc5025_otp_info.reg_num);
	for (i = 0; i < gc5025_otp_info.reg_num; i++) {
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, gc5025_otp_info.reg_addr[i],
			gc5025_otp_info.reg_value[i], IMGSENSOR_I2C_BYTE_DATA);
		LOG_INF("GC5025_OTP_UPDATE_CHIP_VERSION:{0x%x,0x%x}!!\n",
			gc5025_otp_info.reg_addr[i], gc5025_otp_info.reg_value[i]);
	}
}

static void gc5025_gcore_update_otp(void)
{
	gc5025_gcore_update_dd();
	gc5025_gcore_update_chipversion();
}

static void set_dummy(void)
{
	kal_uint32  vb = 32;
	kal_uint32  basic_line = 1968;
	kal_int32 rc = 0;

	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	vb = imgsensor.frame_length - basic_line;
	vb = vb < 32 ? 32 : vb;
	vb = vb > 8191 ? 8191 : vb;
	rc = imgsensor_sensor_i2c_write(&imgsensor,SENSOR_PAGE_SELECT_REG,0x00, IMGSENSOR_I2C_BYTE_DATA);
	if( rc < 0 ){
		LOG_ERR("wtire sensor page ctrl reg failed.");
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor,SENSOR_CISCTRL_CAPT_VB_REG_H, (vb >> 8) & 0x1f, IMGSENSOR_I2C_BYTE_DATA);
	if( rc < 0 ){
		LOG_ERR("wtire sensor csictrl capt vb  h reg  failed.");
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor,SENSOR_CISCTRL_CAPT_VB_REG_L, vb & 0xfe, IMGSENSOR_I2C_BYTE_DATA);
	if( rc < 0 ){
		LOG_ERR("wtire sensor csictrl capt vb  l reg  failed.");
	}
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	LOG_ERR("ENTER\n");

	if(!framerate || !imgsensor.line_length){
		LOG_ERR("Invalid params. framerate=%d, line_length=%d.\n",
			framerate,imgsensor.line_length);
		return;
	}
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en){
		imgsensor.min_frame_length = imgsensor.frame_length;
	}
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
	return;
}	/*  set_max_framerate  */

/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0, cal_shutter = 0;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_INF("Enter set_shutter!\n");

	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin){
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	}else{
		imgsensor.frame_length = imgsensor.min_frame_length;
	}

	if (imgsensor.frame_length > imgsensor_info.max_frame_length){
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	}
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		/* calc fps between 298~305, real fps set to 298 */
		if (realtime_fps >= 298 && realtime_fps <= 305) {
			set_max_framerate(298, 0);
		/* calc fps between 146~150, real fps set to 146 */
		} else if (realtime_fps >= 146 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
		} else {
			set_max_framerate(realtime_fps, 0);
		}
	} else {
		set_max_framerate(realtime_fps, 0);
	}

	cal_shutter = shutter >> 1;
	cal_shutter = cal_shutter << 1;
	g_Dgain_ratio = 256 * shutter / cal_shutter;

	if (cal_shutter <= 10){
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
				0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CAL_SHUTTER_REG_H,
				0xdd, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CAL_SHUTTER_REG_L,
				0x58, IMGSENSOR_I2C_BYTE_DATA);
	}else{
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
				0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CAL_SHUTTER_REG_H,
				0xaa, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CAL_SHUTTER_REG_L,
				0x4d, IMGSENSOR_I2C_BYTE_DATA);
	}

	/* Update Shutter */
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
			0x00, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_BUF_EXP_IN_REG_H,
			(cal_shutter >> 8) & 0x3f, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_BUF_EXP_IN_REG_L,
			cal_shutter & 0xff, IMGSENSOR_I2C_BYTE_DATA);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 real_gain = 0, temp;

	LOG_INF("Enter set_gain!\n");
	real_gain = (gain > SENSOR_MAX_ALL_GAIN) ? SENSOR_MAX_ALL_GAIN : gain;
	if (real_gain < ANALOG_GAIN_1X){
		real_gain = ANALOG_GAIN_1X;
	}

	if((ANALOG_GAIN_1X <= real_gain) &&(real_gain < ANALOG_GAIN_2X)){
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
				0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG,
				0x00, IMGSENSOR_I2C_BYTE_DATA);
		temp = (real_gain * g_Dgain_ratio ) >> 8;
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_H,
				temp >> 6, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_L,
				(temp << 2) & 0xfc, IMGSENSOR_I2C_BYTE_DATA);
		LOG_INF("GC5025_MIPI analogic gain 1x, GC5025_MIPI add pregain = %d\n",temp);
	}else if((ANALOG_GAIN_2X <= real_gain) && (real_gain < ANALOG_GAIN_3X)) {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
				0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG,
				0x01, IMGSENSOR_I2C_BYTE_DATA);
		temp = 64 * real_gain / ANALOG_GAIN_2X;
		temp = (temp * g_Dgain_ratio ) >> 8;
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_H,
				temp >> 6, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_L,
				(temp << 2) & 0xfc, IMGSENSOR_I2C_BYTE_DATA);
		LOG_INF("GC5025_MIPI analogic gain 1.42x, GC5025_MIPI add pregain = %d\n",temp);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
				0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG,
				0x02, IMGSENSOR_I2C_BYTE_DATA);
		temp = 64 * real_gain / ANALOG_GAIN_3X;
		temp = (temp * g_Dgain_ratio ) >> 8;
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_H,
				temp >> 6, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_L,
				(temp << 2) & 0xfc, IMGSENSOR_I2C_BYTE_DATA);
		LOG_INF("GC5025_MIPI analogic gain 1.42x, GC5025_MIPI add pregain = %d\n",temp);
	}
	return gain;
}

static kal_uint32 sensor_init(void)
{
	kal_int32 rc = 0;
	LOG_INF("ENTER.\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.init_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("EXIT.\n");

	return ERROR_NONE;
}	/*  sensor_init  */

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc = 0;
	LOG_INF("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.pre_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("EXIT.\n");

	return ERROR_NONE;
}	/*  preview_setting  */


static kal_uint32 set_capture_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_INF("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.cap_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_INF("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.normal_video_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc = 0;
	kal_uint16 sensor_id = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor, imgsensor_info.sensor_id_reg,
		&sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if( rc < 0 ){
		LOG_ERR("Read id failed.id reg: 0x%x\n", imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	return sensor_id;
}
/*************************************************************************
* FUNCTION
*   sensor_dump_reg
*
* DESCRIPTION
*   This function dump some sensor reg
*
* GLOBALS AFFECTED
*
*************************************************************************/
/*TBD*/
static kal_uint32 sensor_dump_reg(void)
{
	kal_int32 rc = 0;
	LOG_INF("ENTER\n");
	rc = imgsensor_sensor_i2c_process(&imgsensor, &imgsensor_info.dump_info);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
	}
	LOG_INF("EXIT\n");
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
		0x00, IMGSENSOR_I2C_BYTE_DATA);
	if (enable){
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_PATTERN_MODE,
			0x11, IMGSENSOR_I2C_BYTE_DATA);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_PATTERN_MODE,
			0x10, IMGSENSOR_I2C_BYTE_DATA);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = RETRY_TIMES; /* retry 2 time */
	UINT32 reg_sensor_id = 0;
	UINT32 tmp_sensor_id = 0;
	UINT32 vendorID_offset = 4; /* vendorID offset is 4 in eeprom */

	spin_lock(&imgsensor_drv_lock);
	/*init i2c config*/
	imgsensor.i2c_speed = imgsensor_info.i2c_speed;
	imgsensor.addr_type = imgsensor_info.addr_type;
	spin_unlock(&imgsensor_drv_lock);


	/*get sensorID from imagesensor_info */
	tmp_sensor_id = (imgsensor_info.sensor_id & 0xffff000) >> 12;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			reg_sensor_id = return_sensor_id();
			*sensor_id = imgsensor_convert_sensor_id(imgsensor_info.sensor_id, reg_sensor_id, vendorID_offset, 0);
			if (reg_sensor_id == tmp_sensor_id) {
				*sensor_id = imgsensor_info.sensor_id;
				LOG_INF("id reg: 0x%x, read id: 0x%x, expect id:0x%x.\n", imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Check sensor id fail, id reg: 0x%x,read id: 0x%x, expect id:0x%x.\n", imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		retry = RETRY_TIMES;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF*/
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint32 sensor_id = 0;
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	rc = get_imgsensor_id(&sensor_id);
	if( rc != ERROR_NONE ){
		LOG_ERR("probe sensor failed.\n");
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	LOG_INF("sensor probe successfully. sensor_id=0x%x.\n", sensor_id);

	/* initail sequence write in  */
	rc = sensor_init();
	if( rc != ERROR_NONE ){
		LOG_ERR("init failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	/* write registers from sram */
	if (g_otp_identify_flag) {
		gc5025_gcore_update_otp();
	}

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}	/*  open  */



/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/*No Need to implement this function*/
	return ERROR_NONE;
}	/*  close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*	*sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_preview_setting();
	if( rc != ERROR_NONE ){
		LOG_ERR("preview setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}	/*  preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	//LOG_INF("capture30fps: use cap30FPS's setting: %d fps!\n",imgsensor.current_fps/10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	rc = set_capture_setting(imgsensor.current_fps);
	if( rc != ERROR_NONE ){
		LOG_ERR("capture setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}	/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;

	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_normal_video_setting(imgsensor.current_fps);
	if( rc != ERROR_NONE ){
		LOG_ERR("normal video setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}	/*  normal_video   */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	if(sensor_resolution != NULL)
	{
		sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
		sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

		sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
		sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

		sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
		sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	}
	return ERROR_NONE;
}	/*  get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if( !sensor_info || !sensor_config_data ){
		LOG_ERR("Fatal: NULL ptr. sensor_info:%pK, sensor_config_data:%pK.\n",
			sensor_info, sensor_config_data);
		return ERROR_NONE;
	}

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;		/* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	// 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1; /* not use */
	sensor_info->PDAF_Support = 0;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;

	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}	/*  get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;

	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		rc = preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		rc = capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		rc = normal_video(image_window, sensor_config_data);
		break;
	default:
		LOG_ERR("Error ScenarioId setting. scenario_id:%d.\n", scenario_id);
		rc = preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return rc;
}/*control()*/



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0){
		// Dynamic frame rate
		return ERROR_NONE;
	}
	spin_lock(&imgsensor_drv_lock);
	/* fps set to 296 when frame is 300 and auto-flicker enaled */
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE)){
		imgsensor.current_fps = 296;
	/* fps set to 146 when frame is 150 and auto-flicker enaled */
	}else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE)){
		imgsensor.current_fps = 146;
	}else{
		imgsensor.current_fps = framerate;
	}
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable){ //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	}else{ //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if(framerate == 0 || imgsensor_info.pre.linelength == 0){
			return ERROR_NONE;
		}
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter){
			set_dummy();
		}
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if(framerate == 0 || imgsensor_info.normal_video.linelength == 0){
			return ERROR_NONE;
		}
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if(imgsensor.frame_length > imgsensor.shutter){
			set_dummy();
		}
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if(framerate == 0 || imgsensor_info.cap.linelength == 0){
			return ERROR_NONE;
		}
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if(imgsensor.frame_length > imgsensor.shutter){
			set_dummy();
		}
		break;
	default:	//coding with  preview scenario by default
		if(framerate == 0 || imgsensor_info.pre.linelength == 0){
			return ERROR_NONE;
		}
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if(imgsensor.frame_length > imgsensor.shutter){
			set_dummy();
		}
		LOG_ERR("error scenario_id = %d, we use preview scenario.\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	default:
		break;
	}

    return ERROR_NONE;
}

static void set_register(kal_uint32 reg_addr, kal_uint32 reg_data)
{
	kal_int32 rc = ERROR_NONE;
	rc = imgsensor_sensor_i2c_write(&imgsensor, (kal_uint16)reg_addr,
		(kal_uint16)reg_data, IMGSENSOR_I2C_WORD_DATA);
	if( rc < 0 ){
		LOG_ERR("failed. reg_addr:0x%x, reg_data: 0x%x.\n",
			reg_addr, reg_data);
	}
	return;
}

static void get_register(kal_uint32 reg_addr, kal_uint32 *reg_data)
{
	kal_int32 rc = 0;
	kal_uint16 temp_data = 0;
	rc = imgsensor_sensor_i2c_read(&imgsensor, reg_addr,
		&temp_data, IMGSENSOR_I2C_WORD_DATA);
	if( rc < 0 ){
		LOG_ERR("failed. reg_addr:0x%x, reg_data: 0x%x.\n",
			reg_addr, temp_data);
	}
	*reg_data = temp_data;
	LOG_INF("reg_addr:0x%x, reg_data: 0x%x.\n",
		reg_addr, *reg_data);
	return;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	kal_int32 rc = 0;
	LOG_INF("Enter.enable:%d\n", enable);
	if (enable){
		rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.streamon_setting);
	} else {
		rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.streamoff_setting);
	}
	if ( rc < 0 ){
		LOG_ERR("Failed enable:%d.\n", enable);
		return ERROR_SENSOR_POWER_ON_FAIL;
	}
	LOG_INF("Exit.enable:%d\n", enable);

	return ERROR_NONE;
}

static kal_uint32 feature_control_gc5025_king(MSDK_SENSOR_FEATURE_ENUM feature_id,
						UINT8 *feature_para,UINT32 *feature_para_len)
{
	kal_uint32 rc = ERROR_NONE;
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = NULL;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	if(!feature_para || !feature_para_len){
		LOG_ERR("Fatal null ptr. feature_para:%pK,feature_para_len:%pK.\n",
			feature_para,feature_para_len);
		return ERROR_NONE;
	}

	LOG_INF("feature_id = %d.\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len=4;	/* return 4 byte data */
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len=4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		set_register(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		get_register(sensor_reg_data->RegAddr, &sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
		*feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len=4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		if (get_imgsensor_id(feature_return_para_32) == ERROR_NONE) {
			/* Don't Remove */
			gc5025_gcore_identify_otp();
		}
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len=4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				/*imgsensor_winsize_info arry 1 is capture setting*/
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				/*imgsensor_winsize_info arry 2 is video setting*/
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				/*imgsensor_winsize_info arry 0 is preview setting*/
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
		}
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = 0;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		rc = streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0){
			set_shutter((UINT16)*feature_data);
		}
		rc = streaming_control(KAL_TRUE);
		break;
	/******************** PDAF START >>> *********/
	case SENSOR_FEATURE_GET_VC_INFO:
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		break;
	case SENSOR_FEATURE_SET_PDAF:
		break;
	/******************** PDAF END <<< *********/
	case SENSOR_HUAWEI_FEATURE_DUMP_REG:
		sensor_dump_reg();
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	case SENSOR_FEATURE_SET_HDR:
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	case SENSOR_FEATURE_SET_FLASHLIGHT:
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	default:
		LOG_ERR("Not support the feature_id:%d\n",feature_id);
		break;
	}

	return ERROR_NONE;
}/*feature_control_gc5025_king()*/


static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_gc5025_king,
	control,
	close
};


UINT32 GC5025_KING_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL){
		*pfFunc=&sensor_func;
	}
	return ERROR_NONE;
}	/*  GC5025_KING_SensorInit  */
