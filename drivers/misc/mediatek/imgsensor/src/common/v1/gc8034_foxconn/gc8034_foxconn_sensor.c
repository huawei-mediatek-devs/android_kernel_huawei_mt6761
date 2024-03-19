/*
 *  Copyright (C) 2018 Huawei Inc.
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
#include "gc8034_foxconn_sensor.h"

#define RETRY_TIMES	2
#define DD_WIDTH	3284

#define DD_FLAG 0x0b
#define DD_TOTAL_NUMBER 0x0c
#define DD_ERROR_NUMBER 0x0d
#define DD_ROM_START    0x0e
#define REG_INFO_FLAG   0x4e
#define REG_INFO_PAGE   0x4f
#define REG_INFO_ADDR   0X50
#define PRSEL_INFO_FLAG 0x68

/****************************Modify Following Strings for Debug****************************/
#define PFX "[gc8034_foxconn]"
#define DEBUG_GC8034_FOXCONN 0
#define LOG_DBG(fmt, args...)    if (DEBUG_GC8034_FOXCONN) pr_debug(PFX "%s %d " fmt, __func__,__LINE__, ##args)
#define LOG_INF(fmt, args...)    pr_info(PFX "%s %d " fmt, __func__,__LINE__, ##args)
#define LOG_ERR(fmt, args...)    pr_err(PFX "%s %d " fmt, __func__,__LINE__, ##args)

/****************************   Modify end    *******************************************/

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static kal_uint32 Dgain_ratio = 1;
static kal_uint8  BorF = 1;
static struct gc8034_otp_t gc8034_otp_info;
static bool gc8034_otp_identify_flag = FALSE;

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

static void set_dummy(void)
{
	kal_uint32  vb = 16;
	kal_uint32  basic_line = 2484;
	kal_int32 rc = 0;

	LOG_DBG("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	vb = imgsensor.frame_length - basic_line;
	vb = vb < 16 ? 16 : vb;
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
	LOG_DBG("ENTER\n");

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
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
	return;
}   /*  set_max_framerate  */

/*************************************************************************
* FUNCTION
*   set_shutter
*
* DESCRIPTION
*   This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*   iShutter : exposured lines
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0, cal_shutter = 0;
	/*kal_uint32 frame_length = 0;*/
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_DBG("Enter set_shutter!\n");

	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 298 && realtime_fps <= 305)
			set_max_framerate(298, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else
		set_max_framerate(realtime_fps, 0);


	cal_shutter = shutter >> 1;
	cal_shutter = cal_shutter << 1;
	Dgain_ratio = 256 * shutter / cal_shutter;

	/* Update Shutter */
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
			0x00, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_BUF_EXP_IN_REG_H,
			(cal_shutter >> 8) & 0x7f, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_BUF_EXP_IN_REG_L,
			cal_shutter & 0xff, IMGSENSOR_I2C_BYTE_DATA);
	LOG_DBG("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

static void gain2reg(kal_uint16 gain)
{
	kal_int16 gain_index = 0;
	UINT16 temp_gain = 0;
	/*kal_uint8 i = 0;*/
	kal_uint16 gain_level[MAX_AG_INDEX] = {
		0x0040, /* 1.000*/
		0x0058, /* 1.375*/
		0x007d, /* 1.950*/
		0x00ad, /* 2.700*/
		0x00f3, /* 3.800*/
		0x0159, /* 5.400*/
		0x01ea, /* 7.660*/
		0x02ac, /*10.688*/
		0x03c2, /*15.030*/
	};
	kal_uint8 agc_register[2 * MAX_AG_INDEX][AGC_REG_NUM] = {
	  /*{ 0xfe, 0x20, 0x33, 0xfe, 0xdf, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xfe },*/
		/* binning */
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50, 0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50, 0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x4e, 0x84, 0x01, 0x0c, 0x2e, 0x2d, 0x15, 0x19, 0x47, 0x70, 0x9f, 0xd8, 0x00 },
		{ 0x00, 0x51, 0x80, 0x01, 0x07, 0x28, 0x32, 0x22, 0x20, 0x49, 0x70, 0x91, 0xd9, 0x00 },
		{ 0x00, 0x4d, 0x83, 0x01, 0x0f, 0x3b, 0x3b, 0x1c, 0x1f, 0x47, 0x6f, 0x9b, 0xd3, 0x00 },
		{ 0x00, 0x50, 0x83, 0x01, 0x08, 0x35, 0x46, 0x1e, 0x22, 0x4c, 0x70, 0x9a, 0xd2, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		/* fullsize */
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50, 0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50, 0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x4e, 0x84, 0x01, 0x0c, 0x2e, 0x2d, 0x15, 0x19, 0x47, 0x70, 0x9f, 0xd8, 0x00 },
		{ 0x00, 0x51, 0x80, 0x01, 0x07, 0x28, 0x32, 0x22, 0x20, 0x49, 0x70, 0x91, 0xd9, 0x00 },
		{ 0x00, 0x4d, 0x83, 0x01, 0x0f, 0x3b, 0x3b, 0x1c, 0x1f, 0x47, 0x6f, 0x9b, 0xd3, 0x00 },
		{ 0x00, 0x50, 0x83, 0x01, 0x08, 0x35, 0x46, 0x1e, 0x22, 0x4c, 0x70, 0x9a, 0xd2, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c, 0x67, 0x8d, 0xc0, 0x00 }
	};

	for (gain_index = MEAG_INDEX - 1; gain_index >= 0; gain_index--)
		if (gain >= gain_level[gain_index]) {
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xb6,
				gain_index, IMGSENSOR_I2C_BYTE_DATA);
			temp_gain = 256 * gain / gain_level[gain_index];
			temp_gain = temp_gain * Dgain_ratio / 256;
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xb1,
				temp_gain >> 8, IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xb2,
				temp_gain & 0xff, IMGSENSOR_I2C_BYTE_DATA);

			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xfe,
				agc_register[BorF * MAX_AG_INDEX + gain_index][0], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0x20,
				agc_register[BorF * MAX_AG_INDEX + gain_index][1], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0x33,
				agc_register[BorF * MAX_AG_INDEX + gain_index][2], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xfe,
				agc_register[BorF * MAX_AG_INDEX + gain_index][3], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xdf,
				agc_register[BorF * MAX_AG_INDEX + gain_index][4], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xe7,
				agc_register[BorF * MAX_AG_INDEX + gain_index][5], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xe8,
				agc_register[BorF * MAX_AG_INDEX + gain_index][6], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xe9,
				agc_register[BorF * MAX_AG_INDEX + gain_index][7], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xea,
				agc_register[BorF * MAX_AG_INDEX + gain_index][8], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xeb,
				agc_register[BorF * MAX_AG_INDEX + gain_index][9], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xec,
				agc_register[BorF * MAX_AG_INDEX + gain_index][10], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xed,
				agc_register[BorF * MAX_AG_INDEX + gain_index][11], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xee,
				agc_register[BorF * MAX_AG_INDEX + gain_index][12], IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, 0xfe,
				agc_register[BorF * MAX_AG_INDEX + gain_index][13], IMGSENSOR_I2C_BYTE_DATA);
			break;
		}
}

/*************************************************************************
* FUNCTION
*   set_gain
*
* DESCRIPTION
*   This function is to set global gain to sensor.
*
* PARAMETERS
*   iGain : sensor global gain(base: 0x40)
*
* RETURNS
*   the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 real_gain = 0;

	LOG_DBG("Enter set_gain!\n");
	real_gain = (gain < SENSOR_BASE_GAIN) ? SENSOR_BASE_GAIN : gain;
	real_gain = (real_gain > SENSOR_MAX_GAIN) ? SENSOR_MAX_GAIN : real_gain;

	gain2reg(real_gain);

	return gain;
}

static kal_uint32 sensor_init(void)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER.\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.init_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}   /*  sensor_init  */

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.pre_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}   /*  preview_setting  */


static kal_uint32 set_capture_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.cap_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.normal_video_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_DBG("EXIT\n");

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
	if(sensor_id == GC8034_FOXCONN_SENSOR_ID )
    {
		return sensor_id;
    }
    return 0;
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

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = RETRY_TIMES; /* retry 2 time */

	spin_lock(&imgsensor_drv_lock);
	/*init i2c config*/
	imgsensor.i2c_speed = imgsensor_info.i2c_speed;
	imgsensor.addr_type = imgsensor_info.addr_type;
	spin_unlock(&imgsensor_drv_lock);

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("id reg: 0x%x, read id: 0x%x, expect id:0x%x.\n",
					imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Check sensor id fail, id reg: 0x%x,read id: 0x%x, expect id:0x%x.\n",
				imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
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

static void gc8034_gcore_enable_otp(kal_uint8 state)
{
	kal_uint16 otp_clk = 0;
	kal_uint16 otp_en = 0;

	IMGSENSOR_SENSOR_I2C_READ(&imgsensor,0xf2,&otp_clk, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_READ(&imgsensor,0xf4,&otp_en, IMGSENSOR_I2C_BYTE_DATA);
	if (state) {
		otp_clk = otp_clk | 0x01;
		otp_en = otp_en | 0x08;
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf2, otp_clk,IMGSENSOR_I2C_BYTE_DATA);/* [0]OTP_clk_en*/
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf4, otp_en,IMGSENSOR_I2C_BYTE_DATA);/* [3]OTP enable*/
		LOG_INF("GC8034_OTP: Enable OTP!\n");
	} else {
		otp_en = otp_en & 0xf7;
		otp_clk = otp_clk & 0xfe;
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf4, otp_en,IMGSENSOR_I2C_BYTE_DATA);/* [3]OTP enable*/
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf2, otp_clk,IMGSENSOR_I2C_BYTE_DATA);/* [0]OTP_clk_en*/
		LOG_INF("GC8034_OTP: Disable OTP!\n");
	}

	return;
}

static kal_uint8 gc8034_read_otp(kal_uint16 page, kal_uint16 addr)
{
	kal_uint16 regData = 0;
	kal_uint16 tmpData = 0;
	LOG_DBG("ENTER.\n");

	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xfe, 0x00, IMGSENSOR_I2C_BYTE_DATA);//page select

	tmpData = ((page << 2) & 0x3c) + ((addr >> 5) & 0x03); //OTP page select [5:2], OTP address select high bit[9:8]
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xd4, tmpData, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xd5, ((addr << 3) & 0xff), IMGSENSOR_I2C_BYTE_DATA);//OTP address select low bit[7:0]
	mdelay(1);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf3, 0x20, IMGSENSOR_I2C_BYTE_DATA);//OTP read
	IMGSENSOR_SENSOR_I2C_READ(&imgsensor,0xd7, &regData, IMGSENSOR_I2C_BYTE_DATA);//OTP value

	return regData;
}

static void gc8034_read_otp_kgroup(kal_uint16 page, kal_uint16 addr, kal_uint16 *buff, kal_uint16 size)
{
	kal_uint16 i;
	kal_uint16 tmpData = 0;
	kal_uint16 regf4 = 0;
	IMGSENSOR_SENSOR_I2C_READ(&imgsensor,0xf4,&regf4, IMGSENSOR_I2C_BYTE_DATA);

	tmpData = ((page << 2) & 0x3c) + ((addr >> 5) & 0x03);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xd4, tmpData, IMGSENSOR_I2C_BYTE_DATA);
	tmpData = (addr << 3) & 0xff;
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xd5, tmpData, IMGSENSOR_I2C_BYTE_DATA);
	mdelay(1);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf3, 0x20, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf4, (regf4 |0x02), IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf3, 0x80, IMGSENSOR_I2C_BYTE_DATA);
	for (i = 0; i < size; i++) {
		if (((addr + i) % 0x80) == 0) {
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf3, 0x00, IMGSENSOR_I2C_BYTE_DATA);
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf4, (regf4 & 0xfd), IMGSENSOR_I2C_BYTE_DATA);
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xd4, ((++page << 2) & 0x3c), IMGSENSOR_I2C_BYTE_DATA);
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xd5, 0x00, IMGSENSOR_I2C_BYTE_DATA);
			mdelay(1);
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf3, 0x20, IMGSENSOR_I2C_BYTE_DATA);
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf4, (regf4 |0x02), IMGSENSOR_I2C_BYTE_DATA);
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf3, 0x80, IMGSENSOR_I2C_BYTE_DATA);
		}
		IMGSENSOR_SENSOR_I2C_READ(&imgsensor,0xd7, buff + i, IMGSENSOR_I2C_BYTE_DATA);
	}
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf3, 0x00, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xf4, (regf4 & 0xfd), IMGSENSOR_I2C_BYTE_DATA);

	return;
}

static void gc8034_gcore_set_otp_ddparam(void)
{
	kal_uint8 i;
	kal_uint8 total_number = 0;
	kal_uint8 cnt = 0;
	kal_uint16 ddtempbuff[4 * MAX_DD_NUM] = { 0 };

	total_number = gc8034_read_otp(OTP_PAGE0, DD_TOTAL_NUMBER) + gc8034_read_otp(OTP_PAGE0, DD_ERROR_NUMBER);
	if (total_number > MAX_DD_NUM) {
		LOG_INF("GC8034_OTP : total_number(%d) exceeds MAX_DD_NUM.\n", total_number);
		total_number = MAX_DD_NUM;
	}
	gc8034_read_otp_kgroup(OTP_PAGE0, DD_ROM_START, ddtempbuff, 4 * total_number);

	for (i = 0; i < total_number; i++) {
		if ((ddtempbuff[4 * i + 3] & 0x80) == 0x80) {
			if ((ddtempbuff[4 * i + 3] & 0x03) == 0x03) {
				gc8034_otp_info.dd_param[cnt].x = ((ddtempbuff[4 * i + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
				gc8034_otp_info.dd_param[cnt].y = (ddtempbuff[4 * i + 2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4);
				gc8034_otp_info.dd_param[cnt++].t = 2;
				gc8034_otp_info.dd_param[cnt].x = ((ddtempbuff[4 * i + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
				gc8034_otp_info.dd_param[cnt].y = (ddtempbuff[4 * i + 2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4) + 1;
				gc8034_otp_info.dd_param[cnt++].t = 2;
			} else {
				gc8034_otp_info.dd_param[cnt].x = ((ddtempbuff[4 * i + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
				gc8034_otp_info.dd_param[cnt].y = (ddtempbuff[4 * i + 2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4);
				gc8034_otp_info.dd_param[cnt++].t = ddtempbuff[4 * i + 3] & 0x03;
			}
		}
	}
	gc8034_otp_info.dd_cnt = cnt;
	LOG_INF("GC8034_OTP : total_number = %d\n", gc8034_otp_info.dd_cnt);

	return;
}

static void gc8034_gcore_set_otp_reginfo(void)
{
	kal_uint16 i, j;
	kal_uint8 temp = 0;
	gc8034_otp_info.reg_num = 0;

	LOG_INF("ENTER.\n");
	gc8034_otp_info.reg_flag = gc8034_read_otp(OTP_PAGE2, REG_INFO_FLAG);

	if ((gc8034_otp_info.reg_flag & 0x03) != 0x01) {
		LOG_INF("GC8034_OTP : reg_flag = %d\n", gc8034_otp_info.reg_flag);
		return;
	}

	for (i = 0; i < REGS_GROUP; i++) {
		temp = gc8034_read_otp(OTP_PAGE2, REG_INFO_PAGE + REGS_GROUP * i);
		for (j = 0; j < REGS_NUM; j++) {
			if (((temp >> (4 * j + 3)) & 0x01) == 0x01) {
				gc8034_otp_info.reg_page[gc8034_otp_info.reg_num] = (temp >> (4 * j)) & 0x03;
				gc8034_otp_info.reg_addr[gc8034_otp_info.reg_num] =
					gc8034_read_otp(OTP_PAGE2, REG_INFO_ADDR + REGS_GROUP * i + REGS_NUM * j);
				gc8034_otp_info.reg_value[gc8034_otp_info.reg_num] =
					gc8034_read_otp(OTP_PAGE2, REG_INFO_ADDR + REGS_GROUP * i + REGS_NUM * j + 1);
				gc8034_otp_info.reg_num++;
			}
		}
	}

	return;
}

static void gc8034_gcore_read_otp_info(void)
{
	kal_uint8  flagdd = 0;

	memset(&gc8034_otp_info, 0, sizeof(struct gc8034_otp_t));
	/* Static Defective Pixel */
	flagdd = gc8034_read_otp(OTP_PAGE0, DD_FLAG);
	LOG_INF("GC8034_OTP_DD : flag_dd = 0x%x\n", flagdd);

	switch (flagdd & 0x03) {
		case 0x00:
			LOG_INF("GC8034_OTP_DD is Empty !!\n");
			gc8034_otp_info.dd_flag = 0x00;
			break;
		case 0x01:
			LOG_INF("GC8034_OTP_DD is Valid!!\n");
			gc8034_otp_info.dd_flag = 0x01;
			gc8034_gcore_set_otp_ddparam();
			break;
		default:
			LOG_INF("GC8034_OTP_DD is Invalid(%x) !!\n",flagdd & 0x03);
			gc8034_otp_info.dd_flag = 0x02;
			break;
	}

	/* chip regs */
	gc8034_gcore_set_otp_reginfo();
	/*get product level to check prsel*/
	gc8034_otp_info.product_level = gc8034_read_otp(OTP_PAGE2, PRSEL_INFO_FLAG) & 0x07;

	return;
}

static void gc8034_gcore_update_dd(void)
{
	kal_uint16 i, j;
	kal_uint16 temp_val0,temp_val1,temp_val2;
	struct gc8034_dd_t dd_temp = {0, 0, 0};

	if (gc8034_otp_info.dd_flag != 0x01) {
		LOG_INF("gc8034_otp_info.dd_flag = 0x%x.\n", gc8034_otp_info.dd_flag);
		return;
	}
	if (gc8034_otp_info.dd_cnt > IMGSENSOR_ARRAY_SIZE(gc8034_otp_info.dd_param)) {
		LOG_INF("gc8034_otp_info.dd_cnt = %d.\n", gc8034_otp_info.dd_cnt);
		return;
	}

	LOG_INF("GC8034_OTP_AUTO_DD start !\n");
	for(i = 0; i < gc8034_otp_info.dd_cnt - 1; i++) {
		for(j = i + 1; j < gc8034_otp_info.dd_cnt; j++) {
			if(gc8034_otp_info.dd_param[i].y * DD_WIDTH + gc8034_otp_info.dd_param[i].x >
				gc8034_otp_info.dd_param[j].y * DD_WIDTH + gc8034_otp_info.dd_param[j].x) {
				dd_temp = gc8034_otp_info.dd_param[i];
				gc8034_otp_info.dd_param[i] = gc8034_otp_info.dd_param[j];
				gc8034_otp_info.dd_param[j] = dd_temp;
			}
		}
	}
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xfe, 0x01, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xbe, 0x00, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xa9, 0x01, IMGSENSOR_I2C_BYTE_DATA);

	for (i = 0; i < gc8034_otp_info.dd_cnt; i++) {
		temp_val0 = gc8034_otp_info.dd_param[i].x & 0x00ff;
		temp_val1 = ((gc8034_otp_info.dd_param[i].y & 0x000f) << 4) + ((gc8034_otp_info.dd_param[i].x & 0x0f00)>>8);
		temp_val2 = (gc8034_otp_info.dd_param[i].y & 0x0ff0) >> 4;
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xaa, i, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xac, temp_val0, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xac, temp_val1, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xac, temp_val2, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xac, gc8034_otp_info.dd_param[i].t, IMGSENSOR_I2C_BYTE_DATA);

		LOG_INF("GC8034_OTP_GC val0 = 0x%x , val1 = 0x%x , val2 = 0x%x \n",temp_val0,temp_val1,temp_val2);
		LOG_INF("GC8034_OTP_GC x = %d , y = %d \n",((temp_val1&0x0f)<<8) + temp_val0,(temp_val2<<4) + ((temp_val1&0xf0)>>4));
	}

	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xbe, 0x01, IMGSENSOR_I2C_BYTE_DATA);
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xfe, 0x00, IMGSENSOR_I2C_BYTE_DATA);

	return;
}

static void gc8034_gcore_check_prsel(void)
{
	LOG_INF("product_level = 0x%x\n", gc8034_otp_info.product_level);

	if ((gc8034_otp_info.product_level == 0x00) || (gc8034_otp_info.product_level == 0x01)) {
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xfe, 0x00, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xd2, 0xcb, IMGSENSOR_I2C_BYTE_DATA);
	} else {
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xfe, 0x00, IMGSENSOR_I2C_BYTE_DATA);
		IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor,0xd2, 0xc3, IMGSENSOR_I2C_BYTE_DATA);
	}

	return;
}

static void gc8034_gcore_update_chipversion(void)
{
	kal_uint8 i;
	LOG_INF("GC8034_OTP_UPDATE_CHIPVERSION:reg_num = %d\n", gc8034_otp_info.reg_num);

	if ((gc8034_otp_info.reg_flag & 0x03) == 0x01) {
		for (i = 0; i < gc8034_otp_info.reg_num; i++) {
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, 0xfe, gc8034_otp_info.reg_page[i], IMGSENSOR_I2C_BYTE_DATA);
			IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, gc8034_otp_info.reg_addr[i], gc8034_otp_info.reg_value[i], IMGSENSOR_I2C_BYTE_DATA);
			LOG_INF("GC8034_OTP_UPDATE_CHIP_VERSION: P%d:0x%x -> 0x%x\n",
			gc8034_otp_info.reg_page[i], gc8034_otp_info.reg_addr[i], gc8034_otp_info.reg_value[i]);
		}
	}

	return;
}

static void gc8034_gcore_update_otp(void)
{
	LOG_INF("Enter.");
	gc8034_gcore_update_dd();
	gc8034_gcore_check_prsel();
	gc8034_gcore_update_chipversion();

	return;
}

static kal_uint32 gc8034_gcore_initial_otp(void)
{
	kal_int32 rc = 0;

	rc = imgsensor_sensor_write_setting(&imgsensor, &otp_init_setting_array);
	if( rc != 0 ){
		LOG_ERR("GC8034_OTP: OTP initial failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("GC8034_OTP: OTP initial succeed!\n");
	return ERROR_NONE;
}

static void gc8034_gcore_identify_otp(void)
{
	LOG_INF("Enter.");
	//identify only once
	if (!gc8034_otp_identify_flag) {
		gc8034_otp_identify_flag = TRUE;
		if (gc8034_gcore_initial_otp() == ERROR_NONE) {
			gc8034_gcore_enable_otp(OTP_OPEN);
			gc8034_gcore_read_otp_info();
			gc8034_gcore_enable_otp(OTP_CLOSE);
		}
	}
	return;
}

/*************************************************************************
* FUNCTION
*   open
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
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
	LOG_DBG("sensor probe successfully. sensor_id=0x%x.\n", sensor_id);

	/* initail sequence write in  */
	rc = sensor_init();
	if( rc != ERROR_NONE ){
		LOG_ERR("init failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	gc8034_gcore_update_otp();
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
}   /*  open  */



/*************************************************************************
* FUNCTION
*   close
*
* DESCRIPTION
*
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("Enter.\n");
	/*No Need to implement this function*/

	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
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
}   /*  preview   */

/*************************************************************************
* FUNCTION
*   capture
*
* DESCRIPTION
*   This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*   None
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
}   /* capture() */

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
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_normal_video_setting(imgsensor.current_fps);
	if( rc != ERROR_NONE ){
		LOG_ERR("normal video setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}   /*  normal_video   */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	if(sensor_resolution !=NULL){
		//LOG_INF("E\n");
		sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
		sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

		sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
		sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

		sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
		sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	}
	return ERROR_NONE;
}	 /*    get_resolution	 */


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

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;      /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
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
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;   // 0 is default 1x
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
}   /*  get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;

	LOG_DBG("scenario_id = %d\n", scenario_id);
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
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	/* fps set to 298 when frame is 300 and auto-flicker enaled */
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 298;
	/* fps set to 146 when frame is 150 and auto-flicker enaled */
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, UINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
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
	default:  //coding with  preview scenario by default
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

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, UINT32 *framerate)
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

static kal_uint32 feature_control_gc8034_foxconn(MSDK_SENSOR_FEATURE_ENUM feature_id,
							UINT8 *feature_para,UINT32 *feature_para_len)
{
	kal_uint32 rc = ERROR_NONE;
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = NULL;

	if(!feature_para || !feature_para_len){
		LOG_ERR("Fatal null ptr. feature_para:%pK,feature_para_len:%pK.\n",
			feature_para,feature_para_len);
		return ERROR_NONE;
	}

	LOG_DBG("feature_id = %d.\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len=4;   /* return 4 byte data */
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len=4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		rc = get_imgsensor_id(feature_return_para_32);
		if (rc == ERROR_NONE) {
		    gc8034_gcore_identify_otp();
		}
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			(UINT32)*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(UINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
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
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		rc = streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0){
			set_shutter((UINT16)*feature_data);
		}
		rc = streaming_control(KAL_TRUE);
		break;
	case SENSOR_HUAWEI_FEATURE_DUMP_REG:
		sensor_dump_reg();
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if(imgsensor_info.cap.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.cap.pclk /
					(imgsensor_info.cap.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.cap.grabwindow_width;
			}
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(imgsensor_info.normal_video.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.normal_video.pclk /
					(imgsensor_info.normal_video.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.normal_video.grabwindow_width;
			}
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			if(imgsensor_info.pre.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.pre.pclk /
					(imgsensor_info.pre.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.pre.grabwindow_width;
			}
			break;
		}
		break;
	default:
		LOG_INF("Not support the feature_id:%d\n",feature_id);
		break;
	}

	return ERROR_NONE;
}/*feature_control()*/


static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_gc8034_foxconn,
	control,
	close
};


UINT32 GC8034_FOXCONN_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}   /*  GC8034_FOXCONN_SensorInit  */
